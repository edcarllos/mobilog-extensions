# Routing solvers (VRP)

A solver is a routing algorithm that shows in **Extensions → Algorithms** and in the **Benchmark**,
next to the others. The app builds the instance (travel times from the current traffic, demands,
windows, fleet), evaluates every solution with its own numbers and shows the routes on the map.

## Declaring

In the manifest:

```json
"contributes": [{"kind": "resolvedor", "id": "mine", "title": "My solver"}]
```

In `mlx_init`, `solver.declare` with the id, the features (`MLX_VRP_CAPACITY`, `MLX_VRP_TIME_WINDOWS`,
`MLX_VRP_PICKUP_DELIVERY`, `MLX_VRP_HETEROGENEOUS`, `MLX_VRP_DYNAMIC`, `MLX_VRP_PARALLEL`...) and up to 32
parameters (key, label, tip, default, min, max, step, decimals), shown in Algorithms.

## Solving

Each run calls `mlx_task(MLX_TASK_SOLVE_VRP, data, n)` in a **fresh instance** of the module. The data
is the solver's id and the instance (layout in `sdk/include/mobilog_ext.h`; in C++,
`mobilog/vrp/VrpWire.h` reads it: `decodeVrpInstance`). `solver.params` brings the seed, the time limit,
the elapsed time and the parameters.

Send each better solution with `solver.offer` (the routes: lists of locations 1..m-1). The app checks
feasibility and answers with its own cost, whether it is feasible and whether it improved. Stop when
`mobilog_cancelled()` returns 1 (time limit or Cancel).

## In parallel and in slices

With `MLX_VRP_PARALLEL`, the app runs one copy of the module per thread (up to 8), each with its own
seed; `solver.params` brings `_copy` (this copy), `_copies` (how many) and `_sliced` (1: the app asks for
slices).

If `MLX_TASK_SOLVE_VRP` returns 1, the search only prepares, and the app calls
`mlx_task(MLX_TASK_SOLVE_STEP, 0, ms)` repeatedly: work about `ms` milliseconds and return 1 while there
is more to do (0 at the end). That way the simulation keeps its threads. Count iterations with
`solver.progress` so the Benchmark shows convergence.

## Building

C++ solvers usually use the standard library. In `build.json`:

```json
{"runtime": "wasi", "optimize": "O3", "libraries": ["vrp"]}
```

`"vrp"` adds the SDK's routing model (`sdk/contracts`): `Vrp.h` (instance, solution, `evaluate`),
`VrpWire.h` (the blocks) and `Solver.h` (the solver interface).

## Testing

`mobilog_ext test` runs each declared solver on a small CVRP (20 customers, 6 vehicles) for 1 s and
checks that it offers feasible solutions. In the app, the **Benchmark** compares solvers over several
seeds.

## Examples

- `extensions/mobilog.exemplo.busca-local`: C, no library: nearest neighbour, 2-opt, Or-opt.
- `extensions/mobilog.rotas`: C++, five algorithms (Clarke-Wright, ALNS, HGS...), in parallel and in
  slices.
