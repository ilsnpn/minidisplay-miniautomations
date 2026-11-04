# Thèmes archivés

Thèmes retirés du firmware V2 (`mdma_autonome`) parce qu'ils ne servaient pas au quotidien. Ils ne sont plus compilés, donc plus envoyés dans la carte, mais tout ce qu'il faut pour les rétablir est ici. Les captures (rendues par le vrai firmware) sont dans `captures/`.

Pour en remettre un : recopier son bloc dans `THEMES[]`, et le code d'en-tête ou d'effet associé s'il y en a un (voir plus bas). Le thème par défaut se retrouve par son nom (« Claude Code »), l'ordre du tableau n'a pas d'importance. Ajouter aussi son nom à `THEMES` dans `tools/screensim/render.py`, dans le même ordre que le tableau.

## Matrix

Vert phosphore sur noir, police pixel, barres en blocs, pluie de caractères en fond et lignes de balayage.

Dans `THEMES[]` :

```cpp
  { "Matrix", c565(0, 0, 0), c565(0, 38, 10), c565(0, 255, 65), c565(10, 143, 46), c565(184, 255, 201),
    c565(10, 110, 36), c565(255, 77, 77), 0, 0,
    { 0, 255, 65 }, { 255, 77, 77 }, { 10, 143, 46 }, true, H_PLAIN, B_SEGMENT, FX_RAIN },
```

Effet `FX_RAIN` : l'énumération `Fx` doit contenir `FX_RAIN`, la fonction ci-dessous va avant `drawScanlines()`, et `render()` l'appelle juste après `canvas.fillScreen(COL_BG)` :

```cpp
  if (T->fx == FX_RAIN) drawRain();
```

```cpp
// Pluie de caractères Matrix en fond, renouvelée toutes les 5 s
static void drawRain() {
  uint32_t seed = timeOk() ? nowLocal() / 5 : millis() / 5000;
  setRole(F_SMALL);
  for (int16_t x = 2; x < W; x += 12) {
    seed = seed * 1103515245u + 12345u;
    const int16_t len = 4 + (seed >> 16) % 10;
    const int16_t y0 = (int16_t)((seed >> 8) % (H / 9)) * 9 - 40;
    for (int16_t i = 0; i < len; i++) {
      seed = seed * 1103515245u + 12345u;
      const int16_t y = y0 + i * 9;
      if (y < 0 || y > H - 8) continue;
      const uint8_t k = 22 + 48 * i / len;  // la tête de colonne est la plus claire
      const uint16_t c = c565(0, k, k / 4);
      canvas.drawChar(x, y, 33 + (seed >> 16) % 90, c, c, 1);  // fond = couleur : transparent
    }
  }
}
```

## Ubuntu

Aubergine et orange, police FreeSans, barre supérieure façon GNOME.

Dans `THEMES[]` :

```cpp
  { "Ubuntu", c565(48, 10, 36), c565(94, 39, 80), c565(255, 255, 255), c565(179, 155, 171), c565(233, 84, 32),
    c565(122, 106, 117), c565(252, 175, 62), c565(17, 17, 17), c565(255, 255, 255),
    { 233, 84, 32 }, { 239, 41, 41 }, { 138, 226, 52 }, false, H_UBUNTU, B_ROUND, FX_NONE },
```

`H_UBUNTU` doit être dans l'énumération `Header` (entre `H_PLAIN` et `H_MAC`), et ce cas dans le `switch (T->header)` de `drawHeader()` :

```cpp
    case H_UBUNTU:  // barre supérieure GNOME
      canvas.fillRect(0, 0, W, 24, T->barBg);
      setRole(F_LABEL);
      drawLeft(t, MX, 17, T->barFg);
      drawLeft(s, MX + textWidth(t) + 5, 17, COL_ACCENT);
      statusDot(W - MX - 6, 12, 5);
      break;
```

## MS-DOS et Windows 98

Déjà absents de la V2. Ils n'existent que dans la V1 (`firmware/claude_usage`, étiquette Git `v1-usb`).
