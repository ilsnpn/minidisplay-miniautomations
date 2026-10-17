// La carte en 3D (modèle CAO Waveshare) avec l'image du firmware plaquée sur l'écran.
import { useEffect, useMemo, useState } from 'react';
import { continueRender, delayRender, staticFile } from 'remotion';
import * as THREE from 'three';
import { GLTFLoader } from 'three/examples/jsm/loaders/GLTFLoader.js';

export const SCREENS = ['claude', 'matrix', 'msdos', 'win98', 'ubuntu', 'macos', 'aperture'];

// Zone active de l'écran 1,47″ dans le repère du STEP (mm) : centrée sur le module
const ACTIVE = { w: 32.4, h: 17.4, cx: 0, cy: -1.33, z: 1.47 };

export function useAssets() {
  const [assets, setAssets] = useState(null);
  const [handle] = useState(() => delayRender('Chargement du modèle et des écrans'));
  useEffect(() => {
    const tl = new THREE.TextureLoader();
    Promise.all([
      new GLTFLoader().loadAsync(staticFile('models/esp32-c6-lcd-1.47.glb')),
      ...SCREENS.map((s) => tl.loadAsync(staticFile(`screens/${s}-chiffres.png`)))
    ]).then(([gltf, ...tex]) => {
      tex.forEach((t) => { t.colorSpace = THREE.SRGBColorSpace; t.magFilter = THREE.NearestFilter; t.minFilter = THREE.LinearFilter; });
      gltf.scene.traverse((o) => {
        if (o.isMesh && o.parent?.userData?.part === '1_47INCH-172X320-LCD') o.material = new THREE.MeshStandardMaterial({ color: 0x050506, roughness: 0.25, metalness: 0.2 });
        if (o.isMesh && (o.userData.part ?? o.parent?.userData?.part) === 'BOARD') o.material = new THREE.MeshStandardMaterial({ color: 0x17171b, roughness: 0.55, metalness: 0.1 });
        if (o.isMesh && (o.userData.part ?? o.parent?.userData?.part) === 'ESP32-S3-LCD-1_47-PMMA') o.material = new THREE.MeshPhysicalMaterial({ color: 0xffffff, transparent: true, opacity: 0.25, roughness: 0.1 });
      });
      setAssets({ model: gltf.scene, tex });
      continueRender(handle);
    });
  }, [handle]);
  return assets;
}

// rotation : [x, y, z] en radians appliquée à la carte posée écran face caméra
export function Board({ assets, screen, glow = 1, rotation = [0, 0, 0], position = [0, 0, 0], scale = 1 }) {
  const model = useMemo(() => assets.model.clone(), [assets]);
  return (
    <group position={position} rotation={rotation} scale={scale}>
      {/* STEP -> scène : longueur à l'horizontale, USB-C à gauche, écran vers la caméra */}
      <group rotation={[0, 0, -Math.PI / 2]}>
        <primitive object={model} />
        <mesh position={[ACTIVE.cx, ACTIVE.cy, ACTIVE.z]} rotation={[0, 0, Math.PI / 2]}>
          <planeGeometry args={[ACTIVE.w, ACTIVE.h]} />
          <meshBasicMaterial map={assets.tex[screen]} toneMapped={false} color={new THREE.Color(glow, glow, glow)} />
        </mesh>
      </group>
    </group>
  );
}
