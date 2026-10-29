# Interface: panels, charts, indicators, layers and tools

The app draws everything in its own style, on computers, phones and in the browser. An extension
describes the content and receives what the user did.

## Panels

Declare `{"kind": "painel", "id": "main"}`. While the panel is on screen, `MLX_EV_PANEL` arrives (every
frame): answer with the list of widgets.

```c
mlx_writer w;
mlx_w_init(&w, buffer, sizeof buffer);
mlx_ui_begin(&w);
mlx_ui_section(&w, 1, "Result", "optional summary", 1);
mlx_ui_kpi(&w, "Mean time", "12.4 min", 0);
mlx_ui_button(&w, 10, "Calculate", 1, 0);
mlx_ui_end_section(&w);
mlx_ui_send(MLX_FN_UI_PANEL, "main", &w, scratch, sizeof scratch);
```

```cpp
mlx::Ui ui;
ui.section(1, "Result");
ui.kpi("Mean time", "12.4 min");
ui.button(10, "Calculate", true);
ui.endSection();
ui.send(id);
```

Widgets: section (collapsible), text (normal, muted, title, good, warning, bad), button, switch,
number (with min, max, step and decimals), choice (side-by-side buttons), select list, text field,
indicator, table, line chart, bars, progress, separator and "same line". Widget ids are yours and come
back in `MLX_EV_UI` with the action's kind (click, switch, number, choice, text) and value. A value the
user changes shows at once; your answer arrives on the next frame.

The exact layouts are at the end of `sdk/include/mobilog_ext.h` (*Widgets* section).

## Charts

Declare `{"kind": "grafico", "id": "history"}`. While it is on screen, `MLX_EV_CHART` arrives: answer
with `ui.chart` (the same widgets: usually a line chart or bars). The user can pin it to the map.

## Indicators on the top bar

`ui.kpis` puts up to 6 indicators on the top bar (with a short label for phones and a tip). No
declaration needed.

## Layers

Declare `{"kind": "camada", "id": "drawing"}` and ask for `mapa.desenhar`. `draw.set_layer` sends the
whole layer at once: lines (color, width, points), markers (point, icon, color, label), areas (fill,
outline, points) and colors per road edge (0 red to 1 green). Send it again only when it changes. The
user turns the layer on and off in **Map layers**.

## Tools

Declare `{"kind": "ferramenta", "id": "mark"}` and ask for `mapa.ferramentas`. `ui.tool` turns the tool
on (usually from a panel button). With it on, taps and drags on the map arrive in `MLX_EV_TOOL` with the
action (`MLX_TOOL_TAP`, `GRAB`, `DRAG_BEGIN`, `DRAG_MOVE`, `DRAG_END`, `DRAG_CANCEL`, `OFF`) and the point
(lon, lat). Answer 1 to take the gesture. Taps with no tool arrive as `MLX_EV_TAP`.

## Others

- `ui.legend`: legend items (color, square or line, label).
- `ui.toast`: a short notice (up to 6 per minute).
- `ui.focus`: takes the map to a point (with zoom).

## Icons

The extension's icon (`icon.svg`) shows in the store, the manager and its panels' sub-tab. See the icon
guide in [guidelines.md](guidelines.md#icon).
