"""Rend chaque écran du firmware (thème x vue) en PNG pour le site.

Compile le vrai mdma_autonome.ino (V2) sur PC, avec des imitations du matériel
(stub/), grâce au compilateur Zig installé par pip dans .zig/ (ignoré par Git).
Usage : python tools/screensim/render.py
"""
import glob, os, subprocess, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
OUT = ROOT / 'site' / 'assets' / 'themes'
THEMES = ['matrix', 'msdos', 'win98', 'ubuntu', 'claude', 'macos', 'aperture']  # ordre de THEMES[] dans le .ino
VIEWS = ['jauges', 'chiffres', 'horloge', 'elevenlabs', 'reseau', 'pomodoro', 'palais-siam', 'bourse']


def zig():
    target = HERE / '.zig'
    exe = target / 'ziglang' / ('zig.exe' if os.name == 'nt' else 'zig')
    if not exe.exists():
        subprocess.check_call([sys.executable, '-m', 'pip', 'install', '-q', '--target', str(target), 'ziglang'])
    return str(exe)


def gfx_library():
    for base in [Path.home() / 'Documents' / 'Arduino' / 'libraries', Path.home() / 'Arduino' / 'libraries']:
        if (base / 'Adafruit_GFX_Library').exists():
            return base / 'Adafruit_GFX_Library'
    sys.exit("Bibliothèque Adafruit GFX introuvable (installez-la avec arduino-cli lib install)")


def main():
    from PIL import Image
    gfx = gfx_library()
    build = HERE / '.build'
    build.mkdir(exist_ok=True)
    exe = build / 'sim.exe'
    subprocess.check_call([zig(), 'c++', '-std=c++17', '-O1', '-DARDUINO=100', '-w',
                           '-I', str(HERE / 'stub'), '-I', str(gfx), '-I', str(ROOT / 'firmware' / 'mdma_autonome'),
                           '-DMDMA_SECRETS="secrets.h.example"',  # noms de réseaux fictifs
                           '-x', 'c++', str(HERE / 'main.cpp'), str(gfx / 'Adafruit_GFX.cpp'), '-o', str(exe)],
                          stderr=subprocess.DEVNULL)
    subprocess.check_call([str(exe), str(build)])
    OUT.mkdir(parents=True, exist_ok=True)
    for f in glob.glob(str(build / 't*_v*.ppm')):
        t, v = (int(x[1:]) for x in Path(f).stem.split('_'))
        img = Image.open(f)
        img = img.resize((img.width * 2, img.height * 2), Image.NEAREST)  # pixels nets à l'écran
        img.save(OUT / f'{THEMES[t]}-{VIEWS[v]}.png', optimize=True)
    menu = Image.open(build / 'menu.ppm')
    menu.resize((menu.width * 2, menu.height * 2), Image.NEAREST).save(OUT / 'claude-reseau-menu.png', optimize=True)
    print(f'{len(THEMES) * len(VIEWS)} images dans {OUT.relative_to(ROOT)}')


if __name__ == '__main__':
    main()
