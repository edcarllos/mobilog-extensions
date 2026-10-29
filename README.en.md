# Mobilog extensions

[Português](README.md)

Extensions add panels, layers, tools, charts, analyses and routing algorithms to
[Mobilog](https://github.com/edcarllos) (traffic simulation and routing on the real map) without
changing the app. This repository holds the **store**, the **SDK**, the **`mobilog_ext`** tool and the
**source code** of every published extension.

- **Using them:** in the app, **Sistema → Extensões → Loja** (System → Extensions → Store).
- **Writing one:** [quick start](docs/en/quick-start.md) (10 minutes, even without installing anything,
  in Codespaces).
- **Publishing:** a pull request with your extension's folder. See [publishing](docs/en/publishing.md).

## What is here

```
mobilog_store/      THE STORE: the published .mlx packages and the signed index (made by the CI)
extensions/<id>/    each extension's source code (one folder per extension)
sdk/                the SDK: C/C++ headers, CMake, the routing model (VRP)
templates/          C and C++ templates (what `mobilog_ext new` copies)
tools/mobilog_ext/  the tool: new, build, check, test, pack, sign, catalog...
mlx/                the .mlx format library (the same the app checks packages with)
docs/pt, docs/en    manuals, API reference, FAQ
spec/               the format's specification
keys/               the store's public key
scripts/            setting up (wasi-sdk) and publishing the store
```

## How an extension gets into the store

1. Someone writes the code in `extensions/<id>/` and opens a pull request.
2. The CI checks it (manifest, icon, permissions, reproducible build, a test without the app).
3. A maintainer reviews and merges it.
4. The CI builds it again, **signs it with the store's key** and publishes it in `mobilog_store/`.
5. Apps see it in the Store with the **Verificada · Loja** (verified · store) badge; `mobilog_server`s on
   local networks mirror it.

What is in the store is always the build of reviewed code: nobody sends binaries.

## Official extensions

| | |
|---|---|
| [Routing and delivery](extensions/mobilog.rotas) | Mobilog's routing algorithms (CW, ALNS, HGS...) |
| [Hot spots](extensions/mobilog.exemplo.pontos-criticos) | where traffic is slowest right now |
| [Base location](extensions/mobilog.exemplo.bases) | where to open bases by mean time or coverage |
| [Bus line](extensions/mobilog.exemplo.onibus) | a bus line in the simulated traffic |
| [Local search 2-opt / Or-opt](extensions/mobilog.exemplo.busca-local) | a simple solver in C, to learn from |

## Building the tool

```bash
scripts/setup.sh          # wasi-sdk 34 (SHA-256 checked) + build/tools/mobilog_ext/mobilog_ext
ctest --test-dir build    # the self-test: templates, reproducible builds, signatures, refusals
```

## License

[MIT](LICENSE). Each extension may have its own (the `license` field and a `LICENSE` in its folder).
