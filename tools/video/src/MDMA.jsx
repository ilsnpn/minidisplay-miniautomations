// Vidéo de présentation MDMA : 20 s, 1920 x 1080, 30 i/s.
// 1. Le nom MDMA se déplie  2. La carte arrive en 3D  3. Les thèmes défilent  4. Carton final
import { AbsoluteFill, Audio, Easing, interpolate, Sequence, staticFile, useCurrentFrame } from 'remotion';
import { ThreeCanvas } from '@remotion/three';
import { useThree } from '@react-three/fiber';
import { useEffect } from 'react';
import * as THREE from 'three';
import { RoomEnvironment } from 'three/examples/jsm/environments/RoomEnvironment.js';
import { loadFont } from '@remotion/google-fonts/SpaceGrotesk';
import { loadFont as loadMono } from '@remotion/google-fonts/JetBrainsMono';
import { Board, useAssets } from './Board';

const { fontFamily: SANS } = loadFont();
const { fontFamily: MONO } = loadMono();
const ACCENT = '#d97757';
export const DURATION = 550;

// Calendrier des thèmes : intervalles de plus en plus courts, retour à Claude Code
const NAMES = ['Claude Code', 'Matrix', 'MS-DOS', 'Windows 98', 'Ubuntu', 'macOS', 'Aperture Science'];
const T0 = 230;
const SWITCHES = (() => {
  const out = [];
  let f = T0;
  const gaps = [30, 28, 26, 24, 22, 20, 30];
  [1, 2, 3, 4, 5, 6, 0].forEach((s, i) => { f += gaps[i]; out.push({ frame: f, screen: s }); });
  return out;
})();

const ease = Easing.bezier(0.16, 1, 0.3, 1);
const clamp = { extrapolateLeft: 'clamp', extrapolateRight: 'clamp' };

function currentScreen(f) {
  let screen = 0;
  for (const s of SWITCHES) if (f >= s.frame) screen = s.screen;
  return screen;
}

function Env() {
  const { gl, scene } = useThree();
  useEffect(() => {
    const pm = new THREE.PMREMGenerator(gl);
    scene.environment = pm.fromScene(new RoomEnvironment(), 0.04).texture;
  }, [gl, scene]);
  return null;
}

function Intro() {
  const f = useCurrentFrame();
  const open = interpolate(f, [20, 34], [0, 1], { ...clamp, easing: ease });
  const fade = interpolate(f, [46, 54], [1, 0], clamp);
  const zoom = interpolate(f, [42, 54], [1, 3.2], { ...clamp, easing: Easing.in(Easing.cubic) });
  const blurOut = interpolate(f, [44, 54], [0, 14], clamp);
  const stacked = open > 0.02;
  const letter = (ch, rest, i) => {
    const k = interpolate(f, [2 + i * 4, 7 + i * 4], [0, 1], { ...clamp, easing: Easing.out(Easing.cubic) });
    const s = 3 - 2 * k;  // arrive de très près et claque en place
    return (
      <span style={{ display: 'inline-flex' }}>
        <b style={{ color: ACCENT, transform: `scale(${s})`, opacity: k, filter: `blur(${(1 - k) * 10}px)`, display: 'inline-block' }}>{ch}</b>
        <span style={{ maxWidth: `${open * 7}em`, opacity: open, overflow: 'hidden', whiteSpace: 'nowrap', display: 'inline-block' }}>{rest}</span>
      </span>
    );
  };
  return (
    <AbsoluteFill style={{ justifyContent: 'center', alignItems: 'center', opacity: fade, transform: `scale(${zoom})`, filter: `blur(${blurOut}px)` }}>
      <div style={{ fontFamily: SANS, fontWeight: 700, fontSize: 150, color: '#ececf1', letterSpacing: '-0.02em', display: 'flex', flexDirection: stacked ? 'column' : 'row', alignItems: 'center', gap: stacked ? 0 : 18, lineHeight: 1.05 }}>
        <div style={{ display: 'flex', gap: stacked ? 0 : 18 }}>{letter('M', 'ini', 0)}{letter('D', 'isplay', 1)}</div>
        <div style={{ display: 'flex', gap: stacked ? 0 : 18 }}>{letter('M', 'ini', 2)}{letter('A', 'utomations', 3)}</div>
      </div>
    </AbsoluteFill>
  );
}

function Caption({ text, from, to, y = 900, size = 40, mono = false }) {
  const f = useCurrentFrame();
  const o = interpolate(f, [from, from + 10, to - 10, to], [0, 1, 1, 0], clamp);
  const dy = interpolate(f, [from, from + 14], [16, 0], { ...clamp, easing: ease });
  return (
    <div style={{ position: 'absolute', top: y, width: '100%', textAlign: 'center', opacity: o, transform: `translateY(${dy}px)`, fontFamily: mono ? MONO : SANS, fontSize: size, fontWeight: mono ? 500 : 600, color: mono ? ACCENT : '#ececf1', letterSpacing: mono ? '0.08em' : '-0.01em' }}>{text}</div>
  );
}

