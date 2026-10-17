# Site de présentation et de documentation

Site statique, sans étape de build. **Double-cliquez sur `index.html`** : il fonctionne directement en `file://`, et se publiera tel quel (Vercel, GitHub Pages…).

Public visé : toute personne qui veut comprendre, réexpliquer ou refaire le projet. Le site est conçu pour être partagé publiquement.

## Pages

| Page | Contenu |
|---|---|
| `index.html` | présentation, explorateur 3D, fonctionnement de l'écran, matériel, galerie |
| `architecture.html` | la chaîne complète, les rythmes, pourquoi le PC fait le travail réseau, les pannes |
| `pont.html` | le pont PowerShell : jeton, API, rate-limit, heures, protocole série, détection USB |
| `firmware.html` | le firmware : écran, rendu hors écran, réception, horloge, calcul des jauges, pièges |
| `materiel.html` | la carte : bus SPI, GPIO, strapping, USB natif, alimentation |
| `guide.html` | refaire le projet pas à pas depuis un PC vierge |
| `boitiers.html` | acheter la carte, boîtiers imprimés en 3D, fichiers CAO |

## Fichiers

| Fichier | Rôle |
|---|---|
| `style.css` | styles communs |
| `docs.css` | styles des pages de documentation (prose, code, schémas, guide, journal) |
| `js/data.js` | textes des broches et composants de l'explorateur 3D |
| `js/screen.js` | reproduction de l'écran du firmware (maquette 2D + écran 3D) |
| `js/docs.js` | coloration du code, schémas Mermaid, sommaire actif, boutons « Copier » |
| `assets/` | photos (copies de `docs/images/` + retour visuel) |

Bibliothèques chargées par CDN : three.js 0.160 (jsDelivr), highlight.js 11.9 (cdnjs) et Mermaid 10.9 (jsDelivr).

## Maintenir la documentation

- Les **extraits de code** des pages `pont.html` et `firmware.html` sont copiés à la main depuis `bridge/` et `firmware/`. Il faut les mettre à jour quand le code change.
- Les **schémas** sont soit du SVG écrit à la main (classes `d-*` dans `docs.css`), soit du Mermaid (`<pre class="mermaid">`).
- En Mermaid, pas de `;` dans les messages : c'est un séparateur d'instructions.

## Explorateur 3D

- Le modèle est le **modèle CAO officiel Waveshare** (`assets/models/esp32-c6-lcd-1.47.glb`), converti du STEP par `tools/step2glb`.
- Le GLB ne se charge pas en `file://` : en double-clic local, un modèle simplifié construit en code le remplace. Pour voir le vrai modèle en local : `python -m http.server` dans `site/`.
- Les broches, l'écran animé et les zones cliquables absentes du modèle (régulateur) sont ajoutés par le code.
- Des liens directs ouvrent la vue sur un élément précis : `index.html?view=bottom&select=pin:12`, `index.html?select=comp:esp32`.
