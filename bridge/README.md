# MiniDisplay Bridge

Le pont côté PC. Il relève le quota Claude et l'état de status.claude.com, puis envoie le tout à la carte par l'USB.

## Démarrage

Double-cliquez sur **`Start-MiniDisplayBridge.vbs`**. Aucune console ne s'ouvre et aucun droit administrateur n'est requis.

| Fichier | Rôle |
|---|---|
| `MiniDisplayBridge.ps1` | le pont, à lancer avec `-Once` pour un test ou `-Port COM3` pour forcer le port |
| `Start-MiniDisplayBridge.vbs` | lanceur silencieux |
| `Stop-MiniDisplayBridge.ps1` | arrête le pont, **à faire avant tout téléversement** car il occupe le port COM |
| `bridge.log` | journal (envoi des trames, erreurs) |
| `secrets.env.example` | modèle du fichier de clés ; copiez-le en `secrets.env` (ignoré par Git) |

## Fonctionnement

- **Source** : la même que *Claude Bot Widget* (`ClaudeStatus/`). Il appelle `GET https://api.anthropic.com/api/oauth/usage` avec le jeton de Claude Code (`%USERPROFILE%\.claude\.credentials.json`). Ce jeton n'est renouvelé que lorsque Claude Code tourne.
- **Rate-limit** : au plus un appel toutes les 5 minutes. L'attente double à chaque refus 429 (10, 20, 40 puis 60 minutes).
- **Port** : il est détecté automatiquement grâce au VID Espressif `303A`, et rouvert tout seul si la carte est débranchée puis rebranchée.
- **Envoi** : une trame toutes les 30 s, qui sert aussi de battement de cœur. Sans trame pendant 2 minutes, la carte affiche « PC déconnecté ».

> Le widget et le pont interrogent l'endpoint chacun de leur côté. En cas de 429 à répétition, coupez le quota de l'un des deux.

## Option : crédits ElevenLabs

Le pont peut aussi suivre votre solde ElevenLabs, affiché dans une 4ᵉ vue de l'écran.

1. Sur elevenlabs.io, ouvrez **Developers → API Keys** et créez une clé. Dans les permissions, réglez **Utilisateur** sur **Accès**. Tout le reste peut rester sur « Pas d'accès ».
2. Copiez `secrets.env.example` en `secrets.env` et collez-y la clé (`ELEVENLABS_API_KEY=sk_...`). Vous pouvez aussi définir la variable d'environnement `ELEVENLABS_API_KEY`.
3. Relancez le pont : le journal affiche `ElevenLabs : cle trouvee, suivi active`.

Le pont appelle `GET https://api.elevenlabs.io/v1/user/subscription` toutes les 10 minutes et lit `character_count`, `character_limit` et `next_character_count_reset_unix`. Sans clé, rien n'est envoyé et la vue affiche « non configure ».

> `secrets.env` ne doit jamais être commité : il est listé dans `.gitignore`.

## Ressources du PC

Toutes les 3 s, le pont envoie le processeur et la mémoire (lus par WMI) ainsi que la charge et la température de la carte graphique (lues par `nvidia-smi`, cartes NVIDIA uniquement). Aucun droit administrateur n'est nécessaire. Ces trames ne sont pas journalisées.

## Protocole série (115200 bauds, lignes ASCII)

```
PC -> carte : Q;<now>;<five%>;<fiveReset>;<week%>;<weekReset>;<level>;<updated>;<erreur>
PC -> carte : ?                      carte -> PC : MINIDISPLAY claude_usage 1
PC -> carte : E;<utilises>;<limite>;<reset>;<updated>;<erreur>   (optionnelle, ElevenLabs)
PC -> carte : P;<cpu%>;<ramUtiliseeMo>;<ramTotaleMo>;<gpu%>;<gpuTemp>   (toutes les 3 s, sans accusé)
carte -> PC : OK                     (accusé de réception d'une trame Q ou E)
```

- Les heures sont des secondes depuis 1970 en **heure locale** du PC, car la carte n'a pas de fuseau horaire.
- `-1` ou `0` signifie « inconnu ».
- `level` vaut -1 si le site est injoignable, 0 si tout va bien, 1 en cas de dégradation ou de maintenance, 2 en cas de panne.
