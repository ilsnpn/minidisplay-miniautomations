// Vue 3D annotée de la carte (assemblée, éclatable d'un bouton) (page « La carte », section En bref).
// Charge le modèle CAO officiel, sépare l'écran, la plaque acrylique, le circuit et
// les composants du dessous, puis relie chaque élément à son étiquette par un trait
// qui part de la pièce, monte en biais et revient à l'horizontale.
import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { RoomEnvironment } from 'three/addons/environments/RoomEnvironment.js';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';

// Icônes au trait, 24 x 24
const ICON = {
  screen: '<rect x="3" y="5" width="18" height="12" rx="2"/><path d="M8 21h8M12 17v4"/>',
  chip: '<rect x="6" y="6" width="12" height="12" rx="2"/><path d="M9 2v4M15 2v4M9 18v4M15 18v4M2 9h4M2 15h4M18 9h4M18 15h4"/>',
  wifi: '<path d="M2 9a15 15 0 0 1 20 0M5 12.5a10 10 0 0 1 14 0M8.5 16a5 5 0 0 1 7 0"/><circle cx="12" cy="19.5" r="1"/>',
  usb: '<rect x="4" y="8" width="16" height="8" rx="4"/><path d="M8 12h8"/>',
  sd: '<path d="M6 2h9l4 4v16H6z"/><path d="M10 6v3M13 6v3M16 7v2"/>',
};

// Pièces du modèle (noms du STEP Waveshare), étiquette courte et côté d'affichage
const CALLOUTS = [
  { id: 'lcd', parts: ['1_47INCH-172X320-LCD'], icon: 'screen', side: 'left', title: 'Écran 1,47″', text: 'IPS 172 × 320' },
  { id: 'esp', parts: ['USER_LIBRARY-ESP8266EX'], icon: 'chip', side: 'left', title: 'ESP32-C6', text: 'RISC-V 160 MHz' },
  { id: 'tf', parts: ['SHELL', ''], icon: 'sd', side: 'left', title: 'microSD', text: 'lecteur de carte' },
  { id: 'ant', parts: ['C3'], icon: 'wifi', side: 'right', title: 'Wi-Fi 6 · BLE 5', text: 'antenne céramique' },
  { id: 'usb', prefix: 'SOLID-', icon: 'usb', side: 'right', title: 'USB-C', text: 'alimentation + données' }
];

// Écart d'éclatement le long de l'épaisseur (mm, repère du STEP : écran vers +Z)
const LIFT = { lcd: 13, pmma: 6.5, under: -7 };

