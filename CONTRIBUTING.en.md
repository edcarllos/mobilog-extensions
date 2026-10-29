# Contributing

[Português](CONTRIBUTING.md)

Thank you! There are three ways to contribute.

## 1. Publishing a new extension

1. Fork and make a branch.
2. `scripts/setup.sh` (or open it in Codespaces) and
   `mobilog_ext new io.github.<your-user>.<name>`.
3. Write it; check it with `mobilog_ext build`, `test` and `check --store mobilog_store`.
4. Open a pull request that changes **only** `extensions/<id>/`, with the checklist filled in.

Details: [docs/en/publishing.md](docs/en/publishing.md) and the [guidelines](docs/en/guidelines.md).

## 2. Improving an existing extension

PRs in an extension's folder need its author's approval (`.github/CODEOWNERS`). Raise its version and
describe the change in its `CHANGELOG.md`. Only the maintainer publishes the official ones
(`mobilog.*`), but fixes and ideas are welcome.

## 3. Improving the SDK, the tool or the manuals

- Changes in `mlx/`, `sdk/` and `tools/` affect the app and every extension: open an issue first to
  agree on them.
- Run `cmake -S . -B build && cmake --build build && ctest --test-dir build`.
- The API reference (`docs/*/api.md`) is generated: `mobilog_ext doc api --lang en -o docs/en/api.md`
  (and `--lang pt`).
- Manuals are in Portuguese and English: change both.

## Rules

- Reviewable code: nothing obfuscated, no binaries without source.
- Small commits and PRs, with a sentence on why.
- Respect in discussions. Security issues: see [docs/en/security.md](docs/en/security.md).
