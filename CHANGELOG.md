# Novidades / Changes

## 2026-11 — ícone de uma cor / one-colour icon

- Manifesto: `iconMono` (opcional, recomendado), o símbolo numa cor só; o app o pinta na cor do tema nas
  abas, na barra do celular e nos cabeçalhos (Mobilog 0.20.0). O índice da loja leva o SVG; o `check`
  o desenha como o `icon` e recomenda quando falta; os modelos e as 5 oficiais já têm o seu.
- Manifest: `iconMono` (optional, recommended), the symbol in one colour; the app paints it in the
  theme's colour in its tabs, the phone's bar and the headers (Mobilog 0.20.0). The store's index
  carries the SVG; `check` draws it like `icon` and recommends it when missing; the templates and the
  5 official ones have theirs.

## 2026-10 — o repositório público / the public repository

- A biblioteca do formato `.mlx`, o SDK e o `mobilog_ext` saem do Mobilog para cá (MIT).
- A Loja Mobilog: `mobilog_store/`, índice assinado, publicação pelo CI a partir do código revisado.
- `mobilog_ext`: `new`, `build` (sem CMake, reprodutível), `check`, `test` (sem o app, inclusive
  resolvedores), `catalog`, `doc`.
- As 5 extensões oficiais com ícones, capturas, textos em inglês e novidades.
- Manuais em português e inglês, FAQ, diretrizes, especificação do formato.
