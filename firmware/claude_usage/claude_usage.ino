// Claude Usage — Waveshare ESP32-C6-LCD-1.47
// Affiche le quota Claude (session 5 h + semaine) reçu du PC par l'USB.
// Le PC envoie une ligne toutes les ~30 s (voir bridge/MiniDisplayBridge.ps1) :
//   Q;<now>;<five%>;<fiveReset>;<week%>;<weekReset>;<level>;<updated>;<erreur>
// Heures en secondes « heure locale du PC » depuis 1970, -1 / 0 = inconnu.
// level : état de status.claude.com (-1 injoignable, 0 ok, 1 dégradé, 2 panne).
// Optionnel : E;<utilises>;<limite>;<reset>;<updated>;<erreur> pour les crédits
// ElevenLabs, affichés dans une 4e vue.
//   P;<cpu%>;<ramUtiliseeMo>;<ramTotaleMo>;<gpu%>;<gpuTemp> : ressources du PC (5e vue).
// 6e vue : minuteur Pomodoro, autonome.
//   S;<date>;<limite>;<ouvert>;<commandes>;<payees>;<devinettes>;<bonnes>;<total>;<derniers>
//   7e vue : la semaine PALais SIAM, toujours aux couleurs du site quel que soit le thème.
//   M;<symbole>;<nom>;<cours>;<variation%>;<devise>;<ouvert>;<releve>;<min>;<max>;<points>
//   8e vue : cours de bourse ou crypto, avec la courbe de la séance.
//   N;<nombre>;<noms>;<plat> : nouvelle commande sur PALais SIAM, annoncée 5 s
//   par-dessus la vue en cours (aux couleurs du site), puis retour à la vue.
//
// Bouton BOOT (GPIO9) : appui court = vue suivante, appui long = thème suivant.
// Vue, thème et luminosité sont mémorisés en Flash (NVS).

#include <SPI.h>
#include <time.h>
#include <Preferences.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_NeoPixel.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include "state.h"

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
#define STALE_MS 120000UL  // au-delà, le PC est considéré déconnecté
#define LONG_PRESS_MS 700  // appui long sur BOOT
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
enum View : uint8_t { VIEW_GAUGES, VIEW_BIG, VIEW_CLOCK, VIEW_ELEVEN, VIEW_PC, VIEW_POMODORO, VIEW_SIAM, VIEW_MARKET, VIEW_COUNT };

// Pomodoro : 25 min de travail, 5 min de pause, en boucle. Démarre après 5 s passées
// sur la vue, s'arrête dès qu'on la quitte. Tourne sur la carte seule, sans le PC.
#define POMO_ARM_MS 5000UL
#define POMO_WORK_S (25 * 60UL)
#define POMO_BREAK_S (5 * 60UL)
#define POMO_ALERT_MS 4000UL  // clignotement de la LED à chaque changement de phase
static uint32_t pomoEnter = 0;     // millis() à l'arrivée sur la vue
static int8_t pomoLastPhase = -1;  // pour détecter le passage travail <-> pause
static uint32_t pomoAlertUntil = 0;
static uint8_t view = VIEW_GAUGES;

const uint8_t BL_LEVELS[] = { 255, 110, 35 };  // 100 %, ~45 %, ~15 %
const char *const BL_NAMES[] = { "100%", "45%", "15%" };
static uint8_t blIndex = 0;

// Annonce d'une nouvelle commande PALais SIAM (trame N)
#define ORDER_MS 5000UL
static uint32_t orderStart = 0, orderUntil = 0;
static int orderCount = 0;
static char orderNames[64] = "", orderDish[40] = "";

static char toast[40] = "";
static uint32_t toastUntil = 0;

// ---------------- État reçu ----------------
State st;
Eleven el;
PcStats pc;
SiamSummary sm;
Market mk;
#define MARKET_STALE_MS 600000UL  // sans relevé depuis 10 min : cours grisé
#define PC_STALE_MS 15000UL  // sans trame P depuis 15 s : valeurs masquées

static uint32_t nowLocal() {
  return st.pcNow + (millis() - st.rxMs) / 1000;
}

static bool isStale() {
  return !st.have || millis() - st.rxMs > STALE_MS;
}

