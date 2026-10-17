// Copie le modèle 3D et les écrans « chiffres » du site dans public/ avant un rendu.
import { cpSync, mkdirSync } from 'node:fs';
const site = new URL('../../site/assets/', import.meta.url);
const pub = new URL('./public/', import.meta.url);
mkdirSync(new URL('models/', pub), { recursive: true });
mkdirSync(new URL('screens/', pub), { recursive: true });
cpSync(new URL('models/esp32-c6-lcd-1.47.glb', site), new URL('models/esp32-c6-lcd-1.47.glb', pub));
for (const t of ['claude', 'matrix', 'msdos', 'win98', 'ubuntu', 'macos', 'aperture'])
  cpSync(new URL(`themes/${t}-chiffres.png`, site), new URL(`screens/${t}-chiffres.png`, pub));
console.log('public/ prêt');
