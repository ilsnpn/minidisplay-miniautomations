# Vidéo de présentation

Vidéo de 20 s en 1920 × 1080, écrite en code avec [Remotion](https://www.remotion.dev). Elle tourne en boucle en haut de l'accueil du site (`site/assets/mdma.mp4`).

```sh
cd tools/video
npm install
npm run studio   # aperçu interactif dans le navigateur
npm run render   # produit out/mdma.mp4, à copier dans site/assets/
```

- `src/MDMA.jsx` : le montage (nom MDMA, arrivée de la carte en 3D, défilé des 7 thèmes, carton final) et les bruitages.
- `src/Board.jsx` : le modèle CAO Waveshare, avec l'écran rendu par le vrai firmware plaqué dessus.
- `make_sfx.py` : bruitages synthétisés (whoosh, bips, glitch, impact, carillon). Aucun son externe, donc aucun droit à gérer.
- `prepare.mjs` : copie le modèle 3D et les écrans depuis `site/assets/` (régénérés par `tools/screensim`).
