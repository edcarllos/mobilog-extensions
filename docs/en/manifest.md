# The manifest (`manifest.json`)

```json
{
  "id": "io.github.your-user.hot-spots",
  "name": "Pontos quentes",
  "name@en": "Hot spots",
  "version": "1.2.0",
  "apiVersion": 1,
  "minAppVersion": "0.17.0",
  "author": "Your name",
  "license": "MIT",
  "summary": "Onde o trânsito está mais lento agora.",
  "summary@en": "Where traffic is slowest right now.",
  "description": "...",
  "description@en": "Two or three sentences: what it does, for whom, what it shows.",
  "category": "analise",
  "keywords": ["congestion", "traffic"],
  "repository": "https://github.com/edcarllos/mobilog-extensions/tree/main/extensions/io.github.your-user.hot-spots",
  "entry": "extension.wasm",
  "icon": "icon.svg",
  "help": "help.md",
  "help@en": "help.en.md",
  "changelog": "CHANGELOG.md",
  "screenshots": ["store/map.jpg"],
  "permissions": ["mapa.ler", "simulacao.ler", "mapa.desenhar"],
  "limits": {"memoryMB": 64},
  "contributes": [
    {"kind": "painel", "id": "main", "title": "Hot spots"},
    {"kind": "camada", "id": "drawing", "title": "Hot spots"},
    {"kind": "grafico", "id": "history", "title": "Slowness over the day"}
  ]
}
```

The default texts (`name`, `summary`, `description`, `help`) are what the app shows; the app is in
Portuguese today, so write them in Portuguese when you can, and the English ones in the `@en` fields.
Permission, category and contribution keys are fixed words (in Portuguese).

## Fields

| Field | Required | Rules |
|---|---|---|
| `id` | yes | reverse notation, 3-64 lowercase letters, digits and `-`, in 2+ dot-separated parts. `mobilog.*` is reserved for the official extensions |
| `name` | yes | up to 40 characters; may not contain "Mobilog" |
| `name@<lang>` | no | the name in another language (`name@en`) |
| `version` | yes | semver `1.2.3` (or `1.2.3-beta.1`) |
| `apiVersion` | yes | the API level (1 today) |
| `minAppVersion` | no | the lowest Mobilog version |
| `author` | store | up to 60 characters |
| `license` | store | up to 40 characters (e.g. `MIT`, `Apache-2.0`) |
| `summary`, `summary@<lang>` | store | one line, up to 80 characters: what the store's card shows |
| `description`, `description@<lang>` | recommended | up to 400 characters |
| `category` | store | `rotas` (routing), `analise` (analysis), `transporte` (public transport), `seguranca` (safety), `dados` (data), `visualizacao` (visualization), `ferramentas` (tools) or `outros` (other) |
| `keywords` | no | up to 8 search words |
| `homepage`, `repository` | no | `https://` addresses |
| `entry` | yes | the package's `.wasm` file |
| `icon` | store | `icon.svg` (vector, ≤ 32 KB, shapes only) |
| `help`, `help@<lang>` | store | the manual in Markdown (`help.md`, `help.en.md`) |
| `changelog` | store | `CHANGELOG.md`, with a `## <version>` section for the current version |
| `screenshots` | no | up to 4 `.png`/`.jpg` images in the package (≤ 300 KB each; 1280×720 suggested) |
| `permissions` | no | see [permissions.md](permissions.md) |
| `limits.memoryMB` | no | 1 to 512 (default 64) |
| `contributes` | no | what it adds to the interface (below) |

"store": required to enter the store (`mobilog_ext check` checks it); the app accepts packages without.

## `contributes`

| `kind` | What it is |
|---|---|
| `painel` | panel: a sub-tab of the **Extensões** tab |
| `camada` | layer: a map layer (the user turns it on and off in Layers) |
| `ferramenta` | tool: a map tool (taps and drags) |
| `grafico` | chart: a chart that can be pinned to the map |
| `resolvedor` | solver: a routing algorithm (in Algorithms and the Benchmark) |
| `kpi`, `cartao`, `problema`, `analise` | reserved |

Each item has an `id` (lowercase letters, digits and `-`, up to 32), an optional `title` (up to 40) and
an optional `icon` (one of the app's). Indicators on the top bar (`ui.kpis`) and legend items need no
declaration.

## Versions

A higher version installs as an **update** and asks only for the new permissions. The previous one is
kept until the new one starts without errors; the user may go back to it. In the store, every
published change needs a new version and a new section in `CHANGELOG.md`.
