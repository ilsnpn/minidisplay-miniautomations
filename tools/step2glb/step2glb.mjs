// Convertit un fichier STEP (CAO) en GLB (glTF binaire) lisible par three.js.
// Usage : node step2glb.mjs <entree.stp> <sortie.glb> [--list]
//
// occt-import-js (OpenCascade en WebAssembly) tessèle chaque pièce en triangles.
// Chaque pièce devient un nœud glTF nommé, avec un matériau par couleur de face,
// ce qui permet ensuite de retrouver les composants par leur nom dans le site.

import fs from 'node:fs';
import occtimportjs from 'occt-import-js';

const [input, output] = process.argv.slice(2);
const listOnly = process.argv.includes('--list');
if (!input || (!output && !listOnly)) {
  console.error('Usage : node step2glb.mjs <entree.stp> <sortie.glb> [--list]');
  process.exit(1);
}

const occt = await occtimportjs();
const result = occt.ReadStepFile(new Uint8Array(fs.readFileSync(input)), {
  linearUnit: 'millimeter',
  linearDeflectionType: 'bounding_box_ratio',
  linearDeflection: 0.0008,
  angularDeflection: 0.35
});
if (!result.success) {
  console.error('Lecture STEP impossible');
  process.exit(1);
}

// ---------- Inventaire ----------
let tris = 0;
const min = [Infinity, Infinity, Infinity], max = [-Infinity, -Infinity, -Infinity];
for (const m of result.meshes) {
  tris += m.index.array.length / 3;
  const p = m.attributes.position.array;
  for (let i = 0; i < p.length; i += 3) for (let k = 0; k < 3; k++) {
    if (p[i + k] < min[k]) min[k] = p[i + k];
    if (p[i + k] > max[k]) max[k] = p[i + k];
  }
}
console.log(`${result.meshes.length} pièces, ${tris} triangles`);
console.log(`boîte englobante (mm) : min ${min.map((v) => v.toFixed(2))}  max ${max.map((v) => v.toFixed(2))}`);
if (listOnly) {
  result.meshes.forEach((m, i) => {
    const p = m.attributes.position.array;
    const c = [0, 0, 0];
    for (let j = 0; j < p.length; j += 3) for (let k = 0; k < 3; k++) c[k] += p[j + k];
    const n = p.length / 3;
    const col = m.color ? m.color.map((v) => Math.round(v * 255)).join(',') : '-';
    console.log(`${String(i).padStart(3)}  ${m.name.padEnd(36)} tris ${String(m.index.array.length / 3).padStart(6)}  centre ${c.map((v) => (v / n).toFixed(1)).join(', ')}  couleur ${col}`);
  });
  process.exit(0);
}

// ---------- Écriture GLB ----------
const chunks = [];          // morceaux du buffer binaire
let byteLength = 0;
const gltf = {
  asset: { version: '2.0', generator: 'MDMA step2glb (occt-import-js)' },
  scene: 0, scenes: [{ nodes: [] }], nodes: [], meshes: [],
  materials: [], accessors: [], bufferViews: [], buffers: []
};

function pushView(typed, target) {
  const pad = (4 - (byteLength % 4)) % 4;
  if (pad) { chunks.push(new Uint8Array(pad)); byteLength += pad; }
  const bytes = new Uint8Array(typed.buffer, typed.byteOffset, typed.byteLength);
  chunks.push(bytes);
  gltf.bufferViews.push({ buffer: 0, byteOffset: byteLength, byteLength: bytes.byteLength, target });
  byteLength += bytes.byteLength;
  return gltf.bufferViews.length - 1;
}

const matIndex = new Map();
function material(rgb) {
  const key = rgb.map((v) => v.toFixed(3)).join(',');
  if (!matIndex.has(key)) {
    // Heuristique : les gris clairs (blindages, connecteurs) sont métalliques
    const grey = Math.max(...rgb) - Math.min(...rgb) < 0.06 && rgb[0] > 0.55;
    gltf.materials.push({
      name: 'rgb(' + rgb.map((v) => Math.round(v * 255)).join(',') + ')',
      pbrMetallicRoughness: { baseColorFactor: [...rgb, 1], metallicFactor: grey ? 0.85 : 0.05, roughnessFactor: grey ? 0.35 : 0.6 }
    });
    matIndex.set(key, gltf.materials.length - 1);
  }
  return matIndex.get(key);
}

