// MDMA autonome — Waveshare ESP32-C6-LCD-1.47
// Version sans PC : la carte se connecte en Wi-Fi et relève tout elle-même
// (voir reseau.ino). Tous les réglages et secrets sont dans secrets.h.
//
// Vues : Home (logo MDMA), quota Claude (trois versions : jauges, gros chiffres,
// horloge), crédits ElevenLabs, Réseau (signal Wi-Fi et état des sources), Pomodoro,
// PALais SIAM (toujours aux couleurs du site), cours de bourse ou crypto et Eau.
// Une nouvelle commande PALais SIAM est annoncée 5 s par-dessus la vue en cours.
//
// Heures : secondes depuis 1970 en « heure locale à plat » (voir localEpoch),
// affichées avec gmtime comme dans la V1. 0 = inconnue.
//
// Bouton BOOT (GPIO9) : appui court = vue suivante. Appui long : thème suivant sur
// la vue Home ; ailleurs, menu d'options qui défile tant qu'on reste appuyé, le
// relâchement choisit la ligne (version Claude, réseau, cours, verres d'eau...).
// Rappel d'eau : la vue Eau s'affiche par-dessus la vue en cours, un appui note le verre.
// Vue, thème et luminosité sont mémorisés en Flash (NVS).

#include <SPI.h>
#include <time.h>
#include <Preferences.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <ArduinoJson.h>
#include <esp_sntp.h>
#include <esp_crc.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_NeoPixel.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include "state.h"
// Copie locale de secrets.h.example, ignorée par Git. Le banc d'essai du site
// (tools/screensim) lit le modèle à la place, pour ne jamais afficher de vrais réseaux.
#ifndef MDMA_SECRETS
#define MDMA_SECRETS "secrets.h"
#endif
#include MDMA_SECRETS

// Brochage (voir docs/HARDWARE.md)
#define PIN_MOSI 6
#define PIN_SCLK 7
#define PIN_MISO 5
#define PIN_LCD_CS 14
#define PIN_LCD_DC 15
#define PIN_LCD_RST 21
#define PIN_LCD_BL 22
#define PIN_RGB 8
#define PIN_BOOT 9  // bouton BOOT, actif à l'état bas

#define W 320
#define H 172
#define STALE_MS 120000UL  // Wi-Fi perdu depuis plus longtemps : valeurs grisées
#define LONG_PRESS_MS 700  // appui long sur BOOT
#define MENU_STEP_MS 700   // options : une ligne de plus à chaque intervalle d'appui
#define TOAST_MS 1500      // durée d'un message éphémère en pied d'écran
#define LONG_PRESS_BRIGHTNESS 0  // 1 : l'appui long règle la luminosité au lieu du thème

Adafruit_ST7789 tft(&SPI, PIN_LCD_CS, PIN_LCD_DC, PIN_LCD_RST);
GFXcanvas16 canvas(W, H);  // rendu hors écran, puis envoi d'un bloc : pas de scintillement
Adafruit_NeoPixel rgb(1, PIN_RGB, NEO_GRB + NEO_KHZ800);
Preferences prefs;

// ---------------- Thèmes ----------------
// Palettes reprises du site Devian. Mêmes infos partout, seul l'habillage change.
static constexpr uint16_t c565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

enum Header : uint8_t { H_PLAIN, H_DOS, H_WIN98, H_UBUNTU, H_MAC, H_APERTURE };
enum BarStyle : uint8_t { B_ROUND, B_SEGMENT, B_WIN98 };
enum Fx : uint8_t { FX_NONE, FX_SCAN, FX_RAIN };

// nom, fond, piste, texte, atténué, accent, gris, alerte, barre titre fond/texte,
// jauge ok / en avance / en retard, police pixel, en-tête, barres, effet
const Theme THEMES[] = {
  { "Matrix", c565(0, 0, 0), c565(0, 38, 10), c565(0, 255, 65), c565(10, 143, 46), c565(184, 255, 201),
    c565(10, 110, 36), c565(255, 77, 77), 0, 0,
    { 0, 255, 65 }, { 255, 77, 77 }, { 10, 143, 46 }, true, H_PLAIN, B_SEGMENT, FX_RAIN },
  { "MS-DOS", c565(0, 0, 0), c565(48, 48, 48), c565(192, 192, 192), c565(128, 128, 128), c565(255, 255, 255),
    c565(128, 128, 128), c565(255, 255, 85), c565(170, 170, 170), c565(0, 0, 0),
    { 192, 192, 192 }, { 255, 85, 85 }, { 85, 85, 255 }, true, H_DOS, B_SEGMENT, FX_SCAN },
  { "Windows 98", c565(192, 192, 192), c565(223, 223, 223), c565(0, 0, 0), c565(64, 64, 64), c565(0, 0, 128),
    c565(96, 96, 96), c565(128, 0, 0), c565(0, 0, 128), c565(255, 255, 255),
    { 0, 0, 128 }, { 170, 0, 0 }, { 0, 128, 128 }, false, H_WIN98, B_WIN98, FX_NONE },
  { "Ubuntu", c565(48, 10, 36), c565(94, 39, 80), c565(255, 255, 255), c565(179, 155, 171), c565(233, 84, 32),
    c565(122, 106, 117), c565(252, 175, 62), c565(17, 17, 17), c565(255, 255, 255),
    { 233, 84, 32 }, { 239, 41, 41 }, { 138, 226, 52 }, false, H_UBUNTU, B_ROUND, FX_NONE },
  { "Claude Code", c565(10, 10, 14), c565(40, 42, 52), c565(235, 235, 240), c565(140, 145, 158), c565(217, 119, 87),
    c565(110, 115, 125), c565(235, 150, 20), 0, 0,
    { 217, 119, 87 }, { 229, 72, 77 }, { 177, 173, 161 }, false, H_PLAIN, B_ROUND, FX_NONE },
  { "macOS", c565(246, 246, 246), c565(220, 220, 224), c565(29, 29, 31), c565(110, 110, 115), c565(0, 122, 255),
    c565(142, 142, 147), c565(201, 52, 0), c565(232, 232, 232), c565(77, 77, 79),
    { 0, 122, 255 }, { 255, 59, 48 }, { 52, 199, 89 }, false, H_MAC, B_ROUND, FX_NONE },  // mode clair
  { "Aperture Science", c565(0, 0, 0), c565(42, 22, 0), c565(255, 154, 0), c565(154, 92, 0), c565(255, 197, 107),
    c565(110, 66, 0), c565(255, 59, 31), 0, 0,
    { 255, 154, 0 }, { 255, 59, 31 }, { 39, 167, 255 }, true, H_APERTURE, B_SEGMENT, FX_SCAN },
};
#define THEME_COUNT (sizeof(THEMES) / sizeof(THEMES[0]))
#define THEME_DEFAULT 4  // Claude Code

static uint8_t themeIndex = THEME_DEFAULT;
static const Theme *T = &THEMES[THEME_DEFAULT];

#define COL_BG (T->bg)
#define COL_TRACK (T->track)
#define COL_TEXT (T->text)
#define COL_DIM (T->dim)
#define COL_ACCENT (T->accent)
#define COL_GREY (T->grey)
#define COL_WARN (T->warn)
const RGB RGB_UNKNOWN = { 110, 115, 125 };
const uint16_t COL_WHITE = c565(255, 255, 255);
const uint16_t COL_BLACK = 0;

// ---------------- Vues et luminosité ----------------
enum View : uint8_t { VIEW_HOME, VIEW_CLAUDE, VIEW_ELEVEN, VIEW_NET, VIEW_POMODORO, VIEW_SIAM, VIEW_MARKET, VIEW_WATER, VIEW_COUNT };
static const char *const VIEW_NAMES[VIEW_COUNT] = { "MDMA", "Claude", "ElevenLabs", "Reseau", "Pomodoro", "SIAM", "Bourse", "Eau" };

// Vue Claude : trois versions, au choix dans les options
#define CLAUDE_STYLES 3
static const char *const CLAUDE_STYLE_NAMES[CLAUDE_STYLES] = { "jauges", "chiffres", "horloge" };
static uint8_t claudeStyle = 0;

// Pomodoro : 25 min de travail, 5 min de pause, en boucle. Démarre après 5 s passées
// sur la vue, s'arrête dès qu'on la quitte. Tourne sur la carte seule, sans le PC.
#define POMO_ARM_MS 5000UL
#define POMO_WORK_S (25 * 60UL)
#define POMO_BREAK_S (5 * 60UL)
#define POMO_ALERT_MS 4000UL  // clignotement de la LED à chaque changement de phase
static uint32_t pomoEnter = 0;     // millis() à l'arrivée sur la vue
static int8_t pomoLastPhase = -1;  // pour détecter le passage travail <-> pause
static uint32_t pomoAlertUntil = 0;
static uint8_t view = VIEW_HOME;
static uint32_t homeEnter = 0;  // millis() à l'arrivée sur Home : déploiement du logo
#define HOME_ANIM_MS 1600

const uint8_t BL_LEVELS[] = { 255, 110, 35 };  // 100 %, ~45 %, ~15 %
const char *const BL_NAMES[] = { "100%", "45%", "15%" };
static uint8_t blIndex = 0;

// Annonce d'une nouvelle commande PALais SIAM
#define ORDER_MS 5000UL
static uint32_t orderStart = 0, orderUntil = 0;
static int orderCount = 0;
static char orderNames[64] = "", orderDish[40] = "";

static char toast[40] = "";
static uint32_t toastUntil = 0;

// ---------------- État relevé ----------------
// Écrit par la tâche réseau, lu par l'affichage : tout accès passe par Guard.
State st;
Eleven el;
SiamSummary sm;
Market mk;
NetInfo net;
Source srcs[SRC_COUNT] = {};
#define MARKET_STALE_MS 600000UL  // sans relevé depuis 10 min : cours grisé

static const WifiNet WIFI_NETS[] = WIFI_NETWORKS;
#define WIFI_COUNT ((int8_t)(sizeof(WIFI_NETS) / sizeof(WIFI_NETS[0])))

// Cours proposés dans le menu de la vue Bourse, le premier par défaut
#ifndef MARKET_SYMBOLS
#define MARKET_SYMBOLS { MARKET_SYMBOL }
#endif
static const char *const MARKET_LIST[] = MARKET_SYMBOLS;
#define MARKET_COUNT ((uint8_t)(sizeof(MARKET_LIST) / sizeof(MARKET_LIST[0])))
static volatile uint8_t marketSel = 0;
static volatile bool marketChanged = false;  // à relever tout de suite par la tâche réseau

