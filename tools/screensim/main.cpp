// Banc d'essai PC : compile le vrai firmware (V2, mdma_autonome.ino sans reseau.ino)
// et écrit chaque écran (thème x vue) en PPM. Données de démonstration fictives.
#include "Arduino.h"
uint32_t g_millis = 100000000;

// Heure simulée : samedi 17 octobre 2026, 14:32 (déjà locale, voir stub/Arduino.h)
static time_t sim_time(time_t *t) {
  const time_t v = 1792247520;
  if (t) *t = v;
  return v;
}
#define time(x) sim_time(x)

#include "mdma_autonome.ino"

void netSetup() {}  // la tâche réseau n'existe pas dans le banc d'essai

static void dump(const char *path) {
  FILE *f = fopen(path, "wb");
  fprintf(f, "P6\n%d %d\n255\n", W, H);
  const uint16_t *b = canvas.getBuffer();
  for (int i = 0; i < W * H; i++) {
    const uint16_t c = b[i];
    const uint8_t px[3] = { (uint8_t)(((c >> 11) & 0x1F) * 255 / 31), (uint8_t)(((c >> 5) & 0x3F) * 255 / 63), (uint8_t)((c & 0x1F) * 255 / 31) };
    fwrite(px, 1, 3, f);
  }
  fclose(f);
}

int main(int argc, char **argv) {
  const char *out = argc > 1 ? argv[1] : ".";
  const uint32_t now = nowLocal();
  st.have = true;
  st.five = { 47, 1792256580 };
  st.week = { 29, 1792692000 };
  st.level = 0;
  st.updated = now - 60;
  el.have = true;
  el.used = 18135;
  el.limit = 37472;
  el.reset = 1793865876;
  el.updated = now - 120;
  sm.have = true; sm.date = 1792713600; sm.deadline = 1792749600; sm.open = true;  // vendredi 23 octobre, 10:00
  sm.orders = 12; sm.paid = 9; sm.guesses = 7; sm.correct = -1; sm.total = 317;
  snprintf(sm.recent, sizeof(sm.recent), "Camille, Lucas, Sam");

  // Bourse : IART, séance terminée
  snprintf(mk.symbol, sizeof(mk.symbol), "IART");
  snprintf(mk.name, sizeof(mk.name), "Integra LifeSciences");
  snprintf(mk.price, sizeof(mk.price), "13.21");
  mk.pct = -2.37f;
  snprintf(mk.currency, sizeof(mk.currency), "USD");
  mk.open = false;
  mk.updated = now - 120;
  snprintf(mk.low, sizeof(mk.low), "13.11");
  snprintf(mk.high, sizeof(mk.high), "13.81");
  static const uint8_t PTS[] = { 99, 93, 95, 93, 86, 90, 85, 85, 85, 85, 74, 78, 62, 55, 53, 45, 43, 41, 31, 33, 31,
                                 24, 17, 14, 18, 17, 6, 0, 1, 0, 22, 16, 12, 13, 12, 21, 15, 12, 9, 14, 14 };
  mk.n = sizeof(PTS);
  memcpy(mk.pts, PTS, sizeof(PTS));
  mk.rxMs = g_millis;
  mk.have = true;

  // Réseau : partage de connexion du téléphone (noms fictifs de secrets.h.example)
  net.up = true;
  net.current = 0;
  net.forced = -1;
  net.rssi = -58;
  snprintf(net.ip, sizeof(net.ip), "172.20.10.4");
  net.upSinceMs = g_millis - (2 * 3600 + 14 * 60) * 1000UL;
  for (uint8_t i = 0; i < SRC_COUNT; i++) srcs[i] = { true, now - 60, false };

  waterGlasses = 3;  // 0,75 L sur 1,5 L
  waterDay = now / 86400;
  pomoEnter = g_millis - POMO_ARM_MS - (6 * 60 + 36) * 1000UL;  // Pomodoro : 18:24 restantes
  for (uint8_t t = 0; t < THEME_COUNT; t++) {
    T = &THEMES[t];
    // Images numérotées comme sur le site : les quatre versions Claude, puis les autres vues
    uint8_t n = 0;
    for (uint8_t v = 0; v < VIEW_COUNT; v++) {
      view = v;
      for (uint8_t k = 0; k < (v == VIEW_CLAUDE ? CLAUDE_STYLES : 1); k++) {
        claudeStyle = k;
        render();
        char p[512];
        snprintf(p, sizeof(p), "%s/t%d_v%d.ppm", out, t, n++);
        dump(p);
      }
    }
    claudeStyle = 0;
  }
  char p[512];
  // Menu de la vue Réseau, sélection sur « Automatique »
  T = &THEMES[THEME_DEFAULT];
  view = VIEW_NET;
  menuOpen = true;
  menuSel = MENU_AUTO;
  render();
  snprintf(p, sizeof(p), "%s/menu.ppm", out);
  dump(p);
  menuOpen = false;
  // Annonce d'une commande PALais SIAM, 1,5 s après réception
  orderCount = 1;
  snprintf(orderNames, sizeof(orderNames), "Camille");
  snprintf(orderDish, sizeof(orderDish), "Poulet curry vert");
  orderStart = g_millis - 1500;
  orderUntil = orderStart + ORDER_MS;
  render();
  snprintf(p, sizeof(p), "%s/order.ppm", out);
  dump(p);
  // Rappel d'eau, 12 s après son déclenchement
  orderUntil = 0;
  waterUntil = g_millis + WATER_SHOW_MS - 12000;
  render();
  snprintf(p, sizeof(p), "%s/water.ppm", out);
  dump(p);
  waterUntil = 0;
  // Vue PALais SIAM, cuisine fermée
  view = VIEW_SIAM;
  sm.open = false;
  render();
  snprintf(p, sizeof(p), "%s/siam_closed.ppm", out);
  dump(p);
  return 0;
}
