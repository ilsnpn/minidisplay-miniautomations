# Firmware

| Sketch | Rôle |
|---|---|
| `hello_world/` | test de l'écran et de la LED RGB |
| `claude_usage/` | **le projet** : affiche le quota Claude reçu du PC (voir [bridge/](../bridge/README.md)) |

Outils : `arduino-cli`, installé dans `%LOCALAPPDATA%\arduino-cli\`, avec le core `esp32:esp32` 3.3.12 et les bibliothèques Adafruit ST7789, GFX et NeoPixel.

```sh
A="$LOCALAPPDATA/arduino-cli/arduino-cli.exe"
FQBN="esp32:esp32:esp32c6:CDCOnBoot=cdc"
powershell -ExecutionPolicy Bypass -File bridge/Stop-MiniDisplayBridge.ps1   # libère le port COM
"$A" compile --fqbn "$FQBN" firmware/claude_usage
"$A" upload  --fqbn "$FQBN" -p COM3 firmware/claude_usage
```

Si le port n'apparaît pas : maintenez BOOT, appuyez sur RESET, relâchez RESET puis BOOT.

## Écran `claude_usage` (paysage 320×172)

- **En-tête** : « Claude usage », l'heure du PC et une pastille d'état des services (vert, orange, rouge, ou gris si le site est injoignable).
- **Deux jauges**, la session de 5 h puis la semaine. Chacune affiche le pourcentage consommé, une barre et un **trait blanc de repère de temps** qui marque où l'on devrait être si la consommation suivait le rythme de la fenêtre. Sous la barre figurent l'heure du reset et le compte à rebours.
- **Couleur d'une jauge** (même règle que le widget) : selon l'écart entre la consommation et le repère, bleu jusqu'à 20 points, vert jusqu'à 35, rouge au-delà.
- **Vue ElevenLabs** (4ᵉ vue, optionnelle) : crédits restants en grand, pourcentage restant, barre avec repère de rythme sur le mois, crédits utilisés sur la limite et date du reset. Sans clé configurée dans le pont, elle affiche « non configure ».
- **PC** (5ᵉ vue) : processeur, mémoire, charge et température de la carte graphique NVIDIA, rafraîchis toutes les 3 s par le pont. Les jauges passent à la couleur d'alerte du thème au-delà de 60 % (90 % au maximum), et la température GPU au-delà de 70 °C.
- **Pomodoro** (6ᵉ vue) : 25 min de travail, 5 min de pause, en boucle. Le minuteur part 5 s après l'arrivée sur la vue et s'arrête dès qu'on la quitte. Il tourne sur la carte seule, sans le PC. À chaque changement de phase, un message s'affiche et la LED clignote.
- **PALais SIAM** (7ᵉ vue) : la semaine de commandes en cours, toujours aux couleurs du site quel que soit le thème. Commandes de la semaine en très gros, joueurs de la devinette, heure de fermeture, total historique et paiements. Quand la cuisine est fermée (semaine close ou heure limite passée), la vue affiche « Cuisine fermée ».
- **Action** (8ᵉ vue) : cours d'une action ou d'une crypto (BTC, ETH, SOL), variation du jour et courbe de la séance, aux couleurs du thème. Le symbole se règle dans `bridge/settings.env`.
- **Annonce de commande** : à chaque trame `N`, une annonce PALais SIAM (Chef PAL, « +1 COMMANDE », nom et plat) passe 5 s par-dessus n'importe quelle vue, LED clignotante en jaune. Un appui sur BOOT la ferme.
- **LED RGB** : elle prend la couleur de la jauge 5 h et s'éteint sans données.
- **Pied de page** : l'heure du dernier relevé, ou bien une erreur (jeton expiré, 429, PC déconnecté…).

## Bouton BOOT (GPIO9)

- **Appui court** : vue suivante, parmi jauges, gros chiffres, horloge, crédits ElevenLabs, PC, Pomodoro, PALais SIAM et action. Huit points en bas à droite indiquent la vue affichée.
- **Appui long** (0,7 s) : thème suivant, parmi Matrix, MS-DOS, Windows 98, Ubuntu, Claude Code (par défaut), macOS clair et Aperture Science. Les infos restent les mêmes, seul l'habillage change (couleurs, police pixel, barres, en-tête).
- Le réglage de luminosité est conservé dans le code mais désactivé : passez `LONG_PRESS_BRIGHTNESS` à 1 pour le rendre à l'appui long.
- Vue, thème et luminosité sont mémorisés en Flash (NVS, espace de noms `mdma`).
