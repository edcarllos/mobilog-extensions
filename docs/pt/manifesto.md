# O manifesto (`manifest.json`)

```json
{
  "id": "io.github.seu-usuario.pontos-quentes",
  "name": "Pontos quentes",
  "name@en": "Hot spots",
  "version": "1.2.0",
  "apiVersion": 1,
  "minAppVersion": "0.17.0",
  "author": "Seu nome",
  "license": "MIT",
  "summary": "Onde o trânsito está mais lento agora.",
  "summary@en": "Where traffic is slowest right now.",
  "description": "Duas ou três frases: o que faz, para quem, o que mostra.",
  "description@en": "...",
  "category": "analise",
  "keywords": ["congestionamento", "trânsito"],
  "repository": "https://github.com/edcarllos/mobilog-extensions/tree/main/extensions/io.github.seu-usuario.pontos-quentes",
  "entry": "extension.wasm",
  "icon": "icon.svg",
  "help": "help.md",
  "help@en": "help.en.md",
  "changelog": "CHANGELOG.md",
  "screenshots": ["store/mapa.jpg"],
  "permissions": ["mapa.ler", "simulacao.ler", "mapa.desenhar"],
  "limits": {"memoryMB": 64},
  "contributes": [
    {"kind": "painel", "id": "principal", "title": "Pontos quentes"},
    {"kind": "camada", "id": "desenho", "title": "Pontos quentes"},
    {"kind": "grafico", "id": "historico", "title": "Lentidão ao longo do dia"}
  ]
}
```

## Campos

| Campo | Obrigatório | Regras |
|---|---|---|
| `id` | sim | notação reversa, 3 a 64 letras minúsculas, números e `-`, em 2+ partes separadas por ponto. `mobilog.*` é reservado às extensões oficiais |
| `name` | sim | até 40 caracteres; não pode conter "Mobilog" |
| `name@<idioma>` | não | o nome em outro idioma (`name@en`) |
| `version` | sim | semver `1.2.3` (ou `1.2.3-beta.1`) |
| `apiVersion` | sim | o nível da API (hoje 1) |
| `minAppVersion` | não | a versão mínima do Mobilog |
| `author` | loja | até 60 caracteres |
| `license` | loja | até 40 caracteres (ex.: `MIT`, `Apache-2.0`) |
| `summary`, `summary@<idioma>` | loja | uma linha, até 80 caracteres: o que aparece no cartão da loja |
| `description`, `description@<idioma>` | recomendado | até 400 caracteres |
| `category` | loja | `rotas`, `analise`, `transporte`, `seguranca`, `dados`, `visualizacao`, `ferramentas` ou `outros` |
| `keywords` | não | até 8 palavras para a busca |
| `homepage`, `repository` | não | endereços `https://` |
| `entry` | sim | o arquivo `.wasm` do pacote |
| `icon` | loja | `icon.svg` (vetorial, ≤ 32 KB, só formas) |
| `help`, `help@<idioma>` | loja | o manual em Markdown (`help.md`, `help.en.md`) |
| `changelog` | loja | `CHANGELOG.md`, com uma seção `## <versão>` para a versão atual |
| `screenshots` | não | até 4 imagens `.png`/`.jpg` do pacote (≤ 300 KB cada; sugerido 1280×720) |
| `permissions` | não | veja [permissoes.md](permissoes.md) |
| `limits.memoryMB` | não | de 1 a 512 (padrão 64) |
| `contributes` | não | o que ela põe na interface (abaixo) |

"loja": obrigatório para entrar na loja (o `mobilog_ext check` confere); o app aceita sem.

## `contributes`

| `kind` | O que é |
|---|---|
| `painel` | uma sub-aba da aba **Extensões** |
| `camada` | uma camada do mapa (o usuário liga e desliga em Camadas) |
| `ferramenta` | uma ferramenta de mapa (toques e arrastes) |
| `grafico` | um gráfico fixável no mapa |
| `resolvedor` | um algoritmo de roteirização (aparece em Algoritmos e na Bancada) |
| `kpi`, `cartao`, `problema`, `analise` | reservados |

Cada item tem `id` (letras minúsculas, números e `-`, até 32), `title` opcional (até 40) e `icon`
opcional (um ícone do app). Indicadores na barra (`ui.kpis`) e itens de legenda não precisam ser
declarados.

## Versões

Uma versão maior instala como **atualização** e pede só as permissões novas. A anterior fica guardada
até a nova ligar sem erro; o usuário pode voltar a ela. Na loja, toda mudança publicada precisa de uma
versão nova e de uma seção nova no `CHANGELOG.md`.