// Eau : WATER_GOAL_ML par jour en verres de WATER_GLASS_ML, répartis entre WATER_FROM_H
// et WATER_TO_H. 1,5 L en verres de 25 cl de 9 h à 19 h : un rappel toutes les 1 h 40.
// Un rappel ne sonne que si l'on est en retard sur ce rythme.
#ifndef WATER_GOAL_ML
#define WATER_GOAL_ML 1500
#endif
#ifndef WATER_GLASS_ML
#define WATER_GLASS_ML 250
#endif
#ifndef WATER_FROM_H
#define WATER_FROM_H 9
#endif
#ifndef WATER_TO_H
#define WATER_TO_H 19
#endif
#define WATER_TARGET ((WATER_GOAL_ML + WATER_GLASS_ML - 1) / WATER_GLASS_ML)  // verres par jour
#define WATER_STEP_S ((WATER_TO_H - WATER_FROM_H) * 3600UL / WATER_TARGET)    // écart entre deux verres
#define WATER_SHOW_MS 60000UL      // sans appui, le rappel s'efface au bout d'une minute
#define WATER_SNOOZE_MS 1200000UL  // puis revient 20 min plus tard si le verre manque toujours
static uint32_t waterUntil = 0, waterSnoozeMs = 0;
static uint8_t waterGlasses = 0;  // verres bus aujourd'hui
static uint32_t waterDay = 0;     // jour de ce compte (heure locale / 86400)

static SemaphoreHandle_t gLock;    // verrou récursif partagé avec la tâche réseau
static volatile bool dirty = false;  // nouvelles données : à redessiner
struct Guard {
  Guard() { xSemaphoreTakeRecursive(gLock, portMAX_DELAY); }
  ~Guard() { xSemaphoreGiveRecursive(gLock); }
};

// Demandes du menu à la tâche réseau
#define NET_REQ_NONE -2
static volatile int8_t wantNet = NET_REQ_NONE;  // -1 = automatique, sinon index du réseau
static volatile bool sleeping = false;          // veille : écran noir, Wi-Fi coupé

// ---------------- Heure ----------------
// Heure réglée par internet (NTP) une fois le Wi-Fi connecté
static bool timeOk() {
  return time(NULL) > 1700000000;
}

// Jours depuis le 1er janvier 1970 (algorithme « days from civil » de H. Hinnant)
static int32_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int32_t)doe - 719468;
}

// Heure UTC -> secondes depuis 1970 de l'heure locale « à plat » (fuseau TZ_INFO,
// heure d'été comprise). L'affichage les lit avec gmtime, comme dans la V1.
static uint32_t localEpoch(time_t utc) {
  if (utc <= 0) return 0;
  struct tm t;
  localtime_r(&utc, &t);
  return (uint32_t)daysFromCivil(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday) * 86400 + t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec;
}

static uint32_t nowLocal() {
  return timeOk() ? localEpoch(time(NULL)) : 0;
}

// Valeurs grisées : heure inconnue, ou Wi-Fi perdu depuis plus de 2 minutes
static bool isStale() {
  return !timeOk() || (!net.up && millis() - net.downSinceMs > STALE_MS);
}

// Part écoulée de la fenêtre (0..1), ou -1 si l'heure de reset est inconnue.
static float windowFraction(const Gauge &g, uint32_t hours) {
  if (!g.reset || !timeOk()) return -1;
  const float span = hours * 3600.0f;
  float f = ((float)nowLocal() - ((float)g.reset - span)) / span;
  return f < 0 ? 0 : (f > 1 ? 1 : f);
}

static RGB lerpRGB(RGB a, RGB b, float t) {
  return { (uint8_t)(a.r + (b.r - a.r) * t), (uint8_t)(a.g + (b.g - a.g) * t), (uint8_t)(a.b + (b.b - a.b) * t) };
}

// Écart signé entre consommation et repère de temps. Jusqu'à 10 points : couleur
// normale du thème. Au-delà, elle glisse vers la couleur « en avance » (risque de
// manquer) ou « en retard » (de la marge), atteinte à 35 points.
static RGB gaugeRGB(const Gauge &g, float frac) {
  if (g.used < 0) return RGB_UNKNOWN;
  if (frac < 0) return T->ok;
  const float gap = g.used - 100.0f * frac;
  float t = (fabsf(gap) - 10.0f) / 25.0f;
  t = t < 0 ? 0 : (t > 1 ? 1 : t);
  return lerpRGB(T->ok, gap > 0 ? T->ahead : T->behind, t);
}

static uint16_t gaugeColor(const Gauge &g, float frac) {
  const RGB c = gaugeRGB(g, frac);
  return c565(c.r, c.g, c.b);
}

static uint16_t levelColor(int level) {
  switch (level) {
    case 0: return c565(34, 160, 90);
    case 1: return c565(235, 150, 20);
    case 2: return c565(210, 45, 45);
    default: return COL_GREY;
  }
}

// ---------------- Texte ----------------
// Rôles typographiques : FreeSans pour les thèmes modernes, police pixel intégrée
// (agrandie) pour les thèmes rétro.
enum Role : uint8_t { F_SMALL, F_BODY, F_LABEL, F_TITLE, F_VALUE, F_SUB, F_BIG, F_HUGE };
static uint8_t pxSize = 0;  // > 0 : police pixel active, à cette taille

static void setRole(uint8_t r) {  // r : un Role (uint8_t pour les prototypes Arduino)
  static const GFXfont *const GFX[] = { NULL, &FreeSans9pt7b, &FreeSansBold9pt7b, &FreeSansBold12pt7b,
                                        &FreeSansBold12pt7b, &FreeSans12pt7b, &FreeSansBold24pt7b, &FreeSansBold24pt7b };
  static const uint8_t PX[] = { 1, 2, 2, 2, 3, 2, 5, 7 };
  if (r == F_SMALL || T->pixel) {
    canvas.setFont(NULL);
    canvas.setTextSize(PX[r]);
    pxSize = PX[r];
  } else {
    canvas.setFont(GFX[r]);
    canvas.setTextSize(r == F_HUGE ? 2 : 1);
    pxSize = 0;
  }
}

static int16_t textWidth(const char *s) {
  int16_t x1, y1;
  uint16_t w, h;
  canvas.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  return x1 + w;
}

// baseline : ligne de base du texte, y compris pour la police pixel (ancrée en haut)
static void drawLeft(const char *s, int16_t x, int16_t baseline, uint16_t col) {
  canvas.setTextColor(col);
  canvas.setCursor(x, pxSize ? baseline - 7 * pxSize : baseline);
  canvas.print(s);
}

static void drawRight(const char *s, int16_t xRight, int16_t baseline, uint16_t col) {
  drawLeft(s, xRight - textWidth(s), baseline, col);
}

static void drawCenter(const char *s, int16_t cx, int16_t baseline, uint16_t col) {
  drawLeft(s, cx - textWidth(s) / 2, baseline, col);
}

static void toUpper(char *s) {
  for (; *s; s++)
    if (*s >= 'a' && *s <= 'z') *s -= 32;
}

// Polices ASCII uniquement (suffixe 7b) : pas d'accents.
static const char *const DAYS_LONG[] = { "dimanche", "lundi", "mardi", "mercredi", "jeudi", "vendredi", "samedi" };
static const char *const MONTHS[] = { "janvier", "fevrier", "mars", "avril", "mai", "juin",
                                      "juillet", "aout", "septembre", "octobre", "novembre", "decembre" };

static void fmtClock(char *out, size_t n, uint32_t t) {
  time_t tt = t;
  struct tm tm;
  gmtime_r(&tt, &tm);  // t est déjà en heure locale (voir localEpoch)
  snprintf(out, n, "%02d:%02d", tm.tm_hour, tm.tm_min);
}

// Heure de remise à zéro : « 21:00 » aujourd'hui, « demain 02:40 », sinon « jeudi 18:00 ».
static void fmtReset(char *out, size_t n, uint32_t reset) {
  const uint32_t now = nowLocal();
  time_t tr = reset;
  struct tm r;
  gmtime_r(&tr, &r);
  const int32_t days = (int32_t)(reset / 86400) - (int32_t)(now / 86400);  // jours calendaires d'écart
  if (days <= 0) snprintf(out, n, "%02d:%02d", r.tm_hour, r.tm_min);
  else if (days == 1) snprintf(out, n, "demain %02d:%02d", r.tm_hour, r.tm_min);
  else snprintf(out, n, "%s %02d:%02d", DAYS_LONG[r.tm_wday], r.tm_hour, r.tm_min);
}

// ---------------- Dessin : éléments communs ----------------
#define MX 10  // marge horizontale

// Relief Windows 98 : clair en haut à gauche, sombre en bas à droite (inversé si creux)
static void bevel(int16_t x, int16_t y, int16_t w, int16_t h, bool sunken) {
  const uint16_t hi = sunken ? c565(128, 128, 128) : COL_WHITE;
  const uint16_t lo = sunken ? COL_WHITE : c565(128, 128, 128);
  canvas.drawFastHLine(x, y, w, hi);
  canvas.drawFastVLine(x, y, h, hi);
  canvas.drawFastHLine(x, y + h - 1, w, lo);
  canvas.drawFastVLine(x + w - 1, y, h, lo);
}

// Disque anticrénelé : chaque pixel du bord est mélangé au fond selon la part
// couverte. fillCircle trace un hexagone à ces petits rayons.
static void fillCircleAA(int16_t cx, int16_t cy, float r, uint16_t col) {
  const float fx = cx + 0.5f, fy = cy + 0.5f;  // centre au milieu du pixel : disque symétrique
  const int16_t n = (int16_t)ceilf(r) + 1;
  const uint8_t r1 = (col >> 11) & 0x1F, g1 = (col >> 5) & 0x3F, b1 = col & 0x1F;
  for (int16_t y = cy - n; y <= cy + n; y++) {
    if (y < 0 || y >= H) continue;
    for (int16_t x = cx - n; x <= cx + n; x++) {
      if (x < 0 || x >= W) continue;
      const float d = sqrtf((x + 0.5f - fx) * (x + 0.5f - fx) + (y + 0.5f - fy) * (y + 0.5f - fy));
      float a = r + 0.5f - d;  // couverture approchée du pixel
      if (a <= 0) continue;
      if (a >= 1) {
        canvas.drawPixel(x, y, col);
        continue;
      }
      const uint16_t bg = canvas.getPixel(x, y);
      const uint8_t r0 = (bg >> 11) & 0x1F, g0 = (bg >> 5) & 0x3F, b0 = bg & 0x1F;
      canvas.drawPixel(x, y, ((uint16_t)(r0 + (r1 - r0) * a) << 11) | ((uint16_t)(g0 + (g1 - g0) * a) << 5) | (uint16_t)(b0 + (b1 - b0) * a));
    }
  }
}

