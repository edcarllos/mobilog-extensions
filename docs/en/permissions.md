# Permissions

An extension asks in its manifest for what it needs; the user sees the list when installing, as in a
browser, may refuse it and may revoke it later. Ask only for what you use: each extra permission is one
more reason not to install.

| Permission | Allows | Sensitive |
|---|---|---|
| `mapa.ler` (read the map) | network, names, places, zones, neighbourhoods, area, routes and travel-time matrices, scenario summary | |
| `simulacao.ler` (read the simulation) | statistics, speeds, times and passages per edge, vehicle positions | |
| `mapa.desenhar` (draw on the map) | its own layers: lines, markers, areas, colors on roads | |
| `mapa.ferramentas` (map tools) | turning its own map tool on | |
| `cenario.propor` (propose changes) | proposing scenario changes the user sees in the Assistant and applies, or not | |
| `cenario.alterar` (change the scenario) | applying scenario changes at once (with undo) | yes |
| `veiculos.controlar` (control vehicles) | creating its own vehicles in the simulation, sending them on routes and removing them | yes |
| `simulacao.ensaiar` (experiments) | experiments: the simulation copied with and without a change, compared | yes |
| `arquivos.exportar` | reserved | |
| `arquivos.importar` | reserved | yes |

**Sensitive** ones change the user's work or the simulation and are highlighted at install time.

## When a permission is missing

The function that needed it answers `MLX_ERR_DENIED` at once, also when the user revokes the
permission while the extension runs. Handle it: show in the panel what cannot be done and why. With no
permission at all, an extension still has its own panel, indicators, stored data and tasks.

`mobilog_ext test` reports as an error any call to a function whose permission the manifest does not
ask for.

## What no permission allows

- **Network:** extensions cannot reach the internet or the local network.
- **Device files:** there is no file system; only the extension's own stored data.
- **Other extensions:** each one runs isolated, with its own memory.
