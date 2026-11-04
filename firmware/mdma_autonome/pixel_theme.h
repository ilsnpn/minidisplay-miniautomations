// Thème Pixel : noir et blanc, dessiné sur une grille de gros carrés.
//
// Contrairement aux autres thèmes, qui habillent les mêmes vues, celui-ci a ses propres
// vues. L'écran de 320x172 devient une grille de 35x19 cellules de 9 px (7 px allumés,
// 2 px de filet noir), comme un afficheur à matrice de LED. Pas de bandeau de titre :
// chaque vue montre seulement ses chiffres. Que du noir et du blanc, aucun gris.
//
// La taille du « pixel » change selon l'élément : un petit texte est fait de pixels d'une
// cellule, un gros chiffre de pixels de 2 cellules (un seul carré de 16 px), un verre d'eau
// d'un pixel de 3. La grille garde une bordure noire d'une cellule : tout se dessine dans
// les colonnes 1..33 et les lignes 1..17.
//
// Inclus par mdma_autonome.ino juste avant render() : il utilise ses variables et fonctions.
#pragma once

#define PX_COLS 35
#define PX_ROWS 19
#define PX_PITCH 9  // côté d'une cellule, filet compris
#define PX_GAP 2    // filet noir entre deux pixels
#define PX_OX ((W - PX_COLS * PX_PITCH) / 2)
#define PX_OY ((H - PX_ROWS * PX_PITCH) / 2)
#define PX_MAXW 33  // largeur utilisable : colonnes 1..33

// 0 = éteint ; 1..3 = origine d'un pixel de cette taille (en cellules) ; PX_COVERED = cellule
// recouverte par un pixel plus grand, dont l'origine est ailleurs.
#define PX_COVERED 255
static uint8_t pxGrid[PX_ROWS][PX_COLS];

static void pxClear() {
  memset(pxGrid, 0, sizeof(pxGrid));
}

// Un pixel de u x u cellules, dessiné comme un seul carré
static void pxBlock(int16_t x, int16_t y, int16_t u) {
  if (x < 0 || y < 0 || x + u > PX_COLS || y + u > PX_ROWS) return;
  for (int16_t j = 0; j < u; j++)
    for (int16_t i = 0; i < u; i++) pxGrid[y + j][x + i] = PX_COVERED;
  pxGrid[y][x] = (uint8_t)u;
}

static void pxDot(int16_t x, int16_t y) {
  pxBlock(x, y, 1);
}

static void pxFrame(int16_t x, int16_t y, int16_t w, int16_t h) {
  for (int16_t i = 0; i < w; i++) {
    pxDot(x + i, y);
    pxDot(x + i, y + h - 1);
  }
  for (int16_t j = 0; j < h; j++) {
    pxDot(x, y + j);
    pxDot(x + w - 1, y + j);
  }
}

// Chaque pixel allumé est un carré de PX_PITCH * taille - PX_GAP px à l'écran
static void pxFlush() {
  canvas.fillScreen(COL_BLACK);
  for (int16_t y = 0; y < PX_ROWS; y++)
    for (int16_t x = 0; x < PX_COLS; x++) {
      const uint8_t v = pxGrid[y][x];
      if (v == 0 || v == PX_COVERED) continue;
      canvas.fillRect(PX_OX + x * PX_PITCH, PX_OY + y * PX_PITCH, v * PX_PITCH - PX_GAP, v * PX_PITCH - PX_GAP, COL_WHITE);
    }
}