// Pastille d'état des services Claude (status.claude.com)
static void statusDot(int16_t cx, int16_t cy, int16_t r) {
  fillCircleAA(cx, cy, r, isStale() ? COL_GREY : levelColor(st.level));
}

// Logo d'Aperture Science : un diaphragme à huit lames
static void apertureLogo(int16_t cx, int16_t cy, int16_t r) {
  fillCircleAA(cx, cy, r, COL_TEXT);
  fillCircleAA(cx, cy, r * 0.4f, COL_BG);
  for (int i = 0; i < 8; i++) {
    const float a = i * PI / 4, b = a + 1.0f;
    canvas.drawLine(cx + cosf(a) * r * 0.4f, cy + sinf(a) * r * 0.4f,
                    cx + cosf(b) * (r + 1), cy + sinf(b) * (r + 1), COL_BG);
  }
}

// En-tête : titre et sous-titre habillés selon le thème, plus la pastille d'état
static void drawHeader(const char *title, const char *sub) {
  char t[24], s[24], buf[48];
  snprintf(t, sizeof(t), "%s", title);
  snprintf(s, sizeof(s), "%s", sub);
  if (T->pixel) {
    toUpper(t);
    toUpper(s);
  }

  switch (T->header) {
    case H_DOS:  // barre de menu inversée, invite de commande
      canvas.fillRect(0, 0, W, 24, T->barBg);
      setRole(F_TITLE);
      snprintf(buf, sizeof(buf), "C:\\%s>%s", t, s);
      drawLeft(buf, MX, 19, T->barFg);
      fillCircleAA(W - MX - 6, 12, 7, T->barFg);
      statusDot(W - MX - 6, 12, 5);
      break;

    case H_WIN98: {  // fenêtre en relief, barre de titre en dégradé, bouton fermer
      bevel(0, 0, W, H, false);
      for (int16_t x = 3; x < W - 3; x++) {
        const RGB c = lerpRGB({ 0, 0, 128 }, { 16, 132, 208 }, (float)(x - 3) / (W - 6));
        canvas.drawFastVLine(x, 3, 20, c565(c.r, c.g, c.b));
      }
      setRole(F_LABEL);
      drawLeft(t, 8, 18, COL_WHITE);
      drawLeft(s, 8 + textWidth(t) + 5, 18, c565(200, 215, 240));
      const int16_t bx = W - 21, by = 6;
      canvas.fillRect(bx, by, 16, 14, COL_BG);
      bevel(bx, by, 16, 14, false);
      for (int8_t d = 0; d < 2; d++) {
        canvas.drawLine(bx + 4 + d, by + 3, bx + 10 + d, by + 9, COL_BLACK);
        canvas.drawLine(bx + 10 + d, by + 3, bx + 4 + d, by + 9, COL_BLACK);
      }
      statusDot(bx - 12, 13, 5);
      break;
    }

    case H_UBUNTU:  // barre supérieure GNOME
      canvas.fillRect(0, 0, W, 24, T->barBg);
      setRole(F_LABEL);
      drawLeft(t, MX, 17, T->barFg);
      drawLeft(s, MX + textWidth(t) + 5, 17, COL_ACCENT);
      statusDot(W - MX - 6, 12, 5);
      break;

    case H_MAC:  // barre de titre et feux tricolores
      canvas.fillRect(0, 0, W, 26, T->barBg);
      canvas.drawFastHLine(0, 26, W, c565(200, 200, 204));  // filet de séparation
      fillCircleAA(16, 13, 6, c565(255, 95, 87));
      fillCircleAA(34, 13, 6, c565(254, 188, 46));
      fillCircleAA(52, 13, 6, c565(40, 200, 64));
      setRole(F_LABEL);
      snprintf(buf, sizeof(buf), "%s %s", t, s);
      drawCenter(buf, W / 2, 18, T->barFg);
      statusDot(W - MX - 6, 13, 5);
      break;

    case H_APERTURE:  // logo, puis titre façon terminal d'enrichissement
      apertureLogo(MX + 10, 13, 10);
      setRole(F_TITLE);
      drawLeft(t, MX + 26, 21, COL_ACCENT);
      drawLeft(s, MX + 26 + textWidth(t) + 8, 21, COL_DIM);
      canvas.drawFastHLine(MX, 28, W - 2 * MX, COL_DIM);
      statusDot(W - MX - 6, 13, 6);
      break;

    default: {  // H_PLAIN : titre en couleur d'accent, sous-titre atténué
      setRole(F_TITLE);
      drawLeft(t, MX, 22, COL_ACCENT);
      const int16_t titleW = textWidth(t);  // mesuré avec la police du titre
      setRole(F_BODY);
      drawLeft(s, MX + titleW + 6, 22, COL_DIM);
      statusDot(W - MX - 6, 15, 6);
      break;
    }
  }
}

// Barre horizontale avec repère de temps
static void drawBar(int16_t bx, int16_t by, int16_t bw, int16_t bh, const Gauge &g, float frac, uint16_t col) {
  const int16_t pct = g.used > 100 ? 100 : g.used;
  const int16_t fw = g.used > 0 ? (int32_t)bw * pct / 100 : 0;

  switch (T->bar) {
    case B_SEGMENT:  // blocs pleins façon terminal
      canvas.fillRect(bx, by, bw, bh, COL_TRACK);
      for (int16_t x = 0; x + 2 <= fw; x += 6) canvas.fillRect(bx + x, by, fw - x < 4 ? fw - x : 4, bh, col);
      break;

    case B_WIN98:  // barre de progression creuse à blocs
      canvas.fillRect(bx, by, bw, bh, COL_TRACK);
      bevel(bx, by, bw, bh, true);
      for (int16_t x = 2; x + 2 <= fw - 2; x += 10) canvas.fillRect(bx + x, by + 2, fw - 2 - x < 8 ? fw - 2 - x : 8, bh - 4, col);
      break;

    default: {  // B_ROUND
      const int16_t r = bh / 3;
      canvas.fillRoundRect(bx, by, bw, bh, r, COL_TRACK);
      if (fw >= 2 * r) canvas.fillRoundRect(bx, by, fw, bh, r, col);
      else if (fw > 0) canvas.fillRect(bx, by + 2, fw, bh - 4, col);
      break;
    }
  }
  // Repère : où l'on devrait être si la conso suivait le rythme de la fenêtre
  if (frac >= 0) {
    int16_t mx = bx + (int16_t)(frac * (bw - 1));
    canvas.fillRect(mx - 1, by - 3, 3, bh + 6, COL_BG);
    canvas.drawFastVLine(mx, by - 3, bh + 6, COL_TEXT);
  }
}

static bool menuOpen = false;  // options de la vue, ouvertes par un appui long
static bool waterActive();
static void waterFooter(char *out, size_t n);

// Pied de page : état de la liaison à gauche, indicateur de vue à droite
static void drawFooter() {
  char buf[80];
  uint16_t col = COL_GREY;
  if (millis() < toastUntil) {
    snprintf(buf, sizeof(buf), "%s", toast);
    col = COL_TEXT;
  } else if (menuOpen) {
    snprintf(buf, sizeof(buf), "Relachez pour choisir");
  } else if (waterActive()) {
    snprintf(buf, sizeof(buf), "Appui : verre bu");
    col = COL_TEXT;
  } else if (view == VIEW_HOME) {
    snprintf(buf, sizeof(buf), "Appui long : theme suivant");
  } else if (view == VIEW_NET) {
    snprintf(buf, sizeof(buf), "Appui long : menu");
  } else if (view == VIEW_WATER) {
    waterFooter(buf, sizeof(buf));
  } else if (!net.up && !timeOk()) {
    snprintf(buf, sizeof(buf), "Connexion Wi-Fi %s...", net.trying >= 0 ? WIFI_NETS[net.trying].ssid : "");
    col = COL_WARN;
  } else if (view == VIEW_POMODORO) {
    buf[0] = 0;  // le Pomodoro n'a pas besoin du réseau
  } else if (!net.up && millis() - net.downSinceMs > STALE_MS) {
    snprintf(buf, sizeof(buf), "Wi-Fi perdu depuis %lu min", (unsigned long)((millis() - net.downSinceMs) / 60000));
    col = COL_WARN;
  } else if (!timeOk()) {
    snprintf(buf, sizeof(buf), "Reglage de l'heure...");
  } else if (view == VIEW_MARKET) {
    if (mk.have && mk.updated) {
      char t[8];
      fmtClock(t, sizeof(t), mk.updated);
      snprintf(buf, sizeof(buf), "%s - releve a %s", mk.open ? "Marche ouvert" : "Marche ferme", t);
    } else {
      buf[0] = 0;
    }
  } else if (view == VIEW_ELEVEN && el.updated) {
    char t[8];
    fmtClock(t, sizeof(t), el.updated);
    snprintf(buf, sizeof(buf), "Releve a %s", t);
  } else if (view != VIEW_ELEVEN && st.err[0]) {
    snprintf(buf, sizeof(buf), "%.44s", st.err);  // laisse la place aux points de vue
    col = COL_WARN;
  } else if (view != VIEW_ELEVEN && st.updated) {
    char t[8];
    fmtClock(t, sizeof(t), st.updated);
    snprintf(buf, sizeof(buf), "Releve a %s", t);
  } else {
    buf[0] = 0;
  }
  setRole(F_SMALL);
  canvas.setTextColor(col);
  canvas.setCursor(MX, H - 11);
  canvas.print(buf);

  for (uint8_t i = 0; i < VIEW_COUNT; i++) {
    int16_t x = W - MX - 3 - (VIEW_COUNT - 1 - i) * 10;
    fillCircleAA(x, H - 8, 3, i == view ? COL_TEXT : COL_TRACK);
  }
}

// ---------------- Effets d'écran ----------------
// Pluie de caractères Matrix en fond, renouvelée toutes les 5 s
static void drawRain() {
  uint32_t seed = timeOk() ? nowLocal() / 5 : millis() / 5000;
  setRole(F_SMALL);
  for (int16_t x = 2; x < W; x += 12) {
    seed = seed * 1103515245u + 12345u;
    const int16_t len = 4 + (seed >> 16) % 10;
    const int16_t y0 = (int16_t)((seed >> 8) % (H / 9)) * 9 - 40;
    for (int16_t i = 0; i < len; i++) {
      seed = seed * 1103515245u + 12345u;
      const int16_t y = y0 + i * 9;
      if (y < 0 || y > H - 8) continue;
      const uint8_t k = 22 + 48 * i / len;  // la tête de colonne est la plus claire
      const uint16_t c = c565(0, k, k / 4);
      canvas.drawChar(x, y, 33 + (seed >> 16) % 90, c, c, 1);  // fond = couleur : transparent
    }
  }
}

