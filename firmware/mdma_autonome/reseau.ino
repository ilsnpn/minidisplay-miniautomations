// Tâche réseau : Wi-Fi et relevés. Elle tourne à côté de loop() pour que l'écran
// et le bouton restent réactifs pendant les connexions HTTPS (1 à 3 s chacune).
// Tout ce qu'elle écrit dans l'état partagé passe par Guard.
//
// Sources (mêmes adresses et mêmes garde-fous que le pont PC de la V1) :
//   quota Claude      GET api.anthropic.com/api/oauth/usage, toutes les 5 min
//   état des services GET status.claude.com/api/v2/status.json, toutes les 5 min
//   ElevenLabs        GET api.elevenlabs.io/v1/user/subscription, toutes les 10 min
//   PALais SIAM       Supabase du site : nouvelles commandes toutes les 20 s, résumé chaque minute
//   bourse            GET query1.finance.yahoo.com/v8/finance/chart, chaque minute
//   heure             NTP, fuseau TZ_INFO

#define WIFI_TRY_MS 15000UL     // délai pour se connecter à un réseau
#define WIFI_FORCED_MS 60000UL  // réseau imposé introuvable : retour en automatique
#define STATUS_MS (5 * 60000UL)
#define QUOTA_MS (5 * 60000UL)  // un appel toutes les 5 min au plus, comme le widget
#define QUOTA_MAX_MIN 60        // attente maximale après des 429 à répétition
#define ELEVEN_MS (10 * 60000UL)
#define ORDERS_MS 20000UL
#define SUMMARY_MS 60000UL
#define MARKET_MS 60000UL
#define MARKET_POINTS 60

#define QUOTA_URL "https://api.anthropic.com/api/oauth/usage"
#define STATUS_URL "https://status.claude.com/api/v2/status.json"
#define ELEVEN_URL "https://api.elevenlabs.io/v1/user/subscription"
#define CLAUDE_CLIENT_ID "9d1c250a-e61b-44d9-88ed-5944d1962f5e"  // client OAuth public de Claude Code
#define CLAUDE_UA "claude-code/1.0.0"  // sans lui, l'endpoint rate-limite beaucoup plus vite
#define MDMA_UA "MDMA/2.0"

static Preferences netPrefs;  // espace « net » : jetons Claude et réseau imposé (tâche réseau seule)
static volatile bool timeSynced = false;
static uint32_t nextStatus = 0, nextQuota = 0, nextEleven = 0, nextOrders = 0, nextSummary = 0, nextMarket = 0;

// ---------------- Outils ----------------
static bool due(uint32_t at) {
  return (int32_t)(millis() - at) >= 0;
}

static void markSource(uint8_t id, bool ok) {
  Guard g;
  srcs[id].fail = !ok;
  if (ok) srcs[id].okAt = nowLocal();
  dirty = true;
}

// Les polices n'ont que l'ASCII : accents retirés (é -> e), autres caractères ignorés
static void toAscii(char *out, size_t n, const char *in) {
  static const char LATIN1[] = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";  // U+00C0..U+00FF
  size_t j = 0;
  for (const uint8_t *p = (const uint8_t *)(in ? in : ""); *p && j + 1 < n; p++) {
    if (*p >= 32 && *p < 127) out[j++] = *p;
    else if (*p == 0xC3 && p[1] >= 0x80 && p[1] <= 0xBF) out[j++] = LATIN1[*++p - 0x80];
    else if (*p == 0xE2 && p[1] == 0x80 && (p[2] == 0x98 || p[2] == 0x99)) {  // apostrophes typographiques
      out[j++] = '\'';
      p += 2;
    } else if (*p >= 0xC0) {
      while ((p[1] & 0xC0) == 0x80) p++;
    }
  }
  out[j] = 0;
}

