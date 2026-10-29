# Routing and delivery

Mobilog's package of routing algorithms. Installed and on, the **Extensions** tab shows **Problems** (routing on the real map: CVRP, time windows, pickup and delivery, mixed fleet), **Applications** (real-time delivery with continuous re-optimization), **Algorithms** (the parameters of each one) and **Benchmark** (compares algorithms over several seeds).

The algorithms:

- **Nearest neighbour:** greedy, always the nearest customer that fits.
- **Clarke-Wright:** the classic savings construction (asymmetric variant).
- **CW + local search (parallel):** restarts with randomized savings, improved by relocate, swap and 2-opt.
- **ALNS (parallel):** destroys and rebuilds with adaptive weights and simulated annealing; handles time windows, pickup and delivery, vehicles already on the road and mixed fleets.
- **HGS · hybrid genetic (parallel):** giant tour + Split, OX crossover and granular local search.

The parallel ones run one copy per processor core, each with its own seed, and Mobilog keeps the best solution; the work is done in short slices so the simulation stays smooth. Every solution is checked by the app itself before it is used.

The code is in [extensions/mobilog.rotas](https://github.com/edcarllos/mobilog-extensions/tree/main/extensions/mobilog.rotas) (C++ with wasi-sdk and the SDK's VRP model).