// ---------------- Police 3x5 ----------------
// Cinq lignes de trois bits (bit de gauche = colonne de gauche), les glyphes étroits
// n'utilisent que leurs premières colonnes.
struct PxGlyph {
  char c;
  uint8_t w;
  uint16_t rows;
};
#define PXG(a, b, c, d, e) ((uint16_t)(((a) << 12) | ((b) << 9) | ((c) << 6) | ((d) << 3) | (e)))
static const PxGlyph PX_FONT[] = {
  { '0', 3, PXG(7, 5, 5, 5, 7) }, { '1', 3, PXG(2, 6, 2, 2, 7) }, { '2', 3, PXG(7, 1, 7, 4, 7) },
  { '3', 3, PXG(7, 1, 7, 1, 7) }, { '4', 3, PXG(5, 5, 7, 1, 1) }, { '5', 3, PXG(7, 4, 7, 1, 7) },
  { '6', 3, PXG(7, 4, 7, 5, 7) }, { '7', 3, PXG(7, 1, 1, 2, 2) }, { '8', 3, PXG(7, 5, 7, 5, 7) },
  { '9', 3, PXG(7, 5, 7, 1, 7) },
  { 'A', 3, PXG(2, 5, 7, 5, 5) }, { 'B', 3, PXG(6, 5, 6, 5, 6) }, { 'C', 3, PXG(3, 4, 4, 4, 3) },
  { 'D', 3, PXG(6, 5, 5, 5, 6) }, { 'E', 3, PXG(7, 4, 6, 4, 7) }, { 'F', 3, PXG(7, 4, 6, 4, 4) },
  { 'G', 3, PXG(3, 4, 5, 5, 3) }, { 'H', 3, PXG(5, 5, 7, 5, 5) }, { 'I', 3, PXG(7, 2, 2, 2, 7) },
  { 'J', 3, PXG(1, 1, 1, 5, 2) }, { 'K', 3, PXG(5, 5, 6, 5, 5) }, { 'L', 3, PXG(4, 4, 4, 4, 7) },
  { 'M', 3, PXG(5, 7, 7, 5, 5) }, { 'N', 3, PXG(6, 5, 5, 5, 5) }, { 'O', 3, PXG(2, 5, 5, 5, 2) },
  { 'P', 3, PXG(6, 5, 6, 4, 4) }, { 'Q', 3, PXG(2, 5, 5, 7, 3) }, { 'R', 3, PXG(6, 5, 6, 5, 5) },
  { 'S', 3, PXG(3, 4, 2, 1, 6) }, { 'T', 3, PXG(7, 2, 2, 2, 2) }, { 'U', 3, PXG(5, 5, 5, 5, 7) },
  { 'V', 3, PXG(5, 5, 5, 5, 2) }, { 'W', 3, PXG(5, 5, 7, 7, 5) }, { 'X', 3, PXG(5, 5, 2, 5, 5) },
  { 'Y', 3, PXG(5, 5, 2, 2, 2) }, { 'Z', 3, PXG(7, 1, 2, 4, 7) },
  { ' ', 2, PXG(0, 0, 0, 0, 0) }, { ':', 1, PXG(0, 4, 0, 4, 0) }, { '.', 1, PXG(0, 0, 0, 0, 4) },
  { '%', 3, PXG(5, 1, 2, 4, 5) }, { '-', 3, PXG(0, 0, 7, 0, 0) }, { '+', 3, PXG(0, 2, 7, 2, 0) },
  { '/', 3, PXG(1, 1, 2, 4, 4) }, { '!', 1, PXG(4, 4, 4, 0, 4) }, { '?', 3, PXG(7, 1, 2, 0, 2) },
  { '>', 3, PXG(4, 2, 1, 2, 4) },
};

static const PxGlyph *pxGlyph(char c) {
  if (c >= 'a' && c <= 'z') c -= 32;
  for (const PxGlyph &g : PX_FONT)
    if (g.c == c) return &g;
  return nullptr;
}

// Largeur du texte avec `gap` cellules entre les lettres, pixels de `u` cellules
static int16_t pxTextWidth(const char *s, int16_t u, int16_t gap) {
  int16_t w = 0, n = 0;
  for (; *s; s++) {
    const PxGlyph *g = pxGlyph(*s);
    if (!g) continue;
    w += g->w * u;
    n++;
  }
  return n ? w + (n - 1) * gap : 0;
}