// Part écoulée de la fenêtre (0..1), ou -1 si l'heure de reset est inconnue.
static float windowFraction(const Gauge &g, uint32_t hours) {
  if (!g.reset || !st.have) return -1;
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
  gmtime_r(&tt, &tm);  // t est déjà en heure locale du PC
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

// Pied de page : état de la liaison à gauche, indicateur de vue à droite
static void drawFooter() {
  char buf[80];
  uint16_t col = COL_GREY;
  if (millis() < toastUntil) {
    snprintf(buf, sizeof(buf), "%s", toast);
    col = COL_TEXT;
  } else if (!st.have) {
    snprintf(buf, sizeof(buf), "En attente du PC (lancez le pont USB)...");
    col = COL_WARN;
  } else if (view == VIEW_POMODORO) {
    buf[0] = 0;  // le Pomodoro n'a pas besoin du PC
  } else if (view == VIEW_PC) {
    snprintf(buf, sizeof(buf), "Mise a jour toutes les 3 s");
  } else if (view == VIEW_MARKET) {
    if (mk.have && mk.updated) {
      char t[8];
      fmtClock(t, sizeof(t), mk.updated);
      snprintf(buf, sizeof(buf), "%s - releve a %s", mk.open ? "Marche ouvert" : "Marche ferme", t);
    } else {
      buf[0] = 0;
    }
  } else if (isStale()) {
    snprintf(buf, sizeof(buf), "PC deconnecte depuis %lu min", (unsigned long)((millis() - st.rxMs) / 60000));
    col = COL_WARN;
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
  uint32_t seed = st.have ? nowLocal() / 5 : millis() / 5000;
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
  if (g.reset && st.have) {
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
  if (g.reset && st.have) {
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
  if (st.have) fmtClock(buf, sizeof(buf), nowLocal());
  else snprintf(buf, sizeof(buf), "--:--");
  drawCenter(buf, W / 2, 104, isStale() ? COL_GREY : COL_TEXT);

  setRole(F_SUB);
  if (st.have) {
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
    if (!el.have) drawCenter("cle API dans bridge/secrets.env", W / 2, 116, COL_DIM);
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

// ---------------- Vue 5 : ressources du PC ----------------
// Une ligne : libellé (avec détail) à gauche, valeur à droite, barre fine dessous.
// Couleur normale jusqu'à 60 %, puis glisse vers la couleur d'alerte du thème à 90 %.
static void drawPcRow(int16_t top, const char *label, int pct, const char *value, int alertFrom, int alertTo, int level) {
  RGB k = T->ok;
  if (level >= 0) {
    float t = (float)(level - alertFrom) / (alertTo - alertFrom);
    k = lerpRGB(T->ok, T->ahead, t < 0 ? 0 : (t > 1 ? 1 : t));
  }
  const uint16_t col = pct < 0 ? COL_GREY : c565(k.r, k.g, k.b);
  setRole(F_LABEL);
  drawLeft(label, MX, top + 14, COL_DIM);
  setRole(F_VALUE);
  drawRight(value, W - MX, top + 16, col);
  Gauge g = { pct, 0 };
  drawBar(MX, top + 22, W - 2 * MX, 10, g, -1, col);
}

static void drawPcView() {
  char lab[32], val[16];
  drawHeader("PC", "ressources");
  const bool ok = pc.have && millis() - pc.rxMs < PC_STALE_MS;

  if (ok && pc.cpu >= 0) snprintf(val, sizeof(val), "%d%%", pc.cpu); else snprintf(val, sizeof(val), "--");
  drawPcRow(34, "PROCESSEUR", ok ? pc.cpu : -1, val, 60, 90, ok ? pc.cpu : -1);

  const int ram = ok && pc.ramTotal > 0 ? (int)((int64_t)pc.ramUsed * 100 / pc.ramTotal) : -1;
  if (ram >= 0) snprintf(lab, sizeof(lab), "MEMOIRE  %ld/%ld Go", (long)((pc.ramUsed + 512) / 1024), (long)((pc.ramTotal + 512) / 1024));
  else snprintf(lab, sizeof(lab), "MEMOIRE");
  snprintf(val, sizeof(val), ram >= 0 ? "%d%%" : "--", ram);
  drawPcRow(78, lab, ram, val, 60, 90, ram);

  // GPU : la couleur suit la température (normale jusqu'à 70 °C, alerte à 85 °C)
  const int gpu = ok ? pc.gpu : -1;
  if (ok && pc.gpuTemp >= 0) snprintf(lab, sizeof(lab), "GPU  %d C", pc.gpuTemp); else snprintf(lab, sizeof(lab), "GPU");
  snprintf(val, sizeof(val), gpu >= 0 ? "%d%%" : "--", gpu);
  drawPcRow(122, lab, gpu, val, 70, 85, ok ? pc.gpuTemp : -1);
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
  } else if (st.have) {
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
    drawCenter("aucun cours", W / 2, 90, COL_GREY);
    setRole(F_BODY);
    drawCenter("MARKET_SYMBOL dans bridge/settings.env", W / 2, 116, COL_DIM);
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
  const bool open = sm.have && sm.open && (!st.have || nowLocal() < sm.deadline);
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

static void render() {
  if (orderActive() || view == VIEW_SIAM) {  // identité du site, quel que soit le thème
    if (orderActive()) drawOrderView();
    else drawSiamView();
    tft.drawRGBBitmap(0, 0, canvas.getBuffer(), W, H);
    return;
  }
  canvas.fillScreen(COL_BG);
  if (T->fx == FX_RAIN) drawRain();
  switch (view) {
    case VIEW_BIG: drawBigView(); break;
    case VIEW_CLOCK: drawClockView(); break;
    case VIEW_ELEVEN: drawElevenView(); break;
    case VIEW_PC: drawPcView(); break;
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
  if (orderActive()) {  // commande : clignote en jaune curry
    if ((millis() / 250) % 2 == 0) c = rgb.Color(255, 170, 0);
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
  snprintf(toast, sizeof(toast), "%s", msg);
  toastUntil = millis() + TOAST_MS;
}

static void nextView() {
  view = (view + 1) % VIEW_COUNT;
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

// Appui court (relâché avant 700 ms) : vue suivante. Appui long : thème suivant,
// déclenché dès les 700 ms atteintes, sans attendre le relâchement.
static void pollButton() {
  static bool down = false, longDone = false;
  static uint32_t t0 = 0, lastChange = 0;
  const bool pressed = digitalRead(PIN_BOOT) == LOW;
  const uint32_t now = millis();

  if (pressed != down && now - lastChange > 30) {  // anti-rebond
    lastChange = now;
    down = pressed;
    if (down) {
      t0 = now;
      longDone = false;
    } else if (!longDone) {
      if (orderActive()) {  // un appui ferme l'annonce sans changer de vue
        orderUntil = 0;
        render();
        updateLed();
      } else {
        nextView();
      }
    }
  }
  if (down && !longDone && now - t0 >= LONG_PRESS_MS) {
    longDone = true;
    if (LONG_PRESS_BRIGHTNESS) nextBrightness();
    else nextTheme();
  }
}

// ---------------- Liaison série ----------------
// E;<utilises>;<limite>;<reset>;<updated>;<erreur>
static void handleEleven(char *line) {
  char *f[6];
  int n = 0;
  char *p = line;
  while (n < 6) {
    f[n++] = p;
    char *sep = strchr(p, ';');
    if (!sep) break;
    *sep = 0;
    p = sep + 1;
  }
  if (n < 5) return;
  el.used = atol(f[1]);
  el.limit = atol(f[2]);
  el.reset = strtoul(f[3], NULL, 10);
  el.updated = strtoul(f[4], NULL, 10);
  snprintf(el.err, sizeof(el.err), "%s", n > 5 ? f[5] : "");
  el.have = true;
  Serial.println("OK");
}

// N;<nombre>;<noms>;<plat> : pendant une annonce, les commandes s'additionnent
static void handleOrder(char *line) {
  char *f[4];
  int n = 0;
  char *p = line;
  while (n < 4) {
    f[n++] = p;
    char *sep = strchr(p, ';');
    if (!sep) break;
    *sep = 0;
    p = sep + 1;
  }
  if (n < 3) return;
  const int count = atoi(f[1]);
  if (count <= 0) return;
  if (orderActive()) {
    orderCount += count;
    orderDish[0] = 0;  // plusieurs commandes : plus de plat unique à afficher
  } else {
    orderCount = count;
    snprintf(orderDish, sizeof(orderDish), "%s", n > 3 ? f[3] : "");
  }
  snprintf(orderNames, sizeof(orderNames), "%s", f[2]);
  orderStart = millis();
  orderUntil = orderStart + ORDER_MS;
}

// S;<date>;<limite>;<ouvert>;<commandes>;<payees>;<devinettes>;<bonnes>;<total>;<derniers>
static void handleSummary(char *line) {
  char *f[10];
  int n = 0;
  char *p = line;
  while (n < 10) {
    f[n++] = p;
    char *sep = strchr(p, ';');
    if (!sep) break;
    *sep = 0;
    p = sep + 1;
  }
  if (n < 9) return;
  sm.date = strtoul(f[1], NULL, 10);
  sm.deadline = strtoul(f[2], NULL, 10);
  sm.open = atoi(f[3]) == 1;
  sm.orders = atoi(f[4]);
  sm.paid = atoi(f[5]);
  sm.guesses = atoi(f[6]);
  sm.correct = atoi(f[7]);
  sm.total = atoi(f[8]);
  snprintf(sm.recent, sizeof(sm.recent), "%s", n > 9 ? f[9] : "");
  sm.have = true;
}

// M;<symbole>;<nom>;<cours>;<variation%>;<devise>;<ouvert>;<releve>;<min>;<max>;<points>
static void handleMarket(char *line) {
  char *f[11];
  int n = 0;
  char *p = line;
  while (n < 11) {
    f[n++] = p;
    char *sep = strchr(p, ';');
    if (!sep) break;
    *sep = 0;
    p = sep + 1;
  }
  if (n < 10) return;
  snprintf(mk.symbol, sizeof(mk.symbol), "%s", f[1]);
  snprintf(mk.name, sizeof(mk.name), "%s", f[2]);
  snprintf(mk.price, sizeof(mk.price), "%s", f[3]);
  mk.pct = atof(f[4]);
  snprintf(mk.currency, sizeof(mk.currency), "%s", f[5]);
  mk.open = atoi(f[6]) == 1;
  mk.updated = strtoul(f[7], NULL, 10);
  snprintf(mk.low, sizeof(mk.low), "%s", f[8]);
  snprintf(mk.high, sizeof(mk.high), "%s", f[9]);
  mk.n = 0;
  for (char *q = n > 10 ? f[10] : NULL; q && *q && mk.n < sizeof(mk.pts);) {
    mk.pts[mk.n++] = (uint8_t)atoi(q);
    q = strchr(q, ',');
    if (q) q++;
  }
  mk.rxMs = millis();
  mk.have = true;
}

static void handleLine(char *line) {
  if (line[0] == '?') {
    Serial.println("MINIDISPLAY claude_usage 1");
    return;
  }
  if (line[0] == 'P' && line[1] == ';') {  // P;cpu;ramUsed;ramTotal;gpu;gpuTemp (pas d'accusé)
    long v[5] = { -1, -1, -1, -1, -1 };
    char *p = line + 2;
    for (int i = 0; i < 5 && p; i++) {
      v[i] = strtol(p, NULL, 10);
      p = strchr(p, ';');
      if (p) p++;
    }
    pc.cpu = v[0]; pc.ramUsed = v[1]; pc.ramTotal = v[2]; pc.gpu = v[3]; pc.gpuTemp = v[4];
    pc.rxMs = millis();
    pc.have = true;
    return;
  }
  if (line[0] == 'M' && line[1] == ';') {
    handleMarket(line);
    return;
  }
  if (line[0] == 'S' && line[1] == ';') {
    handleSummary(line);
    return;
  }
  if (line[0] == 'N' && line[1] == ';') {
    handleOrder(line);
    return;
  }
  if (line[0] == 'E' && line[1] == ';') {
    handleEleven(line);
    return;
  }
  if (line[0] != 'Q' || line[1] != ';') return;

  char *f[9];
  int n = 0;
  char *p = line;
  while (n < 9) {  // découpage qui conserve les champs vides
    f[n++] = p;
    char *sep = strchr(p, ';');
    if (!sep) break;
    *sep = 0;
    p = sep + 1;
  }
  if (n < 8) return;

  st.pcNow = strtoul(f[1], NULL, 10);
  st.rxMs = millis();
  st.five.used = atoi(f[2]);
  st.five.reset = strtoul(f[3], NULL, 10);
  st.week.used = atoi(f[4]);
  st.week.reset = strtoul(f[5], NULL, 10);
  st.level = atoi(f[6]);
  st.updated = strtoul(f[7], NULL, 10);
  snprintf(st.err, sizeof(st.err), "%s", n > 8 ? f[8] : "");
  st.have = true;
  Serial.println("OK");
}

static void pollSerial() {
  static char line[400];
  static size_t len = 0;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (len) {
        line[len] = 0;
        handleLine(line);
        len = 0;
        render();
        updateLed();
      }
    } else if (len < sizeof(line) - 1) {
      line[len++] = c;
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BOOT, INPUT_PULLUP);

  prefs.begin("mdma", false);
  view = prefs.getUChar("view", VIEW_GAUGES) % VIEW_COUNT;
  themeIndex = prefs.getUChar("theme", THEME_DEFAULT) % THEME_COUNT;
  T = &THEMES[themeIndex];
  blIndex = prefs.getUChar("bl", 0) % (sizeof(BL_LEVELS) / sizeof(BL_LEVELS[0]));

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
  pomoEnter = millis();
  render();
  updateLed();
}

void loop() {
  pollSerial();
  pollButton();

  // Rafraîchit le repère de temps, les nuances des jauges et la vue horloge,
  // et efface le message éphémère une fois expiré
  static uint32_t lastDraw = 0;
  static bool toastShown = false;
  const bool toastActive = millis() < toastUntil;
  // Pomodoro : secondes à l'écran, et LED qui clignote pendant l'alerte
  static bool orderShown = false;
  const bool orderNow = orderActive();
  // Annonce : barre de temps et LED animées, puis retour à la vue dès la fin
  const uint32_t every = orderNow ? 100 : view == VIEW_POMODORO ? (millis() < pomoAlertUntil ? 250 : 1000) : 5000;
  if (millis() - lastDraw >= every || (toastShown && !toastActive) || (orderShown && !orderNow)) {
    lastDraw = millis();
    render();
    updateLed();
  }
  toastShown = toastActive;
  orderShown = orderNow;
  delay(10);
}
