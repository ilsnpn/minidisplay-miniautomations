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
  bool have = false;  // au moins un relevé de quota réussi
  Gauge five = { -1, 0 };
  Gauge week = { -1, 0 };
  int level = -1;
  uint32_t updated = 0;  // heure du dernier relevé de quota réussi
  char err[72] = "";
};

// Crédits ElevenLabs (optionnels)
struct Eleven {
  bool have = false;     // clé configurée dans secrets.h
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
  uint16_t barBg, barFg;  // barre de titre (en-têtes Ubuntu, macOS)
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

// Réseau Wi-Fi déclaré dans secrets.h
struct WifiNet {
  const char *ssid;
  const char *pass;
};

// État de la connexion Wi-Fi (vue Réseau)
struct NetInfo {
  bool up = false;
  int8_t current = -1;  // réseau connecté (index dans WIFI_NETS), -1 = aucun
  int8_t trying = -1;   // réseau en cours d'essai
  int8_t forced = -1;   // réseau imposé depuis le menu, -1 = automatique
  int rssi = 0;         // dBm
  char ip[16] = "";
  uint32_t upSinceMs = 0, downSinceMs = 0;  // millis() de la connexion ou de la perte
};

// Sources de données, pour les pastilles de la vue Réseau
enum SrcId : uint8_t { SRC_TIME, SRC_CLAUDE, SRC_STATUS, SRC_ELEVEN, SRC_MARKET, SRC_SIAM, SRC_COUNT };
struct Source {
  bool on;         // configurée dans secrets.h
  uint32_t okAt;   // heure locale du dernier succès, 0 = jamais
  bool fail;       // le dernier essai a échoué
};

// Résumé PALais SIAM de la semaine en cours (chaque minute)
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

// Cours de bourse ou crypto (chaque minute)
struct Market {
  bool have = false;
  uint32_t rxMs = 0;       // millis() du relevé
  char symbol[12] = "";    // IART, BTC...
  char name[40] = "";
  char price[16] = "";     // déjà formaté
  float pct = 0;           // variation en %
  char currency[6] = "";
  bool open = false;       // marché ouvert (toujours vrai en crypto)
  uint32_t updated = 0;    // heure locale du relevé
  char low[16] = "", high[16] = "";
  uint8_t pts[64];         // courbe, 0..99
  uint8_t n = 0;
};
