# Como contribuir

[English](CONTRIBUTING.en.md)

Obrigado! Há três jeitos de contribuir.

## 1. Publicar uma extensão nova

1. Faça um fork e um branch.
2. `scripts/setup.sh` (ou abra no Codespaces) e
   `mobilog_ext new io.github.<seu-usuario>.<nome>`.
3. Escreva, e confira com `mobilog_ext build`, `test` e `check --store mobilog_store`.
4. Abra um pull request que mude **só** `extensions/<id>/`, com o checklist preenchido.

Detalhes: [docs/pt/publicar.md](docs/pt/publicar.md) e as [diretrizes](docs/pt/diretrizes.md).

## 2. Melhorar uma extensão existente

PRs na pasta de uma extensão precisam da aprovação do autor dela (`.github/CODEOWNERS`). Suba a versão
e escreva a novidade no `CHANGELOG.md` dela. As oficiais (`mobilog.*`) só o mantenedor publica, mas
correções e ideias são bem-vindas.

## 3. Melhorar o SDK, a ferramenta ou os manuais

- Mudanças em `mlx/`, `sdk/` e `tools/` afetam o app e todas as extensões: abra uma issue antes para
  combinar.
- Rode `cmake -S . -B build && cmake --build build && ctest --test-dir build`.
- A referência da API (`docs/*/api.md`) é gerada: `mobilog_ext doc api --lang pt -o docs/pt/api.md`
  (e `--lang en`).
- Manuais em português e inglês: mude os dois.

## Regras

- Código revisável: nada ofuscado, nenhum binário sem fonte.
- Commits e PRs pequenos, com uma frase sobre o porquê.
- Respeito nas conversas. Falhas de segurança: veja [docs/pt/seguranca.md](docs/pt/seguranca.md).
