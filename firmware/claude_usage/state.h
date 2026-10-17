// Types partagés : dans un en-tête pour que les prototypes générés par Arduino les voient.
#pragma once
#include <stdint.h>

struct RGB {
  uint8_t r, g, b;
};

struct Gauge {
  int used;        // %, -1 = inconnu
  uint32_t reset;  // heure locale du reset, 0 = inconnue
};

struct State {
  bool have = false;
  uint32_t pcNow = 0;  // horloge du PC au moment de la réception
  uint32_t rxMs = 0;   // millis() à la réception
  Gauge five = { -1, 0 };
  Gauge week = { -1, 0 };
  int level = -1;
  uint32_t updated = 0;  // heure du dernier relevé de quota réussi
  char err[72] = "";
};

// Crédits ElevenLabs (trame E, optionnelle)
struct Eleven {
  bool have = false;     // au moins une trame E reçue
  int32_t used = -1;     // crédits consommés sur la période, -1 = inconnu
  int32_t limit = -1;    // crédits de la période
  uint32_t reset = 0;    // heure locale de la remise à zéro
  uint32_t updated = 0;  // heure du dernier relevé réussi
  char err[48] = "";
};

// Thème d'affichage : palette et style des éléments (voir THEMES dans le .ino)
struct Theme {
  const char *name;
  uint16_t bg, track, text, dim, accent, grey, warn;
  uint16_t barBg, barFg;  // barre de titre (en-têtes DOS, Windows, Ubuntu, macOS)
  RGB ok, ahead, behind;  // jauge : dans le rythme, en avance, en retard
  bool pixel;             // police pixel intégrée plutôt que FreeSans
  uint8_t header, bar, fx;
};

// État du minuteur Pomodoro (vue 5), calculé à chaque dessin
struct Pomo {
  bool armed;          // encore dans les 5 s d'attente
  uint32_t armLeftMs;  // temps avant le départ
  bool work;           // phase de travail (sinon pause)
  uint32_t left;       // secondes restantes dans la phase
  uint32_t span;       // durée de la phase (s)
  uint32_t cycle;      // numéro du cycle, à partir de 1
};

// Ressources du PC (trame P, toutes les 3 s)
struct PcStats {
  bool have = false;
  uint32_t rxMs = 0;  // millis() à la réception
  int cpu = -1, gpu = -1, gpuTemp = -1;  // %, %, °C ; -1 = inconnu
  int32_t ramUsed = -1, ramTotal = -1;   // Mo
};

// Résumé PALais SIAM de la semaine en cours (trame S, chaque minute)
struct SiamSummary {
  bool have = false;
  uint32_t date = 0;      // jour de la commande (heure locale, minuit)
  uint32_t deadline = 0;  // limite de commande (heure locale)
  bool open = false;      // semaine ouverte aux commandes
  int orders = 0, paid = 0, guesses = 0;
  int correct = -1;       // bonnes réponses, -1 tant que la solution est cachée
  int total = 0;          // commandes depuis le début
  char recent[64] = "";   // dernières personnes à avoir commandé
};