for (const m of result.meshes) {
  const pos = new Float32Array(m.attributes.position.array);
  const nor = m.attributes.normal ? new Float32Array(m.attributes.normal.array) : null;
  const idx = m.index.array;
  const pMin = [Infinity, Infinity, Infinity], pMax = [-Infinity, -Infinity, -Infinity];
  for (let i = 0; i < pos.length; i += 3) for (let k = 0; k < 3; k++) {
    pMin[k] = Math.min(pMin[k], pos[i + k]); pMax[k] = Math.max(pMax[k], pos[i + k]);
  }
  const posAcc = gltf.accessors.push({ bufferView: pushView(pos, 34962), componentType: 5126, count: pos.length / 3, type: 'VEC3', min: pMin, max: pMax }) - 1;
  const norAcc = nor ? gltf.accessors.push({ bufferView: pushView(nor, 34962), componentType: 5126, count: nor.length / 3, type: 'VEC3' }) - 1 : undefined;

  // Une primitive par couleur de face (les faces d'une même pièce peuvent différer)
  const base = m.color || [0.6, 0.6, 0.6];
  const groups = new Map();
  const faces = m.brep_faces && m.brep_faces.length ? m.brep_faces : [{ first: 0, last: idx.length / 3 - 1, color: null }];
  for (const f of faces) {
    const rgb = f.color || base;
    const key = rgb.join(',');
    if (!groups.has(key)) groups.set(key, { rgb, tri: [] });
    const g = groups.get(key).tri;
    for (let t = f.first; t <= f.last; t++) g.push(idx[t * 3], idx[t * 3 + 1], idx[t * 3 + 2]);
  }
  const primitives = [];
  for (const { rgb, tri } of groups.values()) {
    const big = pos.length / 3 > 65535;
    const arr = big ? new Uint32Array(tri) : new Uint16Array(tri);
    const acc = gltf.accessors.push({ bufferView: pushView(arr, 34963), componentType: big ? 5125 : 5123, count: arr.length, type: 'SCALAR' }) - 1;
    const attributes = { POSITION: posAcc };
    if (norAcc !== undefined) attributes.NORMAL = norAcc;
    primitives.push({ attributes, indices: acc, material: material(rgb) });
  }
  gltf.meshes.push({ name: m.name, primitives });
  // three.js renomme les doublons (« _1 », « _2 ») : le nom d'origine part dans extras → userData.part
  gltf.nodes.push({ name: m.name, mesh: gltf.meshes.length - 1, extras: { part: m.name } });
  gltf.scenes[0].nodes.push(gltf.nodes.length - 1);
}

gltf.buffers.push({ byteLength });
let json = Buffer.from(JSON.stringify(gltf));
json = Buffer.concat([json, Buffer.alloc((4 - (json.length % 4)) % 4, 0x20)]);
let bin = Buffer.concat(chunks.map((c) => Buffer.from(c.buffer, c.byteOffset, c.byteLength)));
bin = Buffer.concat([bin, Buffer.alloc((4 - (bin.length % 4)) % 4)]);

const header = Buffer.alloc(12);
header.writeUInt32LE(0x46546c67, 0); header.writeUInt32LE(2, 4);
header.writeUInt32LE(12 + 8 + json.length + 8 + bin.length, 8);
const jh = Buffer.alloc(8); jh.writeUInt32LE(json.length, 0); jh.writeUInt32LE(0x4e4f534a, 4);
const bh = Buffer.alloc(8); bh.writeUInt32LE(bin.length, 0); bh.writeUInt32LE(0x004e4942, 4);
fs.writeFileSync(output, Buffer.concat([header, jh, json, bh, bin]));
console.log(`écrit : ${output} (${(fs.statSync(output).size / 1024 / 1024).toFixed(2)} Mo, ${gltf.materials.length} matériaux)`);
