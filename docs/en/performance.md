# Performance

Mobilog extensions run **compiled**, not interpreted:

- on computers and Android, by wasmtime (Cranelift);
- in the browser (and in the iPhone/iPad app, which runs Mobilog for the web), by the browser's engine.

Measured in Mobilog, the same code runs at **1.0× to 1.4×** the time of C compiled into the app. The
Rotas e entregas package, for instance, runs at 1.04× to 1.39× of native. The simulation step does not
change with the official extensions on.

## Rules

1. **Ask for whole lists, not item by item.** Each `mobilog_call` costs a few tens of nanoseconds (as
   much as a slow function call). Get `map.edges` once, not one edge per call in a loop.
2. **The heavy, common work is native in the app.** Routes, costs from a node and travel-time matrices
   (`route.*`) are computed by the app, in parallel. Do not reimplement Dijkstra in the extension.
3. **Quick hooks; heavy work in tasks.** A hook over 50 ms goes to the log; over 250 ms three times, the
   extension is turned off. Long computations go to `task.start`, in the background.
4. **Send drawings only when they change.** `draw.set_layer` replaces the whole layer; do not send it
   every frame if nothing changed.
5. **Light panels.** `MLX_EV_PANEL` arrives every frame while the panel is on screen: build the widget
   list from what you already computed.
6. **Memory on demand.** `limits.memoryMB` is the maximum, not what is reserved.

## Measuring

- `mobilog_ext test` shows each hook's worst time (interpreted, slower than in the app, but useful to
  compare versions).
- In the app: **Sistema → Extensões → (the extension)** shows calls, last and worst time and memory;
  **Sistema → Extensões → Avançado → Desempenho das extensões** measures the device.
