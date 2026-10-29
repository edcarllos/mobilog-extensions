# Quick start: your first extension in 10 minutes

A Mobilog extension is a **WebAssembly** module in a `.mlx` package. It adds panels, map layers,
tools, charts, indicators or routing algorithms to the app without changing the app. It runs isolated
and does only what the permissions the user accepted allow.

## 1. Set up

**Option A: in the browser, nothing to install.** Open this repository in GitHub Codespaces
(**Code → Codespaces → Create codespace**). The environment (`.devcontainer`) downloads wasi-sdk and
builds `mobilog_ext` for you.

**Option B: on your computer** (Linux or macOS; on Windows, `scripts\setup.ps1`):

```bash
git clone https://github.com/edcarllos/mobilog-extensions.git
cd mobilog-extensions
scripts/setup.sh                      # downloads wasi-sdk 34 (SHA-256 checked) and builds mobilog_ext
export PATH="$PWD/build/tools/mobilog_ext:$PATH"
```

You need `git`, `curl`, `cmake` (3.24+) and a C++20 compiler (gcc, clang or Visual Studio).

## 2. Create the extension

```bash
mobilog_ext new io.github.your-user.my-extension --name "My extension" --author "Your name"
```

This creates `extensions/io.github.your-user.my-extension/` from the C++ template (`--c` for C):

```
manifest.json      identity, version, permissions, what it adds to the interface
icon.svg           the icon (48×48, shapes only)
help.md            the manual (in the app's Help and the store); help.en.md in English
CHANGELOG.md       what changed in each version
README.md          what it is, how to build it
build.json         how to build it (optional)
src/extensao.cpp   the code
test/roteiro.json  what the test does (optional)
```

The **id** is in reverse notation and must be unique: use `io.github.<your-GitHub-user>.<name>`.

The templates' comments and texts are in Portuguese (Mobilog's main language); translate them as you
like. Manifest fields have English variants (`name@en`, `summary@en`, `help@en`...).

## 3. Build, test and check

```bash
mobilog_ext build extensions/io.github.your-user.my-extension   # → dist/<id>-<version>.mlx
mobilog_ext test  extensions/io.github.your-user.my-extension   # runs it without the app
mobilog_ext check extensions/io.github.your-user.my-extension   # everything the store checks
```

- `test` starts the extension in an interpreter, with no map or simulation, sends the events of a
  normal use and shows what it called, how long it took and the memory it used. A call without its
  permission in the manifest is an error, as in the app. Route solvers run on a small instance and
  their solutions are checked.
- `check` builds and checks the package as the store does: manifest, module, icon, summary, category,
  license, help, this version's changes, screenshots.

The tool answers in English unless your system language is Portuguese (`MOBILOG_EXT_LANG=en|pt`
forces one). Problems found in a package come from the app's own checks, in Portuguese.

## 4. See it in the app

In Mobilog on a computer: **Sistema → Extensões → Avançado → Desenvolvimento → Escolher a pasta…**
(System → Extensions → Advanced → Development → Choose folder) and pick your extension's `dist/`
folder. It is installed at once, with the **Em desenvolvimento** (in development) badge.

Keep the build watching:

```bash
mobilog_ext build extensions/io.github.your-user.my-extension --watch
```

Every saved file rebuilds the package and the app reloads it.

## 5. Publish it in the store

Fork this repository, put your extension's folder in `extensions/` and open a pull request. The CI
checks everything; after review and merge, the store builds, signs and publishes it. See
[publishing.md](publishing.md).

## Next

- [concepts.md](concepts.md): lifecycle, events, hooks and tasks, limits.
- [manifest.md](manifest.md) and [permissions.md](permissions.md).
- [interface.md](interface.md): panels, charts, layers, tools.
- [api.md](api.md): every function.
- [solvers.md](solvers.md): routing algorithms.
- [performance.md](performance.md), [testing.md](testing.md), [faq.md](faq.md).
- The official extensions in `extensions/mobilog.*` are complete examples.
