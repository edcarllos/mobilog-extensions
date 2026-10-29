# Interface: painéis, gráficos, indicadores, camadas e ferramentas

O app desenha tudo com a aparência dele, no computador, no celular e no navegador. A extensão descreve
o conteúdo e recebe o que o usuário fez.

## Painéis

Declare `{"kind": "painel", "id": "principal"}`. Quando o painel está na tela, chega `MLX_EV_PANEL`
(a cada quadro): responda com a lista de widgets.

```c
mlx_writer w;
mlx_w_init(&w, buffer, sizeof buffer);
mlx_ui_begin(&w);
mlx_ui_section(&w, 1, "Resultado", "resumo opcional", 1);
mlx_ui_kpi(&w, "Tempo médio", "12,4 min", 0);
mlx_ui_button(&w, 10, "Calcular", 1, 0);
mlx_ui_end_section(&w);
mlx_ui_send(MLX_FN_UI_PANEL, "principal", &w, scratch, sizeof scratch);
```

```cpp
mlx::Ui ui;
ui.section(1, "Resultado");
ui.kpi("Tempo médio", "12,4 min");
ui.button(10, "Calcular", true);
ui.endSection();
ui.send(id);
```

Widgets: seção (recolhível), texto (normal, apagado, título, bom, alerta, ruim), botão, chave, número
(com mínimo, máximo, passo e casas), escolha (botões lado a lado), lista de seleção, campo de texto,
indicador, tabela, gráfico de linhas, barras, progresso, separador e "mesma linha". Os ids dos widgets
são seus e voltam em `MLX_EV_UI` com o tipo da ação (clique, chave, número, escolha, texto) e o valor.
O valor que o usuário muda aparece na hora; a sua resposta chega no quadro seguinte.

Os formatos exatos estão no fim de `sdk/include/mobilog_ext.h` (seção *Widgets*).

## Gráficos

Declare `{"kind": "grafico", "id": "historico"}`. Quando ele está na tela, chega `MLX_EV_CHART`:
responda com `ui.chart` (os mesmos widgets: normalmente um gráfico de linhas ou barras). O usuário pode
fixá-lo no mapa.

## Indicadores na barra

`ui.kpis` põe até 6 indicadores na barra de cima (com rótulo curto para o celular e dica). Não precisa
declarar no manifesto.

## Camadas

Declare `{"kind": "camada", "id": "desenho"}` e peça `mapa.desenhar`. `draw.set_layer` manda o
conteúdo inteiro da camada de uma vez: linhas (cor, largura, pontos), marcadores (ponto, ícone, cor,
rótulo), áreas (preenchimento, contorno, pontos) e cores por trecho de via (0 vermelho a 1 verde).
Mande de novo só quando mudar. O usuário liga e desliga a camada em **Camadas do mapa**.

## Ferramentas

Declare `{"kind": "ferramenta", "id": "marcar"}` e peça `mapa.ferramentas`. `ui.tool` liga a
ferramenta (normalmente por um botão do painel). Com ela ligada, toques e arrastes no mapa chegam em
`MLX_EV_TOOL` com a ação (`MLX_TOOL_TAP`, `GRAB`, `DRAG_BEGIN`, `DRAG_MOVE`, `DRAG_END`, `DRAG_CANCEL`,
`OFF`) e o ponto (lon, lat). Responda 1 para tomar o gesto. Toques sem ferramenta chegam como
`MLX_EV_TAP`.

## Outros

- `ui.legend`: itens de legenda (cor, quadrado ou linha, rótulo).
- `ui.toast`: um aviso rápido (até 6 por minuto).
- `ui.focus`: leva o mapa até um ponto (com zoom).

## Ícones

O ícone da extensão (`icon.svg`) aparece na loja, no gerenciador e na sub-aba dos painéis dela. Veja o
guia de ícones em [diretrizes.md](diretrizes.md#ícone).
