# Concepts

## The module and what it exports

| Export | When the app calls it |
|---|---|
| `mlx_api_version()` | on load: return `MLX_API_VERSION` |
| `mlx_alloc(n)` / `mlx_free(p)` | memory the app writes requests and events into (`MLX_DEFINE_ALLOCATOR` defines both in C; with the C library, use `malloc`/`free`) |
| `mlx_init()` | when turned on: 0 = ok. Declare solvers and subscribe to events here |
| `mlx_event(kind, data, n)` | the events (optional) |
| `mlx_task(kind, data, n)` | background work (optional) |
| `mlx_shutdown()` | when turned off (optional) |

In C++, `mlx::Extension` (`mobilog_ext.hpp`) does this for you: write `init()`, `panel(id)`,
`ui(id, action)`, `tool(...)`, `task(...)` and use `MLX_EXTENSION(YourClass)`.

## Hooks, tasks and solvers

- **Hooks** (`mlx_init`, `mlx_event`) run on the main thread, between two frames, and must be quick:
  over 50 ms the app logs it; over 250 ms three times, it turns the extension off; over 2 s, it stops
  it at once and marks it as hung.
- **Tasks** (`task.start` → `mlx_task`) run in **another instance** of the module, in the background.
  They share no memory with the hooks' instance: bytes in, bytes out (`task.result`). They read the map
  as it was when they started. When one ends, `MLX_EV_TASK_DONE` arrives and `task.poll` brings the
  result. Two at a time, with a time limit.
- **Solvers** run as tasks the app itself starts (see [solvers.md](solvers.md)).

The **Where** column of the [API reference](api.md) says where each function may be called from;
elsewhere it answers `MLX_ERR_UNAVAILABLE`.

## Events

| Event | When | Answer |
|---|---|---|
| `MLX_EV_PANEL` | one of its panels is on screen (every frame) | send its content (`ui.panel`) |
| `MLX_EV_UI` | the user changed a widget | |
| `MLX_EV_CHART` | one of its charts is on screen | send its content (`ui.chart`) |
| `MLX_EV_TOOL` | a tap or drag with its tool on | 1 = taken |
| `MLX_EV_TAP` | a tap no tool took | 1 = used |
| `MLX_EV_FRAME` | every frame (subscription) | |
| `MLX_EV_VEHICLE_ARRIVED` | its vehicles reached the end of their route (subscription) | |
| `MLX_EV_TASK_DONE` / `MLX_EV_EXPERIMENT_DONE` | a task or experiment ended | |
| `MLX_EV_NETWORK` | the network was rebuilt: edge and node ids may have changed; its vehicles are gone | |
| `MLX_EV_SCENARIO` | the scenario changed | |

Subscribing: `mobilog_call(MLX_FN_SUBSCRIBE, &mask, 4, 0, 0)` with `mask = (1u << MLX_EV_FRAME) | ...`.

## Calling the app

Almost everything goes through `mobilog_call(fn, request, size, answer, capacity)`. The result is the
answer's size (it may be larger than the capacity: `mobilog_take` returns it again; `mlx_call_big` does
that for you) or an `MLX_ERR_*` error (< 0):

| Error | |
|---|---|
| `MLX_ERR_UNKNOWN` | this app does not have the function (check with `mobilog_has` first) |
| `MLX_ERR_DENIED` | the permission was not granted, or was revoked |
| `MLX_ERR_INVALID` | malformed request |
| `MLX_ERR_LIMIT` | over a limit |
| `MLX_ERR_UNAVAILABLE` | not now (no area open, wrong thread) |
| `MLX_ERR_NOT_FOUND` | no such item |

```c
double point[2] = {-39.28, -14.79};
uint8_t r[8];
if (mobilog_call(MLX_FN_MAP_NEAREST_NODE, point, 16, r, sizeof r) == 4) { /* u32 node in r */ }
```

```cpp
const uint32_t node = mlx::nearestNode(-39.28, -14.79);
mlx::Answer a = mlx::call(MLX_FN_ROUTE, mlx::Writer().u32(from).u32(to).u8(0));
if (a.ok()) { float cost = a.in.f32(); }
```

`mobilog_has("route.matrix", 12)` tells whether the app has a function. New versions add functions;
numbers never change.

## Memory and limits

- Memory: `limits.memoryMB` (64 MB if left out, up to 512 MB), reserved on demand.
- Stored data (`storage_get`/`storage_set`): 20 MB.
- Own vehicles: 500. Toasts (`ui.toast`): 6 per minute. Log: 50 lines per second.
- Travel-time matrix: 100 locations in hooks, 2000 in tasks.

## What an extension keeps

- **Its data** (`storage_*`): on the device, across sessions.
- **Its section of the scenario** (`scenario.data_get`/`data_set`): goes with the scenario when it is
  saved, exported or synced, even to devices without the extension.

## Languages

The SDK is C (`mobilog_ext.h`) with a C++ layer (`mobilog_ext.hpp`). Any language that produces a
WebAssembly 1.0 module (with bulk memory, sign-ext, non-trapping conversions and mutable globals), with
the exports above, importing only the `mobilog` module (and the few WASI functions listed in
`mlx/include/mlx/Abi.h`), works. See the [FAQ](faq.md).
