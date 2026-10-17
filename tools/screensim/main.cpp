// Banc d'essai PC : compile le vrai firmware et écrit chaque écran (thème x vue) en PPM.
#include "Arduino.h"
uint32_t g_millis = 1000000;
#include "claude_usage.ino"

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
  st.have = true;
  st.pcNow = 1792247520;  // samedi 17 octobre 2026, 14:32
  st.rxMs = g_millis;
  st.five = { 47, 1792256580 };
  st.week = { 29, 1792692000 };
  st.level = 0;
  st.updated = st.pcNow - 60;
  el.have = true;
  el.used = 18135;
  el.limit = 37472;
  el.reset = 1793865876;
  el.updated = st.pcNow - 120;
  pc.have = true; pc.rxMs = g_millis; pc.cpu = 23; pc.ramUsed = 18630; pc.ramTotal = 31850; pc.gpu = 64; pc.gpuTemp = 71;
  sm.have = true; sm.date = 1792713600; sm.deadline = 1792749600; sm.open = true;  // vendredi 23 octobre, 10:00
  sm.orders = 12; sm.paid = 9; sm.guesses = 7; sm.correct = -1; sm.total = 317;
  snprintf(sm.recent, sizeof(sm.recent), "Camille, Lucas, Sam");
  pomoEnter = g_millis - POMO_ARM_MS - (6 * 60 + 36) * 1000UL;  // Pomodoro : 18:24 restantes
  for (uint8_t t = 0; t < THEME_COUNT; t++) {
    T = &THEMES[t];
    for (uint8_t v = 0; v < VIEW_COUNT; v++) {
      view = v;
      render();
      char p[512];
      snprintf(p, sizeof(p), "%s/t%d_v%d.ppm", out, t, v);
      dump(p);
    }
  }
  // Annonce d'une commande PALais SIAM, 1,5 s après réception
  T = &THEMES[THEME_DEFAULT];
  orderCount = 1;
  snprintf(orderNames, sizeof(orderNames), "Camille");
  snprintf(orderDish, sizeof(orderDish), "Poulet curry vert");
  orderStart = g_millis - 1500;
  orderUntil = orderStart + ORDER_MS;
  render();
  char p[512];
  snprintf(p, sizeof(p), "%s/order.ppm", out);
  dump(p);
  // Vue PALais SIAM, cuisine fermée
  orderUntil = 0;
  view = VIEW_SIAM;
  sm.open = false;
  render();
  snprintf(p, sizeof(p), "%s/siam_closed.ppm", out);
  dump(p);
  return 0;
}
