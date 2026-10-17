"""Bruitages synthétisés pour la vidéo (aucun fichier externe, aucun droit)."""
import numpy as np, wave
from pathlib import Path
SR = 44100
OUT = Path(__file__).parent / 'public' / 'sfx'
rng = np.random.default_rng(7)

def save(name, x):
    x = np.clip(x / (np.abs(x).max() + 1e-9) * 0.8, -1, 1)
    with wave.open(str(OUT / f'{name}.wav'), 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes((x * 32767).astype('<i2').tobytes())

def t(d): return np.arange(int(SR * d)) / SR

def lowpass(x, a):  # filtre récursif simple, a = 0..1 (petit = sourd)
    y = np.empty_like(x); acc = 0.0
    for i, v in enumerate(x):
        acc += (a[i] if np.ndim(a) else a) * (v - acc); y[i] = acc
    return y

# Whoosh : bruit filtré dont la coupure monte puis redescend
d = t(0.9); env = np.sin(np.pi * d / d[-1]) ** 2
save('whoosh', lowpass(rng.standard_normal(d.size), 0.02 + 0.25 * env) * env)
# Blip : petit bip électronique
d = t(0.09); save('blip', np.sin(2 * np.pi * 1320 * d) * np.exp(-d * 45))
# Click : déclic sec
d = t(0.04); save('click', rng.standard_normal(d.size) * np.exp(-d * 180) + np.sin(2 * np.pi * 2400 * d) * np.exp(-d * 120))
# Glitch : bruit numérique haché
d = t(0.14); gate = (np.floor(d * 90) % 2)
save('glitch', np.sign(np.sin(2 * np.pi * rng.uniform(300, 1800) * d)) * gate * np.exp(-d * 18) + 0.3 * rng.standard_normal(d.size) * np.exp(-d * 30))
# Impact : basse qui chute en fréquence
d = t(1.2); f = 90 * np.exp(-d * 3) + 38
save('impact', np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-d * 3.2) + 0.15 * lowpass(rng.standard_normal(d.size), 0.05) * np.exp(-d * 8))
# Carillon final : accord doux
d = t(2.2)
save('chime', sum(np.sin(2 * np.pi * fr * d) * np.exp(-d * k) for fr, k in [(523.25, 1.6), (659.25, 1.9), (783.99, 2.2), (1046.5, 2.8)]))
print('ok')
