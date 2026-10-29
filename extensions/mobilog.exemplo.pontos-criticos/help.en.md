# Hot spots

Shows where traffic is slowest right now in the simulation.

- **On the map** (layer *Hot spots*): the slowest stretches in red, numbered from worst to least bad.
  One per street: if the same avenue has several slow stretches, the worst one shows.
- **In the panel** (Extensions → Hot spots): the list with each stretch's speed and how much of the
  limit it is; the button with the number takes the map there.
- **Network slowness:** how much of the allowed speed is lost, weighted by the length and lanes of the
  roads. It shows in the top bar and in a chart you can pin to the map.

The list is rebuilt every 3 seconds of simulation. A stretch enters when it is below half the limit
and at least 40 m long.

Permissions: read the map (names and stretches), read the simulation (speeds) and draw on the map.

The code is in [extensions/mobilog.exemplo.pontos-criticos](https://github.com/edcarllos/mobilog-extensions/tree/main/extensions/mobilog.exemplo.pontos-criticos) and uses only the C SDK (`sdk/include/mobilog_ext.h`).
