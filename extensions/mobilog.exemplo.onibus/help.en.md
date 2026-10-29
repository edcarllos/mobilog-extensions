# Bus line

A circular bus line with buses running **inside the simulation**, in the middle of the traffic: they
face the same signals, queues and congestion as the other vehicles.

## How to use

1. In **Extensions → Bus**, tap **Mark stops** and tap the map, in the order of the route. The line
   links each stop to the next by the fastest path and returns from the last to the first. The stops
   are kept in the scenario.
2. Choose how many buses, the **headway** between departures and the **dwell time**.
3. **Start operation.** The buses leave the first stop, one per headway, and go from stop to stop.

## Indicators

- **Round trip:** how long a bus takes to complete the loop.
- **Headway:** the actual time between two buses passing the first stop.
- **Variation:** how much the headways vary (standard deviation over the mean). A bus caught in
  traffic falls behind, picks up more passengers and falls further behind, and the one behind catches
  up: above 30%, the buses are bunching.

If the network changes (a new road, a closed street) or the simulation restarts, the operation restarts
by itself on the new network, with the buses leaving the first stop again.

Try it: start the operation, then close a lane on the way (Events) or change the time, and watch the
variation rise.

Permissions: read the map and the simulation, draw on the map, receive taps on the tool and control the
buses (the extension's vehicles).

The code is in [extensions/mobilog.exemplo.onibus](https://github.com/edcarllos/mobilog-extensions/tree/main/extensions/mobilog.exemplo.onibus) and uses only the C SDK (`sdk/include/mobilog_ext.h`).