// Lignes de balayage d'écran cathodique : une ligne sur trois assombrie de moitié
static void drawScanlines() {
  uint16_t *b = canvas.getBuffer();
  for (int16_t y = 1; y < H; y += 3) {
    uint16_t *p = b + y * W;
    for (int16_t x = 0; x < W; x++) p[x] = (p[x] >> 1) & 0x7BEF;
  }
}

// ---------------- Vue 1 : jauges ----------------
static void drawGauge(int16_t top, const char *label, const Gauge &g, uint32_t hours) {
  const float frac = windowFraction(g, hours);
  const uint16_t col = isStale() ? COL_GREY : gaugeColor(g, frac);
  char buf[32];

  // Ligne titre : libellé à gauche, pourcentage à droite
  setRole(F_LABEL);
  drawLeft(label, MX, top + 14, COL_DIM);
  setRole(F_VALUE);
  if (g.used >= 0) snprintf(buf, sizeof(buf), "%d%%", g.used);
  else snprintf(buf, sizeof(buf), "--");
  drawRight(buf, W - MX, top + 17, g.used >= 0 ? col : COL_GREY);

  const int16_t by = top + 22, bh = 16;
  drawBar(MX, by, W - 2 * MX, bh, g, frac, col);

  // Sous la barre : l'heure de remise à zéro, rien d'autre
  setRole(F_BODY);
  if (g.reset && timeOk()) {
    fmtReset(buf, sizeof(buf), g.reset);
    drawLeft(buf, MX, by + bh + 16, COL_DIM);
  }
}

static void drawGaugesView() {
  drawHeader("Claude", "usage");
  drawGauge(32, "SESSION 5 H", st.five, 5);
  drawGauge(94, "SEMAINE", st.week, 168);
}

// ---------------- Vue 2 : gros chiffres ----------------
static void drawBigColumn(int16_t cx, const char *label, const Gauge &g, uint32_t hours) {
  const float frac = windowFraction(g, hours);
  const uint16_t col = isStale() ? COL_GREY : gaugeColor(g, frac);
  char buf[24];

  setRole(F_LABEL);
  drawCenter(label, cx, 54, COL_DIM);

  setRole(F_BIG);
  if (g.used >= 0) snprintf(buf, sizeof(buf), "%d%%", g.used);
  else snprintf(buf, sizeof(buf), "--");
  drawCenter(buf, cx, 102, g.used >= 0 ? col : COL_GREY);

  drawBar(cx - 62, 114, 124, 8, g, frac, col);

  setRole(F_BODY);
  if (g.reset && timeOk()) {
    fmtReset(buf, sizeof(buf), g.reset);
    drawCenter(buf, cx, 146, COL_DIM);
  }
}

static void drawBigView() {
  drawHeader("Claude", "usage");
  canvas.drawFastVLine(W / 2, 40, 112, COL_TRACK);
  drawBigColumn(W / 4, "SESSION 5 H", st.five, 5);
  drawBigColumn(3 * W / 4, "SEMAINE", st.week, 168);
}

// ---------------- Vue 3 : horloge ----------------
static void drawClockView() {
  char buf[48];
  drawHeader("Claude", "usage");

  setRole(F_HUGE);
  if (timeOk()) fmtClock(buf, sizeof(buf), nowLocal());
  else snprintf(buf, sizeof(buf), "--:--");
  drawCenter(buf, W / 2, 104, isStale() ? COL_GREY : COL_TEXT);

  setRole(F_SUB);
  if (timeOk()) {
    time_t tt = nowLocal();
    struct tm tm;
    gmtime_r(&tt, &tm);
    snprintf(buf, sizeof(buf), "%s %d %s", DAYS_LONG[tm.tm_wday], tm.tm_mday, MONTHS[tm.tm_mon]);
    drawCenter(buf, W / 2, 130, COL_DIM);
  }

  // Résumé du quota
  setRole(F_BODY);
  if (st.five.used >= 0 && st.week.used >= 0) snprintf(buf, sizeof(buf), "5 h %d%%  -  semaine %d%%", st.five.used, st.week.used);
  else snprintf(buf, sizeof(buf), "quota inconnu");
  drawCenter(buf, W / 2, 152, isStale() ? COL_GREY : COL_ACCENT);
}

// ---------------- Vue 4 : crédits ElevenLabs ----------------
// Milliers lisibles : 19983 -> « 19 983 »
static void fmtThousands(char *out, size_t n, int32_t v) {
  if (v >= 1000000) snprintf(out, n, "%ld %03ld %03ld", (long)(v / 1000000), (long)(v / 1000 % 1000), (long)(v % 1000));
  else if (v >= 1000) snprintf(out, n, "%ld %03ld", (long)(v / 1000), (long)(v % 1000));
  else snprintf(out, n, "%ld", (long)v);
}

static void drawElevenView() {
  char buf[48], a[16], b[16], rs[16];
  drawHeader("ElevenLabs", "credits");

  const bool ok = el.have && el.used >= 0 && el.limit > 0;
  if (!ok) {
    setRole(F_SUB);
    drawCenter(el.have ? "solde indisponible" : "non configure", W / 2, 90, COL_GREY);
    setRole(F_BODY);
    if (!el.have) drawCenter("cle API dans secrets.h", W / 2, 116, COL_DIM);
    else if (el.err[0]) drawCenter(el.err, W / 2, 116, COL_WARN);
    return;
  }

  const int32_t left = el.limit > el.used ? el.limit - el.used : 0;
  const int usedPct = (int)((int64_t)el.used * 100 / el.limit);
  // Repère de rythme sur une période mensuelle (~30 jours avant le reset)
  Gauge g = { usedPct, el.reset };
  const float frac = windowFraction(g, 30 * 24);
  const uint16_t col = isStale() ? COL_GREY : gaugeColor(g, frac);

  setRole(F_LABEL);
  drawLeft("RESTANTS", MX, 50, COL_DIM);
  setRole(F_BIG);
  fmtThousands(buf, sizeof(buf), left);
  drawLeft(buf, MX, 92, isStale() ? COL_GREY : COL_TEXT);
  setRole(F_VALUE);
  snprintf(buf, sizeof(buf), "%d%%", 100 - usedPct);
  drawRight(buf, W - MX, 92, col);

  drawBar(MX, 102, W - 2 * MX, 16, g, frac, col);

  setRole(F_BODY);
  rs[0] = 0;
  if (el.reset) {
    time_t tr = el.reset;
    struct tm r;
    gmtime_r(&tr, &r);
    snprintf(rs, sizeof(rs), "reset %d/%02d", r.tm_mday, r.tm_mon + 1);
    drawRight(rs, W - MX, 138, COL_DIM);
  }
  fmtThousands(a, sizeof(a), el.used);
  fmtThousands(b, sizeof(b), el.limit);
  snprintf(buf, sizeof(buf), "%s / %s utilises", a, b);
  // Police pixel plus large : on retire le mot si les deux textes se chevauchent
  if (textWidth(buf) + textWidth(rs) + 10 > W - 2 * MX) snprintf(buf, sizeof(buf), "%s/%s", a, b);
  drawLeft(buf, MX, 138, COL_DIM);
}

// ---------------- Vue 5 : réseau ----------------
// Menu : Annuler, un réseau par ligne, Automatique, Redémarrer, Veille
static uint8_t menuSel = 0;
#define MENU_AUTO (1 + WIFI_COUNT)
#define MENU_RESTART (MENU_AUTO + 1)
#define MENU_SLEEP (MENU_AUTO + 2)
#define MENU_COUNT (MENU_AUTO + 3)

// Options de chaque vue (appui long), « Annuler » en premier. 0 : pas d'options.
#define WATER_MENU 4
static uint8_t menuCount() {
  switch (view) {
    case VIEW_CLAUDE: return 1 + CLAUDE_STYLES;
    case VIEW_NET: return MENU_COUNT;
    case VIEW_MARKET: return 1 + MARKET_COUNT;
    case VIEW_WATER: return WATER_MENU;
    case VIEW_POMODORO: return 2;
    default: return 0;
  }
}

static void menuLabel(uint8_t i, char *label, size_t ln, char *tag, size_t tn) {
  if (i == 0) {
    snprintf(label, ln, "Annuler");
    return;
  }
  switch (view) {
    case VIEW_CLAUDE:
      snprintf(label, ln, "Version %u - %s", i, CLAUDE_STYLE_NAMES[i - 1]);
      if (claudeStyle == i - 1) snprintf(tag, tn, "actif");
      break;
    case VIEW_MARKET:
      snprintf(label, ln, "%s", MARKET_LIST[i - 1]);
      if (marketSel == i - 1) snprintf(tag, tn, "actif");
      break;
    case VIEW_WATER: {
      static const char *const L[] = { "", "+1 verre", "-1 verre", "Remise a zero" };
      snprintf(label, ln, "%s", L[i]);
      break;
    }
    case VIEW_POMODORO: snprintf(label, ln, "Recommencer"); break;
    default:  // Réseau
      if (i < MENU_AUTO) {
        snprintf(label, ln, "Wi-Fi %s", WIFI_NETS[i - 1].ssid);
        if (net.up && net.current == i - 1) snprintf(tag, tn, "connecte");
      } else if (i == MENU_AUTO) {
        snprintf(label, ln, "Automatique");
        if (net.forced < 0) snprintf(tag, tn, "actif");
      } else if (i == MENU_RESTART) snprintf(label, ln, "Redemarrer");
      else snprintf(label, ln, "Veille");
  }
}

static const char *const SRC_NAMES[SRC_COUNT] = { "Heure", "Claude", "Statut", "Eleven", "Bourse", "SIAM" };

// Barres de signal façon téléphone, de 0 à 4
static int signalBars(int rssi) {
  return rssi >= -55 ? 4 : rssi >= -65 ? 3 : rssi >= -75 ? 2 : rssi >= -85 ? 1 : 0;
}

static void drawSignal(int16_t x, int16_t bottom, int bars, uint16_t col) {
  for (int i = 0; i < 4; i++) {
    const int16_t h = 10 + i * 8;
    canvas.fillRect(x + i * 12, bottom - h, 9, h, i < bars ? col : COL_TRACK);
  }
}

