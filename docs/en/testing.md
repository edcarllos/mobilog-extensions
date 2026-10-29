# Testing

## `mobilog_ext test`: without the app

```bash
mobilog_ext test extensions/io.github.your-user.my-extension
```

Starts the extension in an interpreter (WAMR) the way the app does (`_initialize`, `mlx_api_version`,
`mlx_init`), sends the events of a normal use (the network and the scenario once, its panels and
charts on screen every frame, a tap on the map and on its tools) and turns it off. It shows:

- each hook's worst time;
- the app functions it called, and the ones not available there (no map or simulation, as in the app
  before an area is open: the extension must cope);
- the memory used;
- the extension's log.

**Errors:** a trap (out-of-bounds access, `unreachable`...), `mlx_init` other than 0, a hook over 2 s and
a call to a function whose permission the manifest does not ask for.

Declared route solvers run on a 20-customer CVRP; each one must offer feasible solutions.

### Script

A `test/roteiro.json` in the extension's folder (or `--script file.json`) replaces the default use:

```json
[
  {"network": 1},
  {"frames": 30},
  {"panel": "main"},
  {"click": 10, "in": "main"},
  {"tool": "mark", "action": "tap", "at": [-39.27, -14.78]},
  {"tap": [-39.28, -14.79]},
  {"chart": "history"},
  {"scenario": true},
  {"frames": 30}
]
```

## In the app: the development folder

**Sistema → Extensões → Avançado → Desenvolvimento** (computers): choose the extension's `dist/` folder.
Every new or changed `.mlx` there is installed again at once, turned on, with what it asks granted, and
runs with the **Em desenvolvimento** badge. With:

```bash
mobilog_ext build extensions/io.github.your-user.my-extension --watch
```

saving the code updates the app. The simulation goes on; the extension starts over (its stored data
stays).

For automation (screenshots, end-to-end tests) the app takes `MOBILOG_EXT_DEV=<folder>`.

## Debugging

- **Sistema → Extensões → (the extension) → Registro** (log): what it wrote with `mobilog_log` (or
  `printf` with the C library) and the errors: slow call, hung, trap.
- `mobilog_ext inspect package.mlx`: manifest, permissions, contributions, module (functions, loops,
  memory, imports) and signature.
- A refused module says why: memory with no maximum, a feature the app does not accept (SIMD, threads,
  exceptions...) or an unknown import.
