# Local search 2-opt / Or-opt

A routing solver written as an extension, as an example for anyone who wants to write their own.
With the extension on, it shows as **Local search (extension)** in **Extensions → Algorithms** and in
the **Benchmark**, next to Mobilog's solvers.

## How it works

1. **Initial plan:** each vehicle leaves the depot and always goes to the nearest customer that still fits.
2. **Local search:** within each route, reverses segments while the time drops (**2-opt**); then tries
   to move each customer to its best position, in the same or another route (**Or-opt**), within capacity.
3. **Iterated local search:** while there is time, moves a few customers at random and searches again,
   keeping the best plan.

Each better plan goes to Mobilog, which checks its feasibility and computes its cost with its own numbers.

## Parameters (in Algorithms)

- **Customers moved per perturbation:** the larger, the farther the search goes each round.
- **Accepted worsening (%):** a round that leaves the plan up to this much worse continues from it; above that it goes back to the best.

## What it solves

Problems with capacity (CVRP) and mixed fleets. Time windows, pickup and delivery and vehicles already
on the road are left to Mobilog's solvers (ALNS, HGS).

The code is in [extensions/mobilog.exemplo.busca-local](https://github.com/edcarllos/mobilog-extensions/tree/main/extensions/mobilog.exemplo.busca-local) and uses only the C SDK (`sdk/include/mobilog_ext.h`).