// Durée lisible : « 12 min », « 2h14 »
static void fmtDuration(char *out, size_t n, uint32_t s) {
  if (s < 3600) snprintf(out, n, "%lu min", (unsigned long)(s / 60));
  else snprintf(out, n, "%luh%02lu", (unsigned long)(s / 3600), (unsigned long)(s / 60 % 60));
}

static void drawMenu() {
  drawHeader(VIEW_NAMES[view], "options");
  const uint8_t count = menuCount();
  const int16_t y0 = 32, rh = min(20, (H - y0 - 16) / count);
  setRole(F_BODY);
  for (uint8_t i = 0; i < count; i++) {
    char label[40], tag[12] = "";
    menuLabel(i, label, sizeof(label), tag, sizeof(tag));
    if (T->pixel) {
      toUpper(label);
      toUpper(tag);
    }
    const int16_t y = y0 + i * rh;
    const bool sel = i == menuSel;
    if (sel) canvas.fillRect(MX - 4, y, W - 2 * MX + 8, rh - 2, COL_ACCENT);
    drawLeft(label, MX, y + rh - 4, sel ? COL_BG : COL_TEXT);
    if (tag[0]) drawRight(tag, W - MX, y + rh - 4, sel ? COL_BG : COL_DIM);
  }
}

static void drawNetView() {
  char buf[48], dur[16];
  drawHeader("Reseau", net.up && net.current >= 0 ? WIFI_NETS[net.current].ssid : "hors ligne");

  // Signal : barres, puissance en dBm et qualité
  const int bars = net.up ? max(1, signalBars(net.rssi)) : 0;
  const uint16_t sig = levelColor(bars >= 3 ? 0 : bars == 2 ? 1 : 2);
  drawSignal(MX, 78, bars, sig);
  const int16_t tx = MX + 58;
  static const char *const QUALITY[] = { "", "faible", "moyen", "bon", "excellent" };
  if (net.up) {
    setRole(F_VALUE);
    snprintf(buf, sizeof(buf), "%d dBm", net.rssi);
    drawLeft(buf, tx, 60, sig);
    setRole(F_BODY);
    snprintf(buf, sizeof(buf), "%s - %s", QUALITY[bars], net.forced >= 0 ? "impose" : "auto");
    drawLeft(buf, tx, 80, COL_DIM);

    snprintf(buf, sizeof(buf), "IP %s", net.ip);
    drawLeft(buf, MX, 102, COL_DIM);
    fmtDuration(dur, sizeof(dur), (millis() - net.upSinceMs) / 1000);
    snprintf(buf, sizeof(buf), "depuis %s", dur);
    if (textWidth(buf) + textWidth(net.ip) + 40 > W - 2 * MX) snprintf(buf, sizeof(buf), "%s", dur);
    drawRight(buf, W - MX, 102, COL_DIM);
  } else {
    setRole(F_VALUE);
    drawLeft("Hors ligne", tx, 60, COL_WARN);
    setRole(F_BODY);
    if (net.trying >= 0) snprintf(buf, sizeof(buf), "essai de %s...", WIFI_NETS[net.trying].ssid);
    else snprintf(buf, sizeof(buf), "Wi-Fi coupe");
    drawLeft(buf, tx, 80, COL_DIM);
  }

  // Une pastille par source : vert = relevé réussi, orange = dernier essai en échec,
  // gris = pas encore de relevé, cercle vide = non configurée
  setRole(F_BODY);
  const int16_t cw = (W - 2 * MX) / 3;
  for (uint8_t i = 0; i < SRC_COUNT; i++) {
    const int16_t x = MX + (i % 3) * cw, base = 128 + (i / 3) * 22;
    const Source &s = srcs[i];
    if (!s.on) canvas.drawCircle(x + 5, base - 5, 4, COL_TRACK);
    else fillCircleAA(x + 5, base - 5, 4.5f, s.fail ? levelColor(1) : s.okAt ? levelColor(0) : COL_GREY);
    snprintf(buf, sizeof(buf), "%s", SRC_NAMES[i]);
    if (T->pixel) toUpper(buf);
    drawLeft(buf, x + 15, base, s.on ? COL_TEXT : COL_GREY);
  }
}

// ---------------- Vue 6 : Pomodoro ----------------
static Pomo pomoState() {
  Pomo p = {};
  const uint32_t since = millis() - pomoEnter;
  if (since < POMO_ARM_MS) {
    p.armed = true;
    p.armLeftMs = POMO_ARM_MS - since;
    p.work = true;
    p.left = p.span = POMO_WORK_S;
    p.cycle = 1;
    return p;
  }
  const uint32_t e = (since - POMO_ARM_MS) / 1000, inCycle = e % (POMO_WORK_S + POMO_BREAK_S);
  p.cycle = e / (POMO_WORK_S + POMO_BREAK_S) + 1;
  p.work = inCycle < POMO_WORK_S;
  p.span = p.work ? POMO_WORK_S : POMO_BREAK_S;
  p.left = p.work ? POMO_WORK_S - inCycle : POMO_WORK_S + POMO_BREAK_S - inCycle;
  return p;
}

static void showToast(const char *msg);  // défini plus bas, avec le bouton
static void fitText(char *s, size_t cap, int16_t maxW);

static void drawPomodoroView() {
  char buf[40];
  const Pomo p = pomoState();
  drawHeader("Pomodoro", p.armed ? "pret" : (p.work ? "travail" : "pause"));  // la phase, dans l'en-tête

  // Changement de phase : message et clignotement de la LED
  const int8_t phase = p.armed ? -1 : (int8_t)p.work;
  if (pomoLastPhase >= 0 && phase >= 0 && phase != pomoLastPhase) {
    showToast(p.work ? "Au travail !" : "Pause !");
    pomoAlertUntil = millis() + POMO_ALERT_MS;
  }
  pomoLastPhase = phase;

  const RGB k = p.work ? T->ok : T->behind;
  const uint16_t col = p.armed ? COL_DIM : c565(k.r, k.g, k.b);

  setRole(F_HUGE);
  snprintf(buf, sizeof(buf), "%02lu:%02lu", (unsigned long)(p.left / 60), (unsigned long)(p.left % 60));
  drawCenter(buf, W / 2, 112, p.armed ? COL_DIM : COL_TEXT);

  // Barre : part écoulée de la phase
  Gauge g = { (int)(100 - p.left * 100 / p.span), 0 };
  drawBar(MX + 30, 122, W - 2 * MX - 60, 8, g, -1, col);

  setRole(F_BODY);
  if (p.armed) {
    snprintf(buf, sizeof(buf), "depart dans %lu s", (unsigned long)(p.armLeftMs / 1000 + 1));
  } else if (timeOk()) {
    char t[8];
    fmtClock(t, sizeof(t), nowLocal() + p.left);
    snprintf(buf, sizeof(buf), "cycle %lu  -  fin a %s", (unsigned long)p.cycle, t);
  } else {
    snprintf(buf, sizeof(buf), "cycle %lu", (unsigned long)p.cycle);
  }
  drawCenter(buf, W / 2, 152, col);
}

// ---------------- Vue 8 : bourse ----------------
static void drawMarketView() {
  char buf[48];
  const bool ok = mk.have && millis() - mk.rxMs < MARKET_STALE_MS;
  drawHeader(mk.have ? mk.symbol : "Bourse", mk.have ? (mk.open ? "en direct" : "ferme") : "");

  if (!mk.have) {
    setRole(F_SUB);
    drawCenter(srcs[SRC_MARKET].on ? "releve en cours..." : "aucun cours", W / 2, 90, COL_GREY);
    setRole(F_BODY);
    if (!srcs[SRC_MARKET].on) drawCenter("MARKET_SYMBOLS dans secrets.h", W / 2, 116, COL_DIM);
    else if (srcs[SRC_MARKET].fail) drawCenter("dernier essai en echec", W / 2, 116, COL_DIM);
    return;
  }

  const uint16_t up = c565(34, 170, 90), down = c565(220, 60, 60);
  const uint16_t trend = !ok ? COL_GREY : (mk.pct >= 0 ? up : down);

  // Nom, puis cours en grand et variation
  // Variation à droite, sur la ligne du nom : le cours garde toute la largeur
  setRole(F_VALUE);
  char pct[12];
  snprintf(pct, sizeof(pct), "%+.2f%%", mk.pct);
  drawRight(pct, W - MX, 52, trend);
  const int16_t pctW = textWidth(pct);
  setRole(F_BODY);
  snprintf(buf, sizeof(buf), "%s", mk.name);
  fitText(buf, sizeof(buf), W - 2 * MX - pctW - 10);
  drawLeft(buf, MX, 50, COL_DIM);
  setRole(F_BIG);
  drawLeft(mk.price, MX, 94, ok ? COL_TEXT : COL_GREY);
  const int16_t pw = textWidth(mk.price);
  setRole(F_LABEL);
  drawLeft(mk.currency, MX + pw + 6, 94, COL_DIM);

  // Courbe de la séance, plus bas et plus haut à droite
  if (mk.n >= 2) {
    const int16_t gx = MX, gy = 104, gw = W - 2 * MX - 64, gh = 46;
    canvas.drawFastHLine(gx, gy + gh, gw, COL_TRACK);
    int16_t px = gx, py = gy + gh - mk.pts[0] * gh / 99;
    for (uint8_t i = 1; i < mk.n; i++) {
      const int16_t x = gx + (int32_t)i * (gw - 1) / (mk.n - 1);
      const int16_t y = gy + gh - mk.pts[i] * gh / 99;
      canvas.drawLine(px, py, x, y, trend);
      canvas.drawLine(px, py + 1, x, y + 1, trend);  // trait de 2 px
      px = x;
      py = y;
    }
    fillCircleAA(px, py, 3, trend);
    setRole(F_SMALL);
    canvas.setTextColor(COL_DIM);
    canvas.setCursor(W - MX - 56, gy + 2);
    canvas.print("haut");
    canvas.setCursor(W - MX - 56, gy + 12);
    canvas.setTextColor(COL_TEXT);
    canvas.print(mk.high);
    canvas.setTextColor(COL_DIM);
    canvas.setCursor(W - MX - 56, gy + gh - 18);
    canvas.print("bas");
    canvas.setTextColor(COL_TEXT);
    canvas.setCursor(W - MX - 56, gy + gh - 8);
    canvas.print(mk.low);
  }
}