function Scene3D({ assets }) {
  const f = useCurrentFrame();
  const screen = currentScreen(f);
  const sinceSwitch = Math.min(...SWITCHES.map((s) => (f >= s.frame ? f - s.frame : 1e9)));
  const flash = sinceSwitch < 4 ? interpolate(sinceSwitch, [0, 4], [1.8, 1]) : 1;
  const jitter = sinceSwitch < 3 ? (sinceSwitch % 2 ? 0.6 : -0.6) : 0;

  // Arrivée : vrille puis se pose
  const enter = interpolate(f, [52, 128], [0, 1], { ...clamp, easing: ease });
  const ry = interpolate(enter, [0, 1], [-Math.PI * 2.4, 0.42]) + (f > 128 ? Math.sin((f - 128) / 26) * 0.14 : 0);
  const rx = interpolate(enter, [0, 1], [0.9, -0.28]);
  // Thèmes : face caméra
  const face = interpolate(f, [T0 - 20, T0 + 10], [0, 1], { ...clamp, easing: ease });
  // Final : recule et monte
  const out = interpolate(f, [420, 465], [0, 1], { ...clamp, easing: ease });
  const shake = f >= 100 && f < 116 ? Math.sin(f * 2.7) * interpolate(f, [100, 116], [1.6, 0]) : 0;
  const z = interpolate(enter, [0, 1], [-260, 0]);

  return (
    <ThreeCanvas width={1920} height={1080} camera={{ fov: 30, position: [0, 0, 120] }} gl={{ antialias: true }}
      onCreated={({ gl }) => { gl.toneMapping = THREE.ACESFilmicToneMapping; gl.outputColorSpace = THREE.SRGBColorSpace; }}>
      <Env />
      <ambientLight intensity={0.4} />
      <directionalLight position={[40, 60, 80]} intensity={1.6} />
      <directionalLight position={[-60, -20, 30]} intensity={0.5} color={ACCENT} />
      <Board assets={assets} screen={screen} glow={flash}
        position={[jitter + shake, out * 12 + shake * 0.6, z]}
        rotation={[rx * (1 - face) - 0.12 * face, ry * (1 - face) + 0.18 * face + out * 0.25, 0]}
        scale={1.45 - out * 0.55} />
    </ThreeCanvas>
  );
}

export const MDMA = () => {
  const f = useCurrentFrame();
  const assets = useAssets();
  const bgGlow = interpolate(f, [52, 130], [0, 1], clamp);
  const endO = interpolate(f, [440, 465], [0, 1], { ...clamp, easing: ease });
  const flashO = interpolate(f, [100, 102, 112], [0, 0.55, 0], clamp);

  return (
    <AbsoluteFill style={{ background: '#0b0b0f' }}>
      <AbsoluteFill style={{ background: `radial-gradient(60% 55% at 50% 55%, rgba(217,119,87,${0.22 * bgGlow}), transparent 70%)` }} />
      <Sequence durationInFrames={56}><Intro /></Sequence>
      {assets && f >= 50 && <Scene3D assets={assets} />}
      <AbsoluteFill style={{ background: `radial-gradient(50% 50% at 50% 52%, rgba(255,214,190,${flashO}), transparent 70%)` }} />

      <Caption text="Le PC envoie. L'écran affiche." from={140} to={218} y={880} size={46} />
      {f >= T0 + 30 && f < 420 && <Caption text={NAMES[currentScreen(f)]} from={T0 + 30} to={420} y={900} size={30} mono />}
      <Caption text="7 thèmes · 1 bouton" from={380} to={422} y={160} size={44} />

      <AbsoluteFill style={{ justifyContent: 'flex-end', alignItems: 'center', paddingBottom: 150, opacity: endO }}>
        <div style={{ fontFamily: SANS, fontWeight: 700, fontSize: 120, color: '#ececf1', letterSpacing: '-0.02em' }}>
          <span style={{ color: ACCENT }}>M</span>D<span style={{ color: ACCENT }}>M</span>A
        </div>
        <div style={{ fontFamily: MONO, fontSize: 30, color: '#9a9aa8', marginTop: 6, letterSpacing: '0.06em' }}>Open source · ESP32-C6 · USB-C</div>
      </AbsoluteFill>

      {/* Bruitages */}
      {[0, 1, 2, 3].map((i) => <Sequence key={i} from={5 + i * 4}><Audio src={staticFile('sfx/click.wav')} volume={0.7} /></Sequence>)}
      <Sequence from={99}><Audio src={staticFile('sfx/impact.wav')} volume={0.9} /></Sequence>
      {SWITCHES.map((s) => <Sequence key={s.frame} from={s.frame}><Audio src={staticFile('sfx/glitch.wav')} volume={0.45} /></Sequence>)}
      <Sequence from={442}><Audio src={staticFile('sfx/chime.wav')} volume={0.6} /></Sequence>
    </AbsoluteFill>
  );
};
