# Base location

Chooses where to open **p bases** — ambulances, depots, shops, support points — among candidates you
mark on the map, to serve the places of the area (the ones that generate the most trips).

## How to use

1. In **Extensions → Bases**, tap **Mark on the map** and tap the points where a base could be.
   Each candidate goes to the nearest intersection. The candidates are kept in the scenario.
2. Choose how many bases to open and the objective:
   - **Lowest mean time** (p-median): the travel time from each place to its nearest base, weighted by
     the trips it generates, as low as possible;
   - **Most trips covered** (maximal coverage): as many trips as possible reach a base within the
     coverage time; the mean time breaks ties.
3. **Calculate.** It runs in the background with Mobilog's travel-time matrix (the traffic right now,
   with the scenario).

## The result

On the map, the chosen bases (numbered) and each place linked to its base by a line in the base's
color; the other candidates in grey. In the top bar, the mean time to a base and the share covered.

## How the choice is made

Greedy addition (opens, one at a time, the base that improves the objective most) followed by swaps
(closes one and opens another while it improves). It is fast and usually finds the best choice or one
very close to it.

Permissions: read the map (places and travel times), draw on the map and receive taps on the tool.

The code is in [extensions/mobilog.exemplo.bases](https://github.com/edcarllos/mobilog-extensions/tree/main/extensions/mobilog.exemplo.bases) and uses only the C SDK (`sdk/include/mobilog_ext.h`).