// ---------------- Annonce PALais SIAM ----------------
// Identité du site, indépendante du thème : fond jaune curry à pois, trait encre,
// violet orchidée, et Chef PAL (version simplifiée du SVG du site, même repère 120x132).
const uint16_t SIAM_CURRY = c565(255, 201, 60), SIAM_DOT = c565(232, 176, 44), SIAM_INK = c565(22, 18, 31);
const uint16_t SIAM_ORCHID = c565(123, 47, 247), SIAM_SKIN = c565(249, 217, 187), SIAM_EAR = c565(246, 207, 173);

// Disque cerclé d'encre, comme le trait épais du dessin d'origine
static void inkDisc(int16_t x, int16_t y, float r, uint16_t col) {
  fillCircleAA(x, y, r + 2.5f, SIAM_INK);
  fillCircleAA(x, y, r, col);
}

static void drawChef(int16_t ox, int16_t oy) {
  // Veste et col
  inkDisc(ox + 60, oy + 142, 44, COL_WHITE);
  canvas.fillTriangle(ox + 47, oy + 98, ox + 73, oy + 98, ox + 60, oy + 112, SIAM_ORCHID);
  fillCircleAA(ox + 60, oy + 119, 2.4f, SIAM_INK);
  // Tête
  inkDisc(ox + 34, oy + 72, 5.5f, SIAM_EAR);
  inkDisc(ox + 86, oy + 72, 5.5f, SIAM_EAR);
  inkDisc(ox + 60, oy + 71, 26, SIAM_SKIN);
  fillCircleAA(ox + 44, oy + 80, 3.5f, c565(252, 176, 170));  // joues
  fillCircleAA(ox + 76, oy + 80, 3.5f, c565(252, 176, 170));
  // Yeux plissés de joie
  for (int8_t d = 0; d < 3; d++) {
    canvas.drawLine(ox + 45, oy + 67 + d, ox + 50, oy + 61 + d, SIAM_INK);
    canvas.drawLine(ox + 50, oy + 61 + d, ox + 55, oy + 67 + d, SIAM_INK);
    canvas.drawLine(ox + 65, oy + 67 + d, ox + 70, oy + 61 + d, SIAM_INK);
    canvas.drawLine(ox + 70, oy + 61 + d, ox + 75, oy + 67 + d, SIAM_INK);
  }
  // Bouche ouverte, nez, puis moustache par-dessus
  fillCircleAA(ox + 60, oy + 88, 8, SIAM_INK);
  fillCircleAA(ox + 60, oy + 88, 5.5f, c565(122, 35, 56));
  canvas.fillRect(ox + 50, oy + 78, 21, 10, SIAM_SKIN);
  fillCircleAA(ox + 60, oy + 75, 3.5f, c565(238, 180, 143));
  const uint16_t brown = c565(91, 58, 41);
  fillCircleAA(ox + 53, oy + 81, 5.5f, brown);
  fillCircleAA(ox + 67, oy + 81, 5.5f, brown);
  fillCircleAA(ox + 45, oy + 83, 3.5f, brown);
  fillCircleAA(ox + 75, oy + 83, 3.5f, brown);
  // Toque : trois boules et un bandeau, contour encre commun
  inkDisc(ox + 38, oy + 31, 11, COL_WHITE);
  inkDisc(ox + 82, oy + 31, 11, COL_WHITE);
  inkDisc(ox + 60, oy + 21, 14, COL_WHITE);
  canvas.fillRoundRect(ox + 33, oy + 33, 54, 17, 4, SIAM_INK);
  canvas.fillRoundRect(ox + 36, oy + 36, 48, 11, 3, COL_WHITE);
  fillCircleAA(ox + 38, oy + 31, 11, COL_WHITE);  // efface les traits intérieurs
  fillCircleAA(ox + 82, oy + 31, 11, COL_WHITE);
  fillCircleAA(ox + 60, oy + 21, 14, COL_WHITE);
  // Orchidée sur la toque
  for (int i = 0; i < 5; i++) {
    const float a = i * 2 * PI / 5;
    fillCircleAA(ox + 75 + sinf(a) * 5, oy + 41 - cosf(a) * 5, 3.6f, SIAM_ORCHID);
  }
  fillCircleAA(ox + 75, oy + 41, 2.6f, SIAM_CURRY);
}

// Coupe le texte à la largeur disponible, en terminant par « ... »
static void fitText(char *s, size_t cap, int16_t maxW) {
  if (textWidth(s) <= maxW) return;
  char base[72];
  snprintf(base, sizeof(base), "%s", s);
  for (size_t n = strlen(base); n > 0; n--) {
    base[n] = 0;
    snprintf(s, cap, "%s...", base);
    if (textWidth(s) <= maxW) return;
  }
}

static void siamBackground();
static void siamFrame();

static bool orderActive() {
  return orderUntil && millis() < orderUntil;
}

static void drawOrderView() {
  char buf[72];
  siamBackground();
  drawChef(4, H - 124);

  pxSize = 0;
  const int16_t tx = 136, tw = W - tx - 12;
  canvas.setTextSize(1);
  canvas.setFont(&FreeSansBold24pt7b);
  snprintf(buf, sizeof(buf), "+%d", orderCount);
  drawLeft(buf, tx, 60, SIAM_INK);
  canvas.setFont(&FreeSansBold12pt7b);
  drawLeft(orderCount > 1 ? "COMMANDES" : "COMMANDE", tx, 88, SIAM_ORCHID);

  canvas.setFont(&FreeSansBold9pt7b);
  snprintf(buf, sizeof(buf), "%s", orderNames);
  fitText(buf, sizeof(buf), tw);
  drawLeft(buf, tx, 120, SIAM_INK);
  if (orderDish[0]) {
    canvas.setFont(&FreeSans9pt7b);
    snprintf(buf, sizeof(buf), "%s", orderDish);
    fitText(buf, sizeof(buf), tw);
    drawLeft(buf, tx, 142, SIAM_INK);
  }

  // Cadre encre et barre du temps restant
  siamFrame();
  const uint32_t left = orderActive() ? orderUntil - millis() : 0;
  canvas.fillRect(4, H - 10, (int32_t)(W - 8) * left / ORDER_MS, 6, SIAM_INK);
}

// ---------------- Vue 7 : PALais SIAM ----------------
// Fond curry à pois et cadre encre, communs à la vue et à l'annonce
static void siamBackground() {
  canvas.fillScreen(SIAM_CURRY);
  for (int16_t y = 8; y < H; y += 16)
    for (int16_t x = 8 + (y / 16 % 2) * 8; x < W; x += 16) canvas.fillCircle(x, y, 1, SIAM_DOT);
}

static void siamFrame() {
  for (int8_t i = 0; i < 4; i++) canvas.drawRect(i, i, W - 2 * i, H - 2 * i, SIAM_INK);
}

// Carte blanche cerclée d'encre, ombre portée décalée comme sur le site
static void siamCard(int16_t x, int16_t y, int16_t w, int16_t h) {
  canvas.fillRoundRect(x + 4, y + 4, w, h, 8, SIAM_INK);
  canvas.fillRoundRect(x, y, w, h, 8, SIAM_INK);
  canvas.fillRoundRect(x + 3, y + 3, w - 6, h - 6, 6, COL_WHITE);
}

// Pastille blanche cerclée d'encre (total de commandes)
static void siamPill(const char *txt, int16_t x, int16_t y) {
  canvas.setFont(&FreeSansBold9pt7b);
  const int16_t tw = textWidth(txt);
  canvas.fillRoundRect(x, y, tw + 20, 26, 13, SIAM_INK);
  canvas.fillRoundRect(x + 2, y + 2, tw + 16, 22, 11, COL_WHITE);
  drawLeft(txt, x + 10, y + 18, SIAM_INK);
}

// Indicateur de vue, à l'encre
static void siamDots() {
  for (uint8_t i = 0; i < VIEW_COUNT; i++) {
    const int16_t x = W - MX - 5 - (VIEW_COUNT - 1 - i) * 10;
    if (i == view) fillCircleAA(x, H - 18, 3, SIAM_INK);
    else canvas.drawCircle(x, H - 18, 3, SIAM_INK);
  }
}

static void drawSiamView() {
  char buf[48];
  siamBackground();
  pxSize = 0;
  canvas.setTextSize(1);

  // Cuisine fermée (semaine close ou heure limite passée) : le Chef et l'état
  const bool open = sm.have && sm.open && (!timeOk() || nowLocal() < sm.deadline);
  if (!open) {
    drawChef(4, H - 124);
    canvas.setFont(&FreeSansBold24pt7b);
    if (sm.have) {
      drawLeft("Cuisine", 136, 62, SIAM_INK);
      drawLeft("fermee", 136, 104, SIAM_ORCHID);
      snprintf(buf, sizeof(buf), "%d au total", sm.total);
      siamPill(buf, 136, 122);
    } else {
      canvas.setFont(&FreeSansBold9pt7b);
      drawLeft("Connexion au site...", 140, 96, SIAM_INK);
    }
    siamDots();
    siamFrame();
    return;
  }

  // Logo : « PAL » en pastille violette, puis « ais SIAM »
  canvas.setFont(&FreeSansBold12pt7b);
  const int16_t palW = textWidth("PAL");
  canvas.fillRoundRect(12, 9, palW + 12, 26, 5, SIAM_INK);
  canvas.fillRoundRect(14, 11, palW + 8, 22, 4, SIAM_ORCHID);
  drawLeft("PAL", 18, 30, COL_WHITE);
  canvas.setFont(&FreeSansBold9pt7b);
  drawLeft("ais SIAM", 30 + palW, 29, SIAM_INK);

  // Heure de fermeture, seule en haut à droite
  canvas.setFont(&FreeSansBold12pt7b);
  fmtClock(buf, sizeof(buf), sm.deadline);
  drawRight(buf, W - 14, 30, SIAM_INK);

  // Commandes de la semaine, en très gros
  canvas.setFont(&FreeSansBold24pt7b);
  canvas.setTextSize(2);
  snprintf(buf, sizeof(buf), "%d", sm.orders);
  drawLeft(buf, 14, 112, SIAM_INK);
  canvas.setTextSize(1);
  canvas.setFont(&FreeSansBold9pt7b);
  snprintf(buf, sizeof(buf), "commande%s", sm.orders > 1 ? "s" : "");
  drawLeft(buf, 16, 132, SIAM_ORCHID);

  // Carte devinette : nombre de joueurs
  const int16_t cx = 204, cy = 44, cw = W - cx - 16, ch = 92;
  siamCard(cx, cy, cw, ch);
  canvas.setFont(&FreeSans9pt7b);
  drawCenter("devinette", cx + cw / 2, cy + 22, SIAM_ORCHID);
  canvas.setFont(&FreeSansBold24pt7b);
  snprintf(buf, sizeof(buf), "%d", sm.guesses);
  drawCenter(buf, cx + cw / 2, cy + 62, SIAM_INK);
  canvas.setFont(&FreeSans9pt7b);
  drawCenter(sm.guesses > 1 ? "joueurs" : "joueur", cx + cw / 2, cy + 82, SIAM_INK);

  snprintf(buf, sizeof(buf), "%d au total", sm.total);
  siamPill(buf, 12, 140);
  const int16_t pw = textWidth(buf);  // police de la pastille
  canvas.setFont(&FreeSans9pt7b);
  snprintf(buf, sizeof(buf), "%d payee%s", sm.paid, sm.paid > 1 ? "s" : "");
  drawLeft(buf, 12 + pw + 30, 158, SIAM_INK);
  siamDots();
  siamFrame();
}