// Écart entre les lettres : celui d'un pixel, resserré jusqu'à une cellule si le texte
// ne tient pas sinon dans la largeur utilisable (« 25:00 » en pixels de 2).
static int16_t pxGap(const char *s, int16_t u) {
  int16_t gap = u;
  while (gap > 1 && pxTextWidth(s, u, gap) > PX_MAXW) gap--;
  return gap;
}

static int16_t pxTextW(const char *s, int16_t u) {
  return pxTextWidth(s, u, pxGap(s, u));
}

static void pxText(int16_t x, int16_t y, const char *s, int16_t u) {
  const int16_t gap = pxGap(s, u);
  for (; *s; s++) {
    const PxGlyph *g = pxGlyph(*s);
    if (!g) continue;
    for (int16_t r = 0; r < 5; r++)
      for (int16_t c = 0; c < g->w; c++)
        if (g->rows >> (12 - 3 * r + 2 - c) & 1) pxBlock(x + c * u, y + r * u, u);
    x += g->w * u + gap;
  }
}

static void pxTextC(int16_t cx, int16_t y, const char *s, int16_t u) {
  pxText(cx - pxTextW(s, u) / 2, y, s, u);
}

static void pxTextR(int16_t xr, int16_t y, const char *s, int16_t u) {
  pxText(xr + 1 - pxTextW(s, u), y, s, u);
}

// ---------------- Éléments ----------------
// Barre horizontale d'une rangée de pixels de u cellules : la part consommée en pixels
// pleins, le reste en simple filet central. Un repère juste au-dessus de la barre marque
// où l'on devrait en être (frac, 0..1, -1 = aucun).
static void pxBarH(int16_t x, int16_t y, int16_t w, int16_t u, int used, float frac) {
  int16_t units = used > 0 ? (int16_t)(((int32_t)(used > 100 ? 100 : used) * w / u + 50) / 100) : 0;
  if (used > 0 && units < 1) units = 1;
  for (int16_t i = 0; i < units; i++) pxBlock(x + i * u, y, u);
  for (int16_t i = units * u; i < w; i++) pxDot(x + i, y + u / 2);
  if (frac >= 0) pxDot(x + (int16_t)(frac * (w - 1)), y - 1);
}

static void pxPercent(char *out, size_t n, int used, bool sign) {
  if (used < 0) snprintf(out, n, "--");
  else snprintf(out, n, sign ? "%d%%" : "%d", used > 100 ? 100 : used);
}

// Libellé à gauche, valeur à droite, barre de pixels de 2 dessous (vue Claude, jauges)
static void pxGaugeBand(int16_t top, const char *label, const Gauge &g, float frac) {
  char v[8];
  pxText(1, top, label, 1);
  pxPercent(v, sizeof(v), g.used, true);
  pxTextR(33, top, v, 1);
  pxBarH(1, top + 6, 33, 2, g.used, frac);
}

// Colonne : libellé, chiffre en pixels de 2 (vue Claude, chiffres)
static void pxBigColumn(int16_t cx, const char *label, const Gauge &g) {
  char v[8];
  pxTextC(cx, 1, label, 1);
  pxPercent(v, sizeof(v), g.used, false);
  const int16_t u = pxTextW(v, 2) <= 16 ? 2 : 1;  // « 100 » ne tient pas en grand
  pxTextC(cx, u == 2 ? 7 : 9, v, u);
}

// Ligne « libellé ... valeur » (jauges + Eleven)
static void pxLine(int16_t top, const char *label, const Gauge &g) {
  char v[8];
  pxText(1, top, label, 1);
  pxPercent(v, sizeof(v), g.used, true);
  pxTextR(33, top, v, 1);
}

// Colonne à jauge verticale : chiffre, cadre qui se remplit par le bas, libellé
static void pxTallColumn(int16_t cx, const char *label, const Gauge &g) {
  char v[8];
  pxPercent(v, sizeof(v), g.used, false);
  pxTextC(cx, 1, v, 1);
  pxFrame(cx - 2, 7, 5, 5);
  const int16_t fill = g.used > 0 ? ((g.used > 100 ? 100 : g.used) * 3 + 50) / 100 : 0;
  for (int16_t r = 0; r < fill; r++)
    for (int16_t c = 0; c < 3; c++) pxDot(cx - 1 + c, 10 - r);
  pxTextC(cx, 13, label, 1);
}

