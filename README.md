# MDMA · MiniDisplay MiniAutomations

Moniteur de quota Claude (session 5 h + semaine) sur la carte **Waveshare ESP32-C6-LCD-1.47**.

```
Claude API ──HTTPS──> PC : bridge/MiniDisplayBridge.ps1 ──USB série──> ESP32-C6 : firmware/claude_usage
```

**Démarrage** : double-cliquez sur `bridge/Start-MiniDisplayBridge.vbs`, la carte étant branchée en USB-C.

## Contenu

- [bridge/](bridge/README.md) : pont côté PC (quota + protocole série)
- [firmware/](firmware/README.md) : sketches Arduino (compilation, téléversement, écran)
- [site/](site/README.md) : site de présentation et documentation, en ligne via Vercel
- [tools/step2glb/](tools/step2glb/) : conversion du modèle CAO STEP en GLB pour le site
- [docs/HARDWARE.md](docs/HARDWARE.md) : la carte (GPIO, brochage, dimensions…)
- `ClaudeStatus/` : widget PC d'origine (référence locale, non versionnée)
- Wiki de la carte : http://www.waveshare.com/wiki/ESP32-C6-LCD-1.47