// ---------------- Vue Eau ----------------
static bool waterActive() {
  return waterUntil && millis() < waterUntil;
}

// Début de la plage de rappels du jour, en heure locale à plat
static uint32_t waterStart(uint32_t now) {
  return now / 86400 * 86400 + WATER_FROM_H * 3600UL;
}

// Verres attendus à cette heure : un au début de la plage, puis un par intervalle
static uint8_t waterExpected(uint32_t now) {
  if (now < waterStart(now)) return 0;
  return min((uint32_t)WATER_TARGET, (now - waterStart(now)) / WATER_STEP_S + 1);
}

static void waterSave() {
  prefs.putUChar("wgl", waterGlasses);
  prefs.putUInt("wday", waterDay);
}

static void waterRollDay(uint32_t now) {
  if (!now || now / 86400 == waterDay) return;
  waterDay = now / 86400;
  waterGlasses = 0;
  waterSave();
}

static void waterAdd(int8_t d) {
  waterRollDay(nowLocal());
  waterGlasses = constrain((int)waterGlasses + d, 0, 40);
  waterSave();
  waterUntil = 0;
  if (d > 0) waterSnoozeMs = 0;
  char msg[32];
  snprintf(msg, sizeof(msg), "Eau : %u / %u verres", waterGlasses, WATER_TARGET);
  showToast(msg);
}

// Prochain verre : « prochain verre vers 14:00 », « objectif atteint »
static void waterFooter(char *out, size_t n) {
  const uint32_t now = nowLocal();
  if (waterGlasses >= WATER_TARGET) snprintf(out, n, "Objectif atteint, bravo");
  else if (!now) snprintf(out, n, "Reglage de l'heure...");
  else {
    const uint32_t next = waterStart(now) + waterGlasses * WATER_STEP_S;
    char t[8];
    fmtClock(t, sizeof(t), next);
    if (next <= now) snprintf(out, n, "Un verre maintenant");
    else snprintf(out, n, "Prochain verre vers %s", t);
  }
}

// Verre stylisé : trapèze, rempli s'il est bu
static void drawGlass(int16_t x, int16_t y, bool full, uint16_t blue) {
  const int16_t wt = 18, wb = 13, h = 24, dx = (wt - wb) / 2;
  if (full) {
    canvas.fillTriangle(x, y, x + wt, y, x + dx, y + h, blue);
    canvas.fillTriangle(x + wt, y, x + dx, y + h, x + dx + wb, y + h, blue);
  }
  const uint16_t edge = full ? blue : COL_DIM;
  canvas.drawLine(x, y, x + dx, y + h, edge);
  canvas.drawLine(x + wt, y, x + dx + wb, y + h, edge);
  canvas.drawFastHLine(x + dx, y + h, wb + 1, edge);
}

static void drawWaterView() {
  char buf[32];
  const uint16_t blue = c565(40, 140, 230);
  waterRollDay(nowLocal());
  snprintf(buf, sizeof(buf), "objectif %d.%d L", WATER_GOAL_ML / 1000, WATER_GOAL_ML % 1000 / 100);
  drawHeader("Eau", waterActive() ? "c'est l'heure !" : buf);

  // Quantité bue en grand, l'objectif à côté
  const uint32_t ml = (uint32_t)waterGlasses * WATER_GLASS_ML;
  setRole(F_BIG);
  snprintf(buf, sizeof(buf), "%lu.%02lu", (unsigned long)(ml / 1000), (unsigned long)(ml % 1000 / 10));
  drawLeft(buf, MX, 84, waterActive() ? blue : COL_TEXT);
  const int16_t bw = textWidth(buf);
  setRole(F_LABEL);
  drawLeft("L", MX + bw + 6, 84, COL_DIM);

  // Un verre par rappel de la journée
  const uint8_t shown = min(WATER_TARGET, 8);
  for (uint8_t i = 0; i < shown; i++) drawGlass(MX + i * 28, 104, i < waterGlasses, blue);
  if (waterGlasses > shown) {
    setRole(F_BODY);
    snprintf(buf, sizeof(buf), "+%u", waterGlasses - shown);
    drawLeft(buf, MX + shown * 28, 126, blue);
  }

  // Bouteille qui se remplit, pourcentage dedans
  const int16_t bx = W - MX - 50, by = 46, bwid = 48, bh = 92;
  canvas.fillRoundRect(bx + 16, by - 10, 16, 12, 3, COL_DIM);  // bouchon
  canvas.drawRoundRect(bx, by, bwid, bh, 10, COL_DIM);
  const int16_t fill = (int32_t)(bh - 6) * min(ml, (uint32_t)WATER_GOAL_ML) / WATER_GOAL_ML;
  if (fill > 0) canvas.fillRoundRect(bx + 3, by + bh - 3 - fill, bwid - 6, fill, 7, blue);
  setRole(F_LABEL);
  snprintf(buf, sizeof(buf), "%lu%%", (unsigned long)(ml * 100 / WATER_GOAL_ML));
  drawCenter(buf, bx + bwid / 2, by + bh / 2 + 6, COL_TEXT);
}

// Déclenche le rappel quand on est en retard sur le rythme, dans la plage horaire
static void waterTick() {
  static uint32_t last = 0;
  if (WATER_GOAL_ML <= 0 || waterActive() || millis() - last < 10000) return;
  last = millis();
  const uint32_t now = nowLocal();
  if (!now) return;
  waterRollDay(now);
  const uint32_t h = now / 3600 % 24;
  if (h < WATER_FROM_H || h >= WATER_TO_H || waterGlasses >= waterExpected(now)) return;
  if (waterSnoozeMs && (int32_t)(millis() - waterSnoozeMs) < 0) return;
  waterUntil = millis() + WATER_SHOW_MS;
  waterSnoozeMs = waterUntil + WATER_SNOOZE_MS;
  dirty = true;
}

// ---------------- Vue Home ----------------
// Logo du site : M D M A en couleur d'accent, qui se déploient en
// « MiniDisplay / MiniAutomations » à l'arrivée sur la vue.
static void drawWord(int16_t x, int16_t base, const char *a, const char *ra, const char *b, const char *rb, float t) {
  char buf[16];
  const char *parts[4] = { a, ra, b, rb };
  for (uint8_t k = 0; k < 4; k++) {
    const bool initial = k % 2 == 0;
    snprintf(buf, sizeof(buf), "%s", parts[k]);
    if (!initial) buf[(size_t)(strlen(buf) * t + 0.5f)] = 0;  // lettres révélées
    if (T->pixel) toUpper(buf);
    drawLeft(buf, x, base, initial ? COL_ACCENT : COL_TEXT);
    x += textWidth(buf);
  }
}

// Marque du site : petit écran sombre, barre d'accent
static void brandMark(int16_t x, int16_t y) {
  canvas.fillRoundRect(x, y, 34, 21, 5, c565(58, 58, 70));
  canvas.fillRoundRect(x + 3, y + 3, 28, 15, 3, c565(17, 17, 17));
  canvas.fillRoundRect(x + 7, y + 8, 14, 5, 2, COL_ACCENT);
}

static void drawHomeView() {
  const uint32_t since = millis() - homeEnter;
  float t = since < 400 ? 0 : (float)(since - 400) / (HOME_ANIM_MS - 400);
  t = t > 1 ? 1 : t;
  t = 1 - (1 - t) * (1 - t) * (1 - t);  // ralentit à l'arrivée, comme sur le site

  if (T->pixel) setRole(F_VALUE);
  else {
    pxSize = 0;
    canvas.setFont(&FreeSansBold18pt7b);
    canvas.setTextSize(1);
  }
  drawWord(MX + 4, 62, "M", "ini", "D", "isplay", t);
  drawWord(MX + 4, 104, "M", "ini", "A", "utomations", t);

  // Marque, thème courant et pastille d'état
  brandMark(MX + 4, 116);
  char buf[32];
  setRole(F_LABEL);
  snprintf(buf, sizeof(buf), "%s", T->name);
  if (T->pixel) toUpper(buf);
  drawLeft(buf, MX + 48, 133, COL_DIM);
  statusDot(W - MX - 6, 127, 5);
}

static void render() {
  if (sleeping) return;
  Guard g;  // la tâche réseau ne modifie rien pendant le dessin
  dirty = false;
  const bool water = waterActive() && !orderActive();  // rappel : vue Eau par-dessus
  if (orderActive() || (view == VIEW_SIAM && !water)) {  // identité du site, quel que soit le thème
    if (orderActive()) drawOrderView();
    else drawSiamView();
    tft.drawRGBBitmap(0, 0, canvas.getBuffer(), W, H);
    return;
  }
  canvas.fillScreen(COL_BG);
  if (T->fx == FX_RAIN) drawRain();
  if (water) drawWaterView();
  else if (menuOpen) drawMenu();
  else switch (view) {
    case VIEW_HOME: drawHomeView(); break;
    case VIEW_CLAUDE:
      if (claudeStyle == 1) drawBigView();
      else if (claudeStyle == 2) drawClockView();
      else drawGaugesView();
      break;
    case VIEW_WATER: drawWaterView(); break;
    case VIEW_ELEVEN: drawElevenView(); break;
    case VIEW_NET: drawNetView(); break;
    case VIEW_POMODORO: drawPomodoroView(); break;
    case VIEW_MARKET: drawMarketView(); break;
    default: drawGaugesView(); break;
  }
  drawFooter();
  if (T->fx != FX_NONE) drawScanlines();
  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), W, H);
}