// ---------------- Vues ----------------
static void pxHome() {
  pxTextC(17, 4, "MDMA", 2);
}

static void pxClaude() {
  const float f5 = windowFraction(st.five, 5), fw = windowFraction(st.week, 168);
  Gauge ge = { -1, 0 };
  float fe = -1;
  elevenGauge(ge, fe);
  switch (claudeStyle) {
    case 1:
      pxBigColumn(9, "5H", st.five);
      pxBigColumn(26, "SEM", st.week);
      break;
    case 2:
      pxLine(1, "5H", st.five);
      pxLine(7, "SEM", st.week);
      pxLine(13, "EL", ge);
      break;
    case 3:
      pxTallColumn(6, "5H", st.five);
      pxTallColumn(17, "SEM", st.week);
      pxTallColumn(28, "EL", ge);
      break;
    default:
      pxGaugeBand(1, "5H", st.five, f5);
      pxGaugeBand(10, "SEM", st.week, fw);
  }
}

static void pxNet() {
  char buf[24];
  // Signal : escalier de pixels de 2, une colonne par barre ; une barre éteinte se réduit à un point
  const int bars = net.up ? max(1, signalBars(net.rssi)) : 0;
  for (int i = 0; i < 4; i++) {
    if (i < bars)
      for (int k = 0; k <= i; k++) pxBlock(1 + 2 * i, 9 - 2 * k, 2);
    else
      pxDot(1 + 2 * i, 10);
  }
  if (net.up) {
    snprintf(buf, sizeof(buf), "%d", -net.rssi);  // toujours négatif : le signe est sous-entendu
    if (pxTextW(buf, 2) <= 14) pxTextR(33, 1, buf, 2);
    else pxTextR(33, 1, buf, 1);
    // Une marque par source, en 3 x 2 : pixel de 2 = relevé réussi, point = échec,
    // rien = pas encore de relevé ou source non configurée
    for (uint8_t i = 0; i < SRC_COUNT; i++) {
      const int16_t x = 10 + 3 * (i % 3), y = 4 + 3 * (i / 3);
      if (!srcs[i].on) continue;
      if (srcs[i].fail) pxDot(x, y);
      else if (srcs[i].okAt) pxBlock(x, y, 2);
    }
    snprintf(buf, sizeof(buf), "%.7s", net.current >= 0 ? WIFI_NETS[net.current].ssid : "");
  } else {
    pxTextR(33, 1, "OFF", 2);
    if (net.trying >= 0) snprintf(buf, sizeof(buf), "%.7s", WIFI_NETS[net.trying].ssid);
    else snprintf(buf, sizeof(buf), "WIFI OFF");
  }
  pxText(1, 12, buf, 1);
}

// Le compte à rebours seul, en gros
static void pxPomodoro() {
  char buf[16];
  const Pomo p = pomoState();
  pomoTrackPhase(p);  // message et LED au changement de phase, même si rien ne s'affiche
  snprintf(buf, sizeof(buf), "%02lu:%02lu", (unsigned long)(p.left / 60), (unsigned long)(p.left % 60));
  pxTextC(17, 4, buf, 2);
}

static void pxMarket() {
  char buf[24], sym[12] = "";
  if (!mk.have) {
    pxTextC(17, 4, "--", 2);
    return;
  }
  const int16_t u = pxTextW(mk.price, 2) <= PX_MAXW ? 2 : 1;
  pxTextC(17, u == 2 ? 1 : 4, mk.price, u);
  // Symbole sans suffixe (« BTC-EUR » : BTC)
  for (uint8_t i = 0; i < sizeof(sym) - 1 && mk.symbol[i] && mk.symbol[i] != '-'; i++) sym[i] = mk.symbol[i];
  // Variation en % à gauche (le signe dit le sens), symbole à droite : une seule décimale si ça ne tient pas
  snprintf(buf, sizeof(buf), "%+.2f", mk.pct);
  if (pxTextW(buf, 1) + 2 + pxTextW(sym, 1) > PX_MAXW) snprintf(buf, sizeof(buf), "%+.1f", mk.pct);
  pxText(1, 12, buf, 1);
  pxTextR(33, 12, sym, 1);
}