// « 2026-10-19T23:39:59.579417+00:00 » -> secondes UTC, 0 si illisible
static time_t parseIso(const char *s) {
  int y, mo, d, h = 0, mi = 0, se = 0;
  if (!s || sscanf(s, "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &se) < 3) return 0;
  time_t t = (time_t)daysFromCivil(y, mo, d) * 86400 + h * 3600 + mi * 60 + se;
  const char *z = strlen(s) > 19 ? strpbrk(s + 19, "+-") : NULL;  // décalage horaire éventuel
  if (z) {
    int oh = 0, om = 0;
    sscanf(z + 1, "%d:%d", &oh, &om);
    t -= (*z == '+' ? 1 : -1) * (oh * 3600 + om * 60);
  }
  return t;
}

// Heure ISO -> heure locale arrondie à la minute (l'API renvoie 17:59:59.9 pour 18:00)
static uint32_t isoLocal(const char *s) {
  const time_t t = parseIso(s);
  return t ? localEpoch((t + 30) / 60 * 60) : 0;
}

// 81501.52 -> « 81 502 » ; 13.21 -> « 13.21 »
static void fmtPrice(char *out, size_t n, double v) {
  if (v >= 1000) fmtThousands(out, n, (int32_t)lround(v));
  else snprintf(out, n, "%.2f", v);
}

// Requête HTTPS (POST si body), réponse JSON lue en flux, filtrée puis rangée dans
// doc. Renvoie le code HTTP, ou une valeur négative en cas d'échec réseau.
// headers : paires nom/valeur terminées par NULL. range : en-tête Content-Range.
static int httpJson(const char *url, const char *body, const char *const *headers, const char *ua,
                    JsonDocument &doc, const JsonDocument *filter, String *range) {
  NetworkClientSecure client;
  client.useBuiltinCACertBundle();  // certificats vérifiés avec les autorités usuelles
  HTTPClient http;
  http.useHTTP10(true);  // pas de réponse « chunked » : elle se lit en flux
  http.setConnectTimeout(10000);
  http.setTimeout(15000);
  if (!http.begin(client, url)) return -1;
  http.setUserAgent(ua);
  http.addHeader("Accept", "application/json");
  if (body) http.addHeader("Content-Type", "application/json");
  for (; headers && headers[0]; headers += 2) http.addHeader(headers[0], headers[1]);
  static const char *KEEP[] = { "Content-Range" };
  if (range) http.collectHeaders(KEEP, 1);
  int code = body ? http.POST((uint8_t *)body, strlen(body)) : http.GET();
  if (code > 0) {
    if (range) *range = http.header("Content-Range");
    // Corps lu en entier d'abord : lu en flux, la réponse arrivait tronquée (IncompleteInput)
    const String raw = http.getString();
    const DeserializationError e = filter ? deserializeJson(doc, raw, DeserializationOption::Filter(*filter))
                                          : deserializeJson(doc, raw);
    if (e && code == 200) {  // réponse illisible
      Serial.printf("JSON %s : %s (%u octets)\n", url, e.c_str(), raw.length());
      code = -2;
    }
  }
  http.end();
  return code;
}

// ---------------- Claude : jetons ----------------
// Connexion dédiée à la carte (voir secrets.h). Chaque renouvellement remplace
// aussi le jeton de renouvellement : le nouveau est gardé en Flash aussitôt.
static String claudeAccess, claudeRefresh;
static uint32_t claudeExpires = 0;  // heure UTC d'expiration du jeton d'accès
static bool claudeDead = false;     // jeton de renouvellement refusé : reconnexion nécessaire
static uint8_t claudeHost = 0;      // adresse de renouvellement qui a répondu en dernier

static void claudeLoad() {
  const uint32_t seed = esp_crc32_le(0, (const uint8_t *)CLAUDE_REFRESH_TOKEN, strlen(CLAUDE_REFRESH_TOKEN));
  if (netPrefs.getUInt("seed", 0) != seed) {  // nouveau jeton dans secrets.h : il remplace celui de la Flash
    netPrefs.putUInt("seed", seed);
    netPrefs.putString("rt", CLAUDE_REFRESH_TOKEN);
    netPrefs.remove("at");
    netPrefs.remove("exp");
  }
  claudeRefresh = netPrefs.getString("rt", "");
  claudeAccess = netPrefs.getString("at", "");
  claudeExpires = netPrefs.getUInt("exp", 0);
}

static bool claudeRenew() {
  if (!claudeRefresh.length()) {
    claudeDead = true;
    return false;
  }
  JsonDocument req;
  req["grant_type"] = "refresh_token";
  req["refresh_token"] = claudeRefresh;
  req["client_id"] = CLAUDE_CLIENT_ID;
  String body;
  serializeJson(req, body);
  static const char *const HOSTS[] = { "https://platform.claude.com/v1/oauth/token", "https://console.anthropic.com/v1/oauth/token" };
  for (uint8_t k = 0; k < 2; k++) {
    const uint8_t h = (claudeHost + k) % 2;
    JsonDocument doc;
    const int code = httpJson(HOSTS[h], body.c_str(), NULL, CLAUDE_UA, doc, NULL, NULL);
    const char *at = doc["access_token"];
    if (code == 200 && at) {
      claudeHost = h;
      claudeAccess = at;
      if (doc["refresh_token"].is<const char *>()) claudeRefresh = doc["refresh_token"].as<const char *>();
      claudeExpires = time(NULL) + (doc["expires_in"] | 3600);
      // L'ancien jeton de renouvellement ne vaut déjà plus rien : le nouveau est écrit tout de suite
      netPrefs.putString("rt", claudeRefresh);
      netPrefs.putString("at", claudeAccess);
      netPrefs.putUInt("exp", claudeExpires);
      Serial.printf("Claude : jeton renouvele par %s\n", HOSTS[h]);
      return true;
    }
    Serial.printf("Claude : renouvellement refuse par %s (%d)\n", HOSTS[h], code);
    if (doc["error"] == "invalid_grant") {  // jeton révoqué ou déjà utilisé : l'autre adresse dira pareil
      claudeDead = true;
      return false;
    }
  }
  return false;
}

// ---------------- Quota Claude ----------------
static int quotaBackoff = 0;  // minutes d'attente après un 429

// hide : une valeur périmée par un refus n'est plus affichée (un 429 la garde)
static void quotaFailed(const char *msg, bool hide) {
  Serial.printf("Quota : %s\n", msg);
  {
    Guard g;
    snprintf(st.err, sizeof(st.err), "%s", msg);
    if (hide) st.five.used = st.week.used = -1;
  }
  markSource(SRC_CLAUDE, false);
}

static void fetchQuota() {
  char msg[72];
  nextQuota = millis() + QUOTA_MS;
  // Jeton d'accès absent ou expirant dans moins de 10 min : on le renouvelle
  if (!claudeAccess.length() || (uint32_t)time(NULL) + 600 > claudeExpires) {
    if (!claudeRenew()) {
      quotaFailed(claudeDead ? "Claude : reconnexion necessaire" : "Claude : renouvellement impossible", true);
      if (claudeDead) nextQuota = millis() + 6 * 3600000UL;  // inutile d'insister
      return;
    }
  }
  JsonDocument filter;
  filter["five_hour"]["utilization"] = true;
  filter["five_hour"]["resets_at"] = true;
  filter["seven_day"]["utilization"] = true;
  filter["seven_day"]["resets_at"] = true;
  JsonDocument doc;
  String auth = "Bearer " + claudeAccess;
  const char *hdr[] = { "Authorization", auth.c_str(), "anthropic-beta", "oauth-2025-04-20", NULL };
  int code = httpJson(QUOTA_URL, NULL, hdr, CLAUDE_UA, doc, &filter, NULL);
  if (code == 401 && claudeRenew()) {  // jeton refusé avant l'heure : un renouvellement, un nouvel essai
    auth = "Bearer " + claudeAccess;
    hdr[1] = auth.c_str();
    doc.clear();
    code = httpJson(QUOTA_URL, NULL, hdr, CLAUDE_UA, doc, &filter, NULL);
  }

  if (code == 429) {
    quotaBackoff = min(QUOTA_MAX_MIN, max(5, quotaBackoff) * 2);  // 10, 20, 40 puis 60 min
    nextQuota = millis() + quotaBackoff * 60000UL;
    snprintf(msg, sizeof(msg), "trop d'appels, nouvel essai dans %d min", quotaBackoff);
    quotaFailed(msg, false);
    return;
  }
  if (code == 401 || code == 403) {
    quotaFailed("Claude : acces au quota refuse", true);
    nextQuota = millis() + 15 * 60000UL;
    return;
  }
  if (code != 200) {
    snprintf(msg, sizeof(msg), "quota indisponible (%d)", code);
    quotaFailed(msg, true);
    return;
  }

  quotaBackoff = 0;
  Gauge five = { -1, 0 }, week = { -1, 0 };
  if (!doc["five_hour"]["utilization"].isNull()) five.used = lround(doc["five_hour"]["utilization"].as<double>());
  if (!doc["seven_day"]["utilization"].isNull()) week.used = lround(doc["seven_day"]["utilization"].as<double>());
  five.reset = isoLocal(doc["five_hour"]["resets_at"]);
  week.reset = isoLocal(doc["seven_day"]["resets_at"]);
  const bool ok = five.used >= 0 || week.used >= 0;
  {
    Guard g;
    st.five = five;
    st.week = week;
    st.updated = nowLocal();
    st.have = true;
    snprintf(st.err, sizeof(st.err), "%s", ok ? "" : "reponse sans quota exploitable");
  }
  markSource(SRC_CLAUDE, ok);
  Serial.printf("Quota : 5 h %d %%, semaine %d %%\n", five.used, week.used);
}

// ---------------- État des services Claude ----------------
static void fetchStatus() {
  nextStatus = millis() + STATUS_MS;
  JsonDocument filter;
  filter["status"]["indicator"] = true;
  JsonDocument doc;
  const int code = httpJson(STATUS_URL, NULL, NULL, MDMA_UA, doc, &filter, NULL);
  const char *ind = doc["status"]["indicator"] | "";
  int level = -1;  // -1 injoignable, 0 ok, 1 dégradé ou maintenance, 2 panne
  if (code == 200) {
    if (!strcmp(ind, "none")) level = 0;
    else if (!strcmp(ind, "minor") || !strcmp(ind, "maintenance")) level = 1;
    else if (!strcmp(ind, "major") || !strcmp(ind, "critical")) level = 2;
  }
  {
    Guard g;
    st.level = level;
  }
  markSource(SRC_STATUS, level >= 0);
}

// ---------------- ElevenLabs ----------------
static void fetchEleven() {
  nextEleven = millis() + ELEVEN_MS;
  JsonDocument filter;
  filter["character_count"] = true;
  filter["character_limit"] = true;
  filter["next_character_count_reset_unix"] = true;
  JsonDocument doc;
  const char *hdr[] = { "xi-api-key", ELEVENLABS_API_KEY, NULL };
  const int code = httpJson(ELEVEN_URL, NULL, hdr, MDMA_UA, doc, &filter, NULL);
  {
    Guard g;
    if (code == 401 || code == 403) {
      snprintf(el.err, sizeof(el.err), "cle refusee (permission Utilisateur ?)");
      el.used = -1;
    } else if (code != 200) {
      snprintf(el.err, sizeof(el.err), "ElevenLabs indisponible (%d)", code);
    } else {
      el.used = doc["character_count"] | -1;
      el.limit = doc["character_limit"] | -1;
      el.reset = localEpoch(doc["next_character_count_reset_unix"] | 0L);
      el.updated = nowLocal();
      el.err[0] = 0;
    }
  }
  markSource(SRC_ELEVEN, code == 200);
}

// ---------------- PALais SIAM ----------------
// Connexion avec le compte de l'organisateur (même email et mot de passe que sur
// le site) : les règles du site laissent tout compte connecté lire les commandes.
static String siamToken;
static uint32_t siamExpiresMs = 0;
static long siamLastId = -1;  // dernière commande vue, -1 = pas encore lue

static bool siamLogin() {
  if (siamToken.length() && !due(siamExpiresMs)) return true;
  JsonDocument req;
  req["email"] = PALAIS_SIAM_EMAIL;
  req["password"] = PALAIS_SIAM_PASSWORD;
  String body;
  serializeJson(req, body);
  JsonDocument filter;
  filter["access_token"] = true;
  filter["expires_in"] = true;
  JsonDocument doc;
  const char *hdr[] = { "apikey", PALAIS_SIAM_KEY, NULL };
  const int code = httpJson(PALAIS_SIAM_URL "/auth/v1/token?grant_type=password", body.c_str(), hdr, MDMA_UA, doc, &filter, NULL);
  if (code != 200 || !doc["access_token"].is<const char *>()) {
    Serial.printf("PALais SIAM : connexion refusee (%d), verifiez email et mot de passe\n", code);
    siamToken = "";
    return false;
  }
  siamToken = doc["access_token"].as<const char *>();
  siamExpiresMs = millis() + ((doc["expires_in"] | 3600) - 120) * 1000UL;
  return true;
}

// Lecture sur l'API du site. range : nombre total de lignes demandé au serveur.
static int siamGet(const char *path, JsonDocument &doc, String *range) {
  char url[320];
  snprintf(url, sizeof(url), "%s/rest/v1/%s", PALAIS_SIAM_URL, path);
  const String auth = "Bearer " + siamToken;
  const char *hdr[] = { "apikey", PALAIS_SIAM_KEY, "Authorization", auth.c_str(), NULL, NULL, NULL };
  if (range) {
    hdr[4] = "Prefer";
    hdr[5] = "count=exact";
  }
  const int code = httpJson(url, NULL, hdr, MDMA_UA, doc, NULL, range);
  if (code == 401) siamToken = "";  // session expirée : reconnexion au prochain passage
  return code;
}

// Nom affiché pour une commande : l'invité, la personne pour qui on commande
// (« via » le compte, dans l'annonce), sinon le compte lui-même
static void orderName(JsonObject o, char *out, size_t n, bool via) {
  char raw[96];
  const char *who = o["profiles"]["display_name"] | "Quelqu'un";
  const char *guest = o["guest_name"] | "";
  const char *forName = o["for_name"] | "";
  if (*guest) snprintf(raw, sizeof(raw), "%s", guest);
  else if (*forName && via) snprintf(raw, sizeof(raw), "%s (via %s)", forName, who);
  else if (*forName) snprintf(raw, sizeof(raw), "%s", forName);
  else snprintf(raw, sizeof(raw), "%s", who);
  toAscii(out, n, raw);
}

static void fetchOrders() {
  nextOrders = millis() + ORDERS_MS;
  if (!siamLogin()) {
    nextOrders = millis() + 10 * 60000UL;
    markSource(SRC_SIAM, false);
    return;
  }
  JsonDocument doc;
  if (siamLastId < 0) {
    // Premier passage : on part de la dernière commande, sans annoncer l'existant
    if (siamGet("orders?select=id&order=id.desc&limit=1", doc, NULL) != 200) return;
    siamLastId = doc[0]["id"] | 0L;
    Serial.printf("PALais SIAM : connecte, derniere commande n. %ld\n", siamLastId);
    return;
  }
  // Toutes les commandes, y compris les siennes et celles saisies à la main
  char path[200];
  snprintf(path, sizeof(path), "orders?select=id,guest_name,for_name,profiles(display_name),dishes(name)&id=gt.%ld&order=id.asc", siamLastId);
  if (siamGet(path, doc, NULL) != 200) return;
  JsonArray rows = doc.as<JsonArray>();
  if (!rows.size()) return;
  char names[64] = "", one[48], dish[40] = "";
  for (JsonObject o : rows) {
    siamLastId = o["id"] | siamLastId;
    orderName(o, one, sizeof(one), true);
    if (names[0]) strlcat(names, ", ", sizeof(names));
    strlcat(names, one, sizeof(names));
  }
  if (rows.size() == 1) toAscii(dish, sizeof(dish), rows[0]["dishes"]["name"] | "");
  announceOrder(rows.size(), names, dish);
  Serial.printf("PALais SIAM : %u nouvelle(s) commande(s)\n", (unsigned)rows.size());
  nextSummary = millis();  // résumé à jour tout de suite
}

// Résumé de la semaine en cours : la plus récente, ouverte ou non
static void fetchSummary() {
  nextSummary = millis() + SUMMARY_MS;
  if (!siamLogin()) {
    markSource(SRC_SIAM, false);
    return;
  }
  char path[160];
  JsonDocument wk, orders, guesses, one;
  String range;
  const char *step = "semaine";
  int code = siamGet("weeks?select=id,order_date,deadline,status,answer_revealed&order=order_date.desc&limit=1", wk, NULL);
  bool ok = code == 200 && !wk[0].isNull();
  const long wid = wk[0]["id"] | 0L;
  if (ok) {
    step = "commandes";
    snprintf(path, sizeof(path), "orders?select=id,paid,guest_name,for_name,profiles(display_name)&week_id=eq.%ld&order=id.desc", wid);
    ok = (code = siamGet(path, orders, NULL)) == 200;
  }
  if (ok) {  // en organisateur, toutes les réponses sont lisibles
    step = "devinettes";
    snprintf(path, sizeof(path), "guesses?select=is_correct&week_id=eq.%ld", wid);
    ok = (code = siamGet(path, guesses, NULL)) == 200;
  }
  // Total depuis le début, compté par le serveur : en-tête Content-Range « 0-0/57 ».
  // Une seule ligne sur plusieurs : le serveur répond 206 (contenu partiel).
  if (ok) {
    step = "total";
    code = siamGet("orders?select=id&limit=1", one, &range);
    ok = code == 200 || code == 206;
  }
  if (!ok) {
    Serial.printf("PALais SIAM : resume impossible, etape %s (%d)\n", step, code);
    markSource(SRC_SIAM, false);
    return;
  }

  JsonObject w = wk[0];
  SiamSummary s;
  int y = 0, m = 0, d = 0;
  if (sscanf(w["order_date"] | "", "%d-%d-%d", &y, &m, &d) == 3) s.date = (uint32_t)daysFromCivil(y, m, d) * 86400;
  s.deadline = isoLocal(w["deadline"]);
  s.open = !strcmp(w["status"] | "", "open");
  s.orders = orders.size();
  for (JsonObject o : orders.as<JsonArray>())
    if (o["paid"] | false) s.paid++;
  s.guesses = guesses.size();
  if (w["answer_revealed"] | false) {
    s.correct = 0;
    for (JsonObject g : guesses.as<JsonArray>())
      if (g["is_correct"] | false) s.correct++;
  }
  const int slash = range.indexOf('/');
  s.total = slash >= 0 ? range.substring(slash + 1).toInt() : 0;
  char one3[32];
  uint8_t k = 0;
  for (JsonObject o : orders.as<JsonArray>()) {
    if (k++ == 3) break;
    orderName(o, one3, sizeof(one3), false);
    if (s.recent[0]) strlcat(s.recent, ", ", sizeof(s.recent));
    strlcat(s.recent, one3, sizeof(s.recent));
  }
  s.have = true;
  {
    Guard g;
    sm = s;
  }
  markSource(SRC_SIAM, true);
}

// ---------------- Bourse ----------------
// API de graphiques de Yahoo Finance, publique et sans clé. Les cryptos
// s'écrivent BTC-USD ; BTC, ETH et SOL seuls sont complétés.
static void fetchMarket() {
  nextMarket = millis() + MARKET_MS;
  char label[24];
  const uint8_t sel = marketSel;
  snprintf(label, sizeof(label), "%s", MARKET_LIST[sel]);
  toUpper(label);
  String ticker = label;
  if (ticker == "BTC" || ticker == "ETH" || ticker == "SOL") ticker += "-USD";
  const bool crypto = ticker.endsWith("-USD");
  const size_t ll = strlen(label);
  if (ll > 4 && !strcmp(label + ll - 4, "-USD")) label[ll - 4] = 0;  // « BTC-USD » -> « BTC »
  String sym = ticker;
  sym.replace("^", "%5E");  // indices : ^GSPC
  const String url = "https://query1.finance.yahoo.com/v8/finance/chart/" + sym + "?range=1d&interval=5m";

  JsonDocument filter;
  JsonObject fm = filter["chart"]["result"][0]["meta"].to<JsonObject>();
  fm["regularMarketPrice"] = true;
  fm["chartPreviousClose"] = true;
  fm["shortName"] = true;
  fm["currency"] = true;
  fm["currentTradingPeriod"]["regular"] = true;
  filter["chart"]["result"][0]["indicators"]["quote"][0]["close"] = true;
  JsonDocument doc;
  const int code = httpJson(url.c_str(), NULL, NULL, "Mozilla/5.0", doc, &filter, NULL);
  JsonObject res = doc["chart"]["result"][0];
  if (code != 200 || res.isNull()) {
    Serial.printf("Bourse : %s indisponible (%d)\n", ticker.c_str(), code);
    markSource(SRC_MARKET, false);
    return;
  }
  JsonObject meta = res["meta"];
  static float cl[400];
  uint16_t n = 0;
  for (JsonVariant v : res["indicators"]["quote"][0]["close"].as<JsonArray>())
    if (!v.isNull() && n < sizeof(cl) / sizeof(cl[0])) cl[n++] = v.as<float>();
  const double price = meta["regularMarketPrice"] | 0.0;
  // Référence : clôture de la veille en bourse ; en crypto (24 h/24), début de la journée
  const double ref = crypto && n ? cl[0] : (meta["chartPreviousClose"] | 0.0);
  const long now = time(NULL);
  const bool open = crypto || (now >= (meta["currentTradingPeriod"]["regular"]["start"] | 0L) && now < (meta["currentTradingPeriod"]["regular"]["end"] | 0L));

  Market m;
  snprintf(m.symbol, sizeof(m.symbol), "%s", label);
  char name[48];
  toAscii(name, sizeof(name), meta["shortName"] | label);
  const size_t len = strlen(name);
  if (len > 4 && !strcmp(name + len - 4, " USD")) name[len - 4] = 0;  // « Bitcoin USD » -> « Bitcoin »
  snprintf(m.name, sizeof(m.name), "%s", name);
  fmtPrice(m.price, sizeof(m.price), price);
  m.pct = ref ? (price - ref) / ref * 100 : 0;
  snprintf(m.currency, sizeof(m.currency), "%s", meta["currency"] | "");
  m.open = open;
  m.updated = nowLocal();

  // Courbe : au plus MARKET_POINTS points, plus le dernier cours, ramenés entre 0 et 99
  if (n >= 2) {
    float s[MARKET_POINTS + 2];
    uint8_t k = 0;
    const uint16_t step = max(1, (n + MARKET_POINTS - 1) / MARKET_POINTS);
    for (uint16_t i = 0; i < n && k < MARKET_POINTS; i += step) s[k++] = cl[i];
    s[k++] = price;
    float lo = s[0], hi = s[0];
    for (uint8_t i = 1; i < k; i++) {
      lo = min(lo, s[i]);
      hi = max(hi, s[i]);
    }
    const float span = hi > lo ? hi - lo : 1;
    for (uint8_t i = 0; i < k; i++) m.pts[i] = (uint8_t)lroundf((s[i] - lo) / span * 99);
    m.n = k;
    fmtPrice(m.low, sizeof(m.low), lo);
    fmtPrice(m.high, sizeof(m.high), hi);
  }
  m.rxMs = millis();
  m.have = true;
  {
    Guard g;
    if (sel != marketSel) return;  // cours changé pendant le relevé
    mk = m;
    dirty = true;
  }
  markSource(SRC_MARKET, true);
}

// ---------------- Wi-Fi ----------------
static uint32_t tryStartMs = 0, forcedSinceMs = 0;
static int8_t lastTried = -1;
static bool radioOff = false;

static void markDown() {
  Guard g;
  net.up = false;
  net.current = -1;
  net.downSinceMs = millis();
  dirty = true;
}

static void wifiBegin(int8_t i) {
  WiFi.disconnect();
  WiFi.begin(WIFI_NETS[i].ssid, WIFI_NETS[i].pass);
  tryStartMs = millis();
  lastTried = i;
  {
    Guard g;
    net.trying = i;
    dirty = true;
  }
  Serial.printf("Wi-Fi : essai de %s\n", WIFI_NETS[i].ssid);
}

static void onTimeSync(struct timeval *) {
  timeSynced = true;
}

// Mode automatique : les réseaux de secrets.h dans l'ordre, 15 s chacun.
// Réseau imposé (menu) : lui seul, et retour en automatique après 1 min sans succès.
static void wifiTick() {
  if (sleeping) {  // veille : radio coupée jusqu'au réveil
    if (!radioOff) {
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      radioOff = true;
      markDown();
      Guard g;
      net.trying = -1;
    }
    return;
  }
  if (radioOff) {
    WiFi.mode(WIFI_STA);
    radioOff = false;
    lastTried = -1;
    tryStartMs = 0;
    forcedSinceMs = millis();
    Guard g;
    net.downSinceMs = millis();
  }

  const int8_t req = wantNet;  // choix fait dans le menu
  if (req != NET_REQ_NONE) {
    wantNet = NET_REQ_NONE;
    {
      Guard g;
      net.forced = req;
      dirty = true;
    }
    netPrefs.putChar("wifi", req);
    forcedSinceMs = millis();
    if (req >= 0 && !(net.up && net.current == req)) {
      if (net.up) markDown();
      wifiBegin(req);
      return;
    }
  }

  const wl_status_t ws = WiFi.status();
  if (ws == WL_CONNECTED) {
    Guard g;
    if (!net.up) {
      net.up = true;
      net.current = lastTried;
      net.trying = -1;
      net.upSinceMs = millis();
      snprintf(net.ip, sizeof(net.ip), "%s", WiFi.localIP().toString().c_str());
      dirty = true;
      Serial.printf("Wi-Fi : connecte a %s (%s)\n", WIFI_NETS[lastTried].ssid, net.ip);
      configTzTime(TZ_INFO, "pool.ntp.org", "time.google.com");
    }
    net.rssi = WiFi.RSSI();
    return;
  }
  if (net.up) {  // connexion perdue : on repart du premier réseau de la liste
    Serial.println("Wi-Fi : connexion perdue");
    markDown();
    lastTried = -1;
    tryStartMs = 0;
    forcedSinceMs = millis();
  }
  // Mot de passe refusé ou réseau absent : inutile d'attendre la fin du délai
  const bool failed = ws == WL_CONNECT_FAILED || ws == WL_NO_SSID_AVAIL;
  if (tryStartMs && !failed && millis() - tryStartMs < WIFI_TRY_MS) return;
  if (tryStartMs && failed && millis() - tryStartMs < 3000) return;  // laisse le temps au pilote de démarrer

  if (net.forced >= 0 && millis() - forcedSinceMs > WIFI_FORCED_MS) {
    {
      Guard g;
      net.forced = -1;
    }
    netPrefs.putChar("wifi", -1);
    showToast("Reseau introuvable : mode auto");
  }
  wifiBegin(net.forced >= 0 ? net.forced : (lastTried + 1) % WIFI_COUNT);
}

// ---------------- Tâche ----------------
// Un relevé par passage au plus : un choix fait dans le menu est pris en compte vite
static void fetchOne() {
  if (due(nextStatus)) fetchStatus();
  else if (srcs[SRC_CLAUDE].on && due(nextQuota)) fetchQuota();
  else if (srcs[SRC_ELEVEN].on && due(nextEleven)) fetchEleven();
  else if (srcs[SRC_SIAM].on && due(nextOrders)) fetchOrders();
  else if (srcs[SRC_SIAM].on && due(nextSummary)) fetchSummary();
  else if (srcs[SRC_MARKET].on && (marketChanged || due(nextMarket))) {
    marketChanged = false;
    fetchMarket();
  }
}

static void netTask(void *) {
  for (;;) {
    wifiTick();
    if (timeSynced) {
      timeSynced = false;
      markSource(SRC_TIME, true);
      Serial.println("Heure : reglee par internet");
    }
    if (net.up && timeOk() && !sleeping) fetchOne();
    vTaskDelay(pdMS_TO_TICKS(250));
  }
}

void netSetup() {
  netPrefs.begin("net", false);
  claudeLoad();
  {
    Guard g;
    srcs[SRC_TIME].on = srcs[SRC_STATUS].on = true;
    srcs[SRC_CLAUDE].on = strlen(CLAUDE_REFRESH_TOKEN) > 0;
    srcs[SRC_ELEVEN].on = el.have = strlen(ELEVENLABS_API_KEY) > 0;
    srcs[SRC_MARKET].on = strlen(MARKET_LIST[0]) > 0;
    srcs[SRC_SIAM].on = strlen(PALAIS_SIAM_URL) && strlen(PALAIS_SIAM_KEY) && strlen(PALAIS_SIAM_EMAIL) && strlen(PALAIS_SIAM_PASSWORD);
    if (el.have) snprintf(el.err, sizeof(el.err), "premier releve en cours");
    if (!srcs[SRC_CLAUDE].on) snprintf(st.err, sizeof(st.err), "aucun jeton Claude dans secrets.h");
    const int8_t f = netPrefs.getChar("wifi", -1);
    net.forced = f < WIFI_COUNT ? f : -1;
    net.downSinceMs = millis();
  }
  forcedSinceMs = millis();
  setenv("TZ", TZ_INFO, 1);
  tzset();
  sntp_set_time_sync_notification_cb(onTimeSync);
  WiFi.setHostname("mdma");
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);  // les essais et le choix du réseau sont gérés ici
  xTaskCreate(netTask, "net", 16384, NULL, 1, NULL);
}