function init(root) {
  const stage = root.querySelector('.ex-stage');
  const svg = root.querySelector('svg');
  const badge = root.querySelector('.ex-badge');
  const toggle = root.querySelector('.ex-toggle');

  const renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
  renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
  renderer.outputColorSpace = THREE.SRGBColorSpace;
  renderer.toneMapping = THREE.ACESFilmicToneMapping;
  stage.prepend(renderer.domElement);

  const scene = new THREE.Scene();
  scene.environment = new THREE.PMREMGenerator(renderer).fromScene(new RoomEnvironment(), 0.04).texture;
  const key = new THREE.DirectionalLight(0xffffff, 1.6);
  key.position.set(30, 60, 40);
  scene.add(key, new THREE.AmbientLight(0xffffff, 0.35));

  const camera = new THREE.PerspectiveCamera(30, 1, 1, 1000);
  camera.position.set(0, 46, 104);
  const controls = new OrbitControls(camera, renderer.domElement);
  controls.enableDamping = true;
  controls.enablePan = false;
  controls.enableZoom = false;
  controls.target.set(0, 0, 0);

  // STEP : X largeur, Y longueur, Z épaisseur. On couche la carte, longueur à l'horizontale.
  const outer = new THREE.Group();
  const inner = new THREE.Group();
  inner.rotation.x = -Math.PI / 2;
  outer.rotation.y = Math.PI / 2;
  outer.add(inner);
  scene.add(outer);

  // Étiquettes HTML
  const items = CALLOUTS.map((c) => {
    const el = document.createElement('div');
    el.className = 'ex-label ' + c.side;
    el.innerHTML = `<svg viewBox="0 0 24 24" aria-hidden="true">${ICON[c.icon]}</svg><div><b>${c.title}</b><span>${c.text}</span></div>`;
    stage.appendChild(el);
    return { ...c, el, nodes: [], anchors: [] };
  });

  let explode = 0, explodeTarget = 0, ready = false;  // assemblée par défaut
  const moving = [];  // { node, dz }

  new GLTFLoader().load('assets/models/esp32-c6-lcd-1.47.glb', (gltf) => {
    const model = gltf.scene;
    inner.add(model);
    // Centrage dans le repère de la carte, pour qu'elle tourne sur elle-même
    inner.updateMatrixWorld(true);
    const all = new THREE.Box3().setFromObject(model).applyMatrix4(new THREE.Matrix4().copy(inner.matrixWorld).invert());
    model.position.copy(all.getCenter(new THREE.Vector3())).negate();
    model.updateMatrixWorld(true);

    // Épaisseur du circuit pour trier dessus / dessous
    const boardNode = model.children.find((n) => n.userData.part === 'BOARD');
    const boardBox = new THREE.Box3().setFromObject(boardNode);
    const toInner = (b) => b.applyMatrix4(new THREE.Matrix4().copy(inner.matrixWorld).invert());
    inner.updateMatrixWorld(true);
    toInner(boardBox);
    const midZ = (boardBox.min.z + boardBox.max.z) / 2;

    model.children.forEach((node) => {
      const part = node.userData.part ?? '';
      const box = toInner(new THREE.Box3().setFromObject(node));
      let dz = 0;
      if (part === '1_47INCH-172X320-LCD') dz = LIFT.lcd;
      else if (part === 'ESP32-S3-LCD-1_47-PMMA') {
        dz = LIFT.pmma;
        node.traverse((o) => { if (o.isMesh) o.material = new THREE.MeshPhysicalMaterial({ color: 0xffffff, transmission: 0.95, roughness: 0.1, thickness: 1, transparent: true, opacity: 0.22 }); });
      } else if (part !== 'BOARD' && box.max.z < midZ) dz = LIFT.under;
      if (part === '1_47INCH-172X320-LCD') node.traverse((o) => { if (o.isMesh) o.material = new THREE.MeshStandardMaterial({ color: 0x0b0b0d, roughness: 0.2, metalness: 0.3 }); });
      if (dz) moving.push({ node, dz });
      items.forEach((it) => {
        if ((it.parts && it.parts.includes(part)) || (it.prefix && part.startsWith(it.prefix))) it.nodes.push({ node, box, dz });
      });
    });

    // Ancres : centre de chaque pièce (les deux boutons gardent chacun la leur)
    items.forEach((it) => {
      if (it.id === 'btn') {
        it.anchors = it.nodes.map(({ box, dz }) => ({ p: box.getCenter(new THREE.Vector3()), dz }));
      } else if (it.nodes.length) {
        const u = new THREE.Box3();
        it.nodes.forEach(({ box }) => u.union(box));
        const p = u.getCenter(new THREE.Vector3());
        if (it.id === 'lcd') p.z = u.max.z;  // sur la face de l'écran
        if (it.id === 'pcb') { p.x = u.max.x; p.z = u.max.z; }  // sur un bord
        it.anchors = [{ p, dz: it.nodes[0].dz }];
      }
    });

    badge.remove();
    ready = true;
  }, undefined, () => {
    badge.textContent = 'Modèle 3D indisponible (ouvrez le site par un serveur web)';
  });

  toggle.addEventListener('click', () => {
    explodeTarget = explodeTarget ? 0 : 1;
    toggle.textContent = explodeTarget ? 'Assembler' : 'Éclater';
    toggle.setAttribute('aria-pressed', explodeTarget === 1);
  });

  let W = 0, H = 0;
  function resize() {
    W = stage.clientWidth;
    H = W < 620 ? 280 : stage.clientHeight;  // écran étroit : hauteur fixe, étiquettes dessous
    renderer.setSize(W, H, false);
    camera.aspect = W / H;
    camera.updateProjectionMatrix();
    svg.setAttribute('viewBox', `0 0 ${W} ${H}`);
  }
  new ResizeObserver(resize).observe(stage);
  resize();

  let visible = true;
  new IntersectionObserver(([e]) => { visible = e.isIntersecting; }).observe(stage);

  const v = new THREE.Vector3();
  const NS = 'http://www.w3.org/2000/svg';
  function drawLeaders() {
    svg.replaceChildren();
    const narrow = W < 620;
    root.classList.toggle('narrow', narrow);
    if (!ready || narrow) return;
    inner.updateMatrixWorld(true);
    for (const side of ['left', 'right']) {
      const list = items.filter((it) => it.side === side && it.anchors.length);
      // Position écran de chaque ancre
      list.forEach((it) => {
        it.screen = it.anchors.map(({ p, dz }) => {
          v.set(p.x, p.y, p.z + dz * explode);
          inner.localToWorld(v).project(camera);
          return [(v.x + 1) / 2 * W, (1 - v.y) / 2 * H];
        });
        it.y = it.screen.reduce((s, a) => s + a[1], 0) / it.screen.length;
      });
      // Étiquettes rangées dans l'ordre vertical des ancres, sans chevauchement
      list.sort((a, b) => a.y - b.y);
      const gap = 52, top = 30, bottom = H - 30;
      let y = top;
      list.forEach((it) => { it.ly = Math.max(y, Math.min(it.y, bottom)); y = it.ly + gap; });
      for (let i = list.length - 1, lim = bottom; i >= 0; i--) { list[i].ly = Math.min(list[i].ly, lim); lim = list[i].ly - gap; }

      const lx = side === 'left' ? 200 : W - 200;      // bord intérieur de la colonne d'étiquettes
      const ex = side === 'left' ? lx + 26 : lx - 26;  // coude
      list.forEach((it) => {
        it.el.style.top = it.ly + 'px';
        it.screen.forEach(([ax, ay]) => {
          const path = document.createElementNS(NS, 'polyline');
          path.setAttribute('points', `${ax},${ay} ${ex},${it.ly} ${lx},${it.ly}`);
          svg.appendChild(path);
          const ring = document.createElementNS(NS, 'circle');
          ring.setAttribute('cx', ax); ring.setAttribute('cy', ay); ring.setAttribute('r', 4.5);
          svg.appendChild(ring);
        });
      });
    }
  }

  const clock = new THREE.Clock();
  (function loop() {
    requestAnimationFrame(loop);
    if (!visible) return;
    const dt = Math.min(clock.getDelta(), 0.05);
    explode += (explodeTarget - explode) * Math.min(1, dt * 4);
    moving.forEach(({ node, dz }) => { node.position.z = dz * explode; });
    // Léger balancement tant que l'utilisateur ne tourne pas la carte lui-même
    if (!controls.userTouched) outer.rotation.y = Math.PI / 2 + Math.sin(clock.elapsedTime * 0.4) * 0.25;
    controls.update();
    renderer.render(scene, camera);
    drawLeaders();
  })();
  controls.addEventListener('start', () => { controls.userTouched = true; });
}

const root = document.getElementById('exploded');
if (root) init(root);