// LED RGB : même nuance que la jauge 5 h, éteinte sans données
static void updateLed() {
  uint32_t c = 0;
  Guard g;
  if (sleeping) {
    // veille : LED éteinte
  } else if (orderActive()) {  // commande : clignote en jaune curry
    if ((millis() / 250) % 2 == 0) c = rgb.Color(255, 170, 0);
  } else if (waterActive()) {  // rappel d'eau : clignote en bleu
    if ((millis() / 500) % 2 == 0) c = rgb.Color(40, 140, 230);
  } else if (view == VIEW_POMODORO) {  // Pomodoro : couleur de la phase, clignote au changement
    const Pomo p = pomoState();
    const RGB k = p.work ? T->ok : T->behind;
    const bool blinkOff = millis() < pomoAlertUntil && (millis() / 250) % 2;
    if (!p.armed && !blinkOff) c = rgb.Color(k.r, k.g, k.b);
  } else if (!isStale() && st.five.used >= 0) {
    const RGB k = gaugeRGB(st.five, windowFraction(st.five, 5));
    c = rgb.Color(k.r, k.g, k.b);
  }
  rgb.setPixelColor(0, c);
  rgb.show();
}

// ---------------- Bouton BOOT ----------------
static void showToast(const char *msg) {
  Guard g;  // appelé aussi par la tâche réseau
  snprintf(toast, sizeof(toast), "%s", msg);
  toastUntil = millis() + TOAST_MS;
  dirty = true;
}

static void nextView() {
  view = (view + 1) % VIEW_COUNT;
  if (view == VIEW_HOME) homeEnter = millis();
  prefs.putUChar("view", view);
  if (view == VIEW_POMODORO) {  // chaque arrivée relance le compte à rebours de 5 s
    pomoEnter = millis();
    pomoLastPhase = -1;
  }
  render();
}

static void nextTheme() {
  themeIndex = (themeIndex + 1) % THEME_COUNT;
  T = &THEMES[themeIndex];
  prefs.putUChar("theme", themeIndex);
  char msg[32];
  snprintf(msg, sizeof(msg), "Theme %s", T->name);
  showToast(msg);
  render();
  updateLed();
}

// Réglage de luminosité, mis de côté : réactivable avec LONG_PRESS_BRIGHTNESS
static void nextBrightness() {
  blIndex = (blIndex + 1) % (sizeof(BL_LEVELS) / sizeof(BL_LEVELS[0]));
  ledcWrite(PIN_LCD_BL, BL_LEVELS[blIndex]);
  prefs.putUChar("bl", blIndex);
  char msg[32];
  snprintf(msg, sizeof(msg), "Luminosite %s", BL_NAMES[blIndex]);
  showToast(msg);
  render();
}

// Veille : écran et LED éteints, Wi-Fi coupé par la tâche réseau. La carte reste
// alimentée par l'USB ; un appui sur BOOT la réveille.
static void enterSleep() {
  Guard g;
  sleeping = true;
  rgb.setPixelColor(0, 0);
  rgb.show();
  tft.fillScreen(ST77XX_BLACK);
  ledcWrite(PIN_LCD_BL, 0);
}

static void wake() {
  while (digitalRead(PIN_BOOT) == LOW) delay(10);  // l'appui de réveil ne change pas de vue
  sleeping = false;
  ledcWrite(PIN_LCD_BL, BL_LEVELS[blIndex]);
  showToast("Reveil, connexion Wi-Fi...");
  render();
  updateLed();
}

static void runMenu(uint8_t i) {
  char msg[40];
  if (i == 0) return;  // Annuler
  if (view == VIEW_CLAUDE) {
    claudeStyle = i - 1;
    prefs.putUChar("cst", claudeStyle);
    return;
  }
  if (view == VIEW_WATER) {
    if (i == 3) waterAdd(-(int8_t)waterGlasses);
    else waterAdd(i == 1 ? 1 : -1);
    return;
  }
  if (view == VIEW_POMODORO) {
    pomoEnter = millis();
    pomoLastPhase = -1;
    return;
  }
  if (view == VIEW_MARKET) {
    if (marketSel == i - 1) return;
    marketSel = i - 1;
    prefs.putUChar("mkt", marketSel);
    {
      Guard g;
      mk.have = false;  // pas d'ancien cours sous le nouveau nom
    }
    marketChanged = true;
    snprintf(msg, sizeof(msg), "Cours %s", MARKET_LIST[marketSel]);
    showToast(msg);
    return;
  }
  if (i < MENU_AUTO) {
    wantNet = i - 1;
    snprintf(msg, sizeof(msg), "Connexion a %s...", WIFI_NETS[i - 1].ssid);
    showToast(msg);
  } else if (i == MENU_AUTO) {
    wantNet = -1;
    showToast("Wi-Fi automatique");
  } else if (i == MENU_RESTART) {
    {
      Guard g;
      canvas.fillScreen(COL_BG);
      setRole(F_TITLE);
      drawCenter("Redemarrage...", W / 2, H / 2 + 8, COL_TEXT);
      tft.drawRGBBitmap(0, 0, canvas.getBuffer(), W, H);
    }
    delay(500);
    ESP.restart();
  } else {
    enterSleep();
  }
}

// Appui court (relâché avant 700 ms) : vue suivante. Appui long, déclenché dès les
// 700 ms atteintes : thème suivant sur la vue Home, options ailleurs. Dans les options,
// la sélection descend d'une ligne toutes les 0,7 s tant qu'on reste appuyé, et le
// relâchement la valide.
static void pollButton() {
  static bool down = false, longDone = false;
  static uint32_t t0 = 0, lastChange = 0, menuStep = 0;
  const bool pressed = digitalRead(PIN_BOOT) == LOW;
  const uint32_t now = millis();

  if (pressed != down && now - lastChange > 30) {  // anti-rebond
    lastChange = now;
    down = pressed;
    if (down) {
      t0 = now;
      longDone = false;
    } else if (menuOpen) {
      menuOpen = false;
      runMenu(menuSel);
      render();
    } else if (!longDone) {
      if (orderActive()) {  // un appui ferme l'annonce sans changer de vue
        orderUntil = 0;
        render();
        updateLed();
      } else if (waterActive()) {
        waterAdd(1);
        render();
        updateLed();
      } else {
        nextView();
      }
    }
  }
  if (down && !longDone && now - t0 >= LONG_PRESS_MS) {
    longDone = true;
    if (orderActive() || waterActive()) {
      // une annonce est à l'écran : l'appui long ne fait rien
    } else if (view == VIEW_HOME) {
      if (LONG_PRESS_BRIGHTNESS) nextBrightness();
      else nextTheme();
    } else if (!menuCount()) {
      showToast("Pas d'options ici");
    } else {
      menuOpen = true;
      menuSel = 0;  // « Annuler » : un appui trop court ne déclenche rien
      menuStep = now;
      render();
    }
  }
  if (menuOpen && down && now - menuStep >= MENU_STEP_MS) {
    menuStep = now;
    menuSel = (menuSel + 1) % menuCount();
    render();
  }
}

// ---------------- Annonce PALais SIAM ----------------
// Nouvelle commande, signalée par la tâche réseau : pendant une annonce, les
// commandes s'additionnent.
static void announceOrder(int count, const char *names, const char *dish) {
  if (count <= 0) return;
  Guard g;
  if (orderActive()) {
    orderCount += count;
    orderDish[0] = 0;  // plusieurs commandes : plus de plat unique à afficher
  } else {
    orderCount = count;
    snprintf(orderDish, sizeof(orderDish), "%s", dish);
  }
  snprintf(orderNames, sizeof(orderNames), "%s", names);
  orderStart = millis();
  orderUntil = orderStart + ORDER_MS;
  dirty = true;
}

void netSetup();  // reseau.ino

void setup() {
  Serial.begin(115200);  // journal, lisible en USB à la maison
  pinMode(PIN_BOOT, INPUT_PULLUP);
  gLock = xSemaphoreCreateRecursiveMutex();

  prefs.begin("mdma", false);
  view = prefs.getUChar("view", VIEW_HOME) % VIEW_COUNT;
  claudeStyle = prefs.getUChar("cst", 0) % CLAUDE_STYLES;
  waterGlasses = prefs.getUChar("wgl", 0);
  waterDay = prefs.getUInt("wday", 0);
  themeIndex = prefs.getUChar("theme", THEME_DEFAULT) % THEME_COUNT;
  T = &THEMES[themeIndex];
  blIndex = prefs.getUChar("bl", 0) % (sizeof(BL_LEVELS) / sizeof(BL_LEVELS[0]));
  marketSel = prefs.getUChar("mkt", 0) % MARKET_COUNT;

  ledcAttach(PIN_LCD_BL, 5000, 8);  // rétroéclairage en PWM 5 kHz, 8 bits
  ledcWrite(PIN_LCD_BL, BL_LEVELS[blIndex]);

  SPI.begin(PIN_SCLK, PIN_MISO, PIN_MOSI);
  tft.init(172, 320);
  tft.setSPISpeed(40000000);
  tft.setRotation(1);  // paysage 320x172
  tft.fillScreen(ST77XX_BLACK);

  rgb.begin();
  rgb.setBrightness(25);

  if (!canvas.getBuffer()) {  // 110 Ko : ne devrait pas arriver sans PSRAM non plus
    tft.setTextColor(ST77XX_RED);
    tft.setCursor(10, 10);
    tft.print("Memoire insuffisante");
    while (true) delay(1000);
  }
  netSetup();  // sources configurées, puis tâche réseau (Wi-Fi et relevés)
  pomoEnter = homeEnter = millis();
  render();
  updateLed();
}

void loop() {
  if (sleeping) {
    if (digitalRead(PIN_BOOT) == LOW) wake();
    delay(20);
    return;
  }
  pollButton();
  waterTick();

  // Rafraîchit le repère de temps, les nuances des jauges et la vue horloge,
  // et efface le message éphémère une fois expiré
  static uint32_t lastDraw = 0;
  static bool toastShown = false;
  const bool toastActive = millis() < toastUntil;
  // Pomodoro : secondes à l'écran, et LED qui clignote pendant l'alerte
  static bool orderShown = false;
  const bool orderNow = orderActive() || waterActive();
  // Annonce : barre de temps et LED animées, puis retour à la vue dès la fin
  const bool homeAnim = view == VIEW_HOME && millis() - homeEnter < HOME_ANIM_MS + 100;
  const uint32_t every = orderNow ? 100 : homeAnim ? 30 : view == VIEW_POMODORO ? (millis() < pomoAlertUntil ? 250 : 1000) : 5000;
  if (dirty || millis() - lastDraw >= every || (toastShown && !toastActive) || (orderShown && !orderNow)) {
    lastDraw = millis();
    render();
    updateLed();
  }
  toastShown = toastActive;
  orderShown = orderNow;
  delay(10);
}