static void pxWater() {
  char buf[16];
  waterRollDay(nowLocal());
  const uint32_t ml = (uint32_t)waterGlasses * WATER_GLASS_ML;
  snprintf(buf, sizeof(buf), "%lu.%02lu", (unsigned long)(ml / 1000), (unsigned long)(ml % 1000 / 10));
  const int16_t w = pxTextW(buf, 2), x0 = (PX_COLS - (w + 3 + 3)) / 2;
  pxText(x0, 1, buf, 2);
  pxText(x0 + w + 3, 6, "L", 1);
  // Un verre par rappel, en pixel de 3 : plein si bu, sinon un seul petit point en son centre
  const uint8_t shown = min((int)WATER_TARGET, 8);
  const int16_t gx = (PX_COLS - (shown * 4 - 1)) / 2;
  for (uint8_t i = 0; i < shown; i++) {
    if (i < waterGlasses) pxBlock(gx + i * 4, 12, 3);
    else pxDot(gx + i * 4 + 1, 13);
  }
}

// Options (appui long) : trois lignes visibles, la ligne choisie précédée d'une flèche,
// un point à droite pour l'option déjà active. Libellés courts : 7 lettres au plus.
static void pxMenuLabel(uint8_t i, char *out, size_t n, bool &active) {
  active = false;
  if (i == 0) {
    snprintf(out, n, "ANNULER");
    return;
  }
  switch (view) {
    case VIEW_CLAUDE: {
      static const char *const L[] = { "JAUGES", "GROS", "J+EL", "G+EL" };
      snprintf(out, n, "%s", L[i - 1]);
      active = claudeStyle == i - 1;
      break;
    }
    case VIEW_MARKET:
      snprintf(out, n, "%.7s", MARKET_LIST[i - 1]);
      active = marketSel == i - 1;
      break;
    case VIEW_WATER: {
      static const char *const L[] = { "", "+1", "-1", "A ZERO" };
      snprintf(out, n, "%s", L[i]);
      break;
    }
    case VIEW_POMODORO: snprintf(out, n, "RESET"); break;
    default:  // Réseau
      if (i < MENU_AUTO) {
        snprintf(out, n, "%.7s", WIFI_NETS[i - 1].ssid);
        active = net.up && net.current == i - 1;
      } else if (i == MENU_AUTO) {
        snprintf(out, n, "AUTO");
        active = net.forced < 0;
      } else if (i == MENU_RESTART) snprintf(out, n, "REBOOT");
      else snprintf(out, n, "VEILLE");
  }
}

static void pxMenu() {
  const uint8_t count = menuCount();
  const int first = max(0, min((int)menuSel - 1, (int)count - 3));  // fenêtre de trois lignes autour du choix
  for (uint8_t k = 0; k < 3 && first + k < count; k++) {
    const uint8_t i = first + k;
    const int16_t y = 1 + k * 6;
    char label[16];
    bool active;
    pxMenuLabel(i, label, sizeof(label), active);
    if (i == menuSel) pxText(1, y, ">", 1);
    pxText(5, y, label, 1);
    if (active) pxDot(33, y + 2);
  }
}

static void drawPixelScreen() {
  pxClear();
  if (menuOpen) {
    pxMenu();
  } else {
    switch (view) {
      case VIEW_CLAUDE: pxClaude(); break;
      case VIEW_NET: pxNet(); break;
      case VIEW_POMODORO: pxPomodoro(); break;
      case VIEW_MARKET: pxMarket(); break;
      case VIEW_WATER: pxWater(); break;
      default: pxHome();
    }
  }
  pxFlush();
}
