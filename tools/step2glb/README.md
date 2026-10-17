# step2glb

Convertit le modèle CAO officiel de la carte (STEP, fourni par Waveshare) en GLB pour l'explorateur 3D du site.

```sh
cd tools/step2glb
npm install
node step2glb.mjs ESP32-C6-LCD-1.47.stp ../../site/assets/models/esp32-c6-lcd-1.47.glb
node step2glb.mjs ESP32-C6-LCD-1.47.stp --list   # inventaire des pièces (nom, centre, couleur)
```

- Source : `ESP32-C6-LCD-1.47-3D_Drawing.rar` sur le [wiki Waveshare](https://www.waveshare.com/wiki/ESP32-C6-LCD-1.47). Sous Windows 11, l'archive s'extrait avec `tar -xf`.
- Le fichier STEP (10,9 Mo) n'est pas versionné. Seul le GLB produit (4,7 Mo) l'est.
- Tessellation par [occt-import-js](https://github.com/kovacsv/occt-import-js), c'est-à-dire OpenCascade en WebAssembly.
- Chaque pièce devient un nœud glTF avec un matériau par couleur. Son nom d'origine est conservé dans `extras.part`, que three.js expose dans `userData.part`. C'est ainsi que le site retrouve la puce, l'écran, les boutons…
- Repère du modèle : X = largeur, Y = longueur (USB-C vers −Y), Z = épaisseur (écran vers +Z). Le site le fait pivoter dans son propre repère.
