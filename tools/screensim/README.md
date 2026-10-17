# screensim

Rend les écrans du firmware en images pour le site, **au pixel près** : c'est le vrai `firmware/claude_usage/claude_usage.ino` qui dessine, compilé sur PC.

```sh
python tools/screensim/render.py
```

- `stub/` imite le matériel (écran, LED, USB, Flash) juste assez pour compiler ; seul le canevas de dessin (Adafruit GFX) est réel.
- `main.cpp` injecte des données de démonstration (samedi 17 octobre, 14:32) puis enregistre chaque thème × vue.
- Le compilateur Zig est installé par pip dans `.zig/` au premier lancement. Pillow et la bibliothèque Adafruit GFX sont nécessaires.
- Sortie : `site/assets/themes/<theme>-<vue>.png`, agrandies ×2 sans lissage.

Relancez-le après toute retouche du dessin dans le firmware.
