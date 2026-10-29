# API reference

> Written by `mobilog_ext doc api` from `sdk/include/mobilog_ext.h` and the app's table (`mlx/Api.h`). Do not edit by hand: the CI checks it.

Each function is called with `mobilog_call(number, request, size, answer, capacity)` (C++: `mlx::call`). Blocks use `u8`, `u32`, `i32`, `u64`, `f32`, `f64` (little-endian), `texto` = text (u32 size + UTF-8), `ponto` = point (f64 lon, f64 lat) and `lista` = list (u32 count + items). **Where:** hooks = `mlx_init`/`mlx_event` (main thread), tasks = `mlx_task`, solvers = a route solver's run. The layout notes come from the SDK header, in Portuguese (the types and order are what matter).

| Function | No. | Permission | Where | Request → answer |
|---|---|---|---|---|
| **The extension itself** | | | | |
| `task.result` `MLX_FN_TASK_RESULT` | 1 | none | tasks | bytes do resultado → nada |
| `echo` `MLX_FN_ECHO` | 2 | none | hooks, tasks, solvers | bytes → os mesmos bytes |
| `events.subscribe` `MLX_FN_SUBSCRIBE` | 3 | none | hooks | u32 máscara (1 << MLX_EV_*) dos eventos que vêm sem pedir → nada |
| **Map (mapa.ler)** | | | | |
| `map.info` `MLX_FN_MAP_INFO` | 100 | `mapa.ler` | hooks, tasks, solvers | nada → u32 nós, u32 trechos, u32 nomes, f64 lonMín, f64 latMín, f64 lonMáx, f64 latMáx, texto área, u64 versão da malha |
| `map.edges` `MLX_FN_MAP_EDGES` | 101 | `mapa.ler` | hooks, tasks, solvers | nada → lista de 20 bytes: u32 de, u32 para, u8 classe, u8 faixas, u8 marcas (1 fechado, 2 desenhado, 4 alterado), u8 0, f32 limite m/s, f32 comprimento m, u32 nome (0xFFFFFFFF sem nome) |
| `map.edge_points` `MLX_FN_MAP_EDGE_POINTS` | 102 | `mapa.ler` | hooks, tasks, solvers | u32 trecho → lista de pontos |
| `map.nodes` `MLX_FN_MAP_NODES` | 103 | `mapa.ler` | hooks, tasks, solvers | nada → lista de pontos |
| `map.names` `MLX_FN_MAP_NAMES` | 104 | `mapa.ler` | hooks, tasks, solvers | nada → lista de textos |
| `map.nearest_edge` `MLX_FN_MAP_NEAREST_EDGE` | 105 | `mapa.ler` | hooks, tasks, solvers | ponto, f32 metros no máximo → u32 trecho, f32 ao longo, f32 distância |
| `map.nearest_node` `MLX_FN_MAP_NEAREST_NODE` | 106 | `mapa.ler` | hooks, tasks, solvers | ponto → u32 nó |
| `map.places` `MLX_FN_MAP_PLACES` | 107 | `mapa.ler` | hooks | nada → lista: texto nome, texto categoria, ponto, f32 viagens/dia, u8 próprio |
| `map.zones` `MLX_FN_MAP_ZONES` | 108 | `mapa.ler` | hooks | nada → lista: texto nome, f32 viagens/dia, lista de pontos |
| `map.neighbourhoods` `MLX_FN_MAP_NEIGHBOURHOODS` | 109 | `mapa.ler` | hooks | nada → lista: texto nome, ponto centro, lista de u32 trechos |
| `map.area` `MLX_FN_MAP_AREA` | 110 | `mapa.ler` | hooks | nada → lista de anéis (cada um lista de pontos) |
| `map.edge_ref` `MLX_FN_MAP_EDGE_REF` | 111 | `mapa.ler` | hooks, tasks, solvers | u32 trecho → u32 trecho real, f64 lon/lat início, f64 lon/lat fim |
| `map.resolve_edge` `MLX_FN_MAP_RESOLVE_EDGE` | 112 | `mapa.ler` | hooks, tasks, solvers | a referência acima → u32 trecho na malha de agora |
| **Routes (mapa.ler)** | | | | |
| `route.find` `MLX_FN_ROUTE` | 120 | `mapa.ler` | hooks, tasks, solvers | u32 de (nó), u32 para, u8 custo → f32 custo, f32 metros, lista de u32 trechos |
| `route.costs_from` `MLX_FN_ROUTE_COSTS_FROM` | 121 | `mapa.ler` | hooks, tasks, solvers | u32 de, u8 custo → lista de f32 por nó (infinito: inalcançável) |
| `route.matrix` `MLX_FN_ROUTE_MATRIX` | 122 | `mapa.ler` | hooks, tasks, solvers | u8 custo, lista de u32 nós → lista de n×n f32 (linha = origem); até 100 nós nos ganchos, 2000 nas tarefas |
| **Simulation (simulacao.ler; hooks only)** | | | | |
| `sim.info` `MLX_FN_SIM_INFO` | 130 | `simulacao.ler` | hooks | nada → f64 tempo simulado s, f64 hora do dia, f64 ms por passo, i32 dia da semana, u32 veículos ativos, u32 chegadas, u32 alvo, f32 km/h médios, u64 versão, u8 pausada |
| `sim.speed_ratios` `MLX_FN_SIM_SPEED_RATIOS` | 131 | `simulacao.ler` | hooks | nada → lista de f32 por trecho: velocidade / limite agora |
| `sim.passages` `MLX_FN_SIM_PASSAGES` | 132 | `simulacao.ler` | hooks | nada → lista de u32 por trecho: veículos desde o início |
| `sim.edge_times` `MLX_FN_SIM_EDGE_TIMES` | 133 | `simulacao.ler` | hooks | nada → lista de f32 por trecho: tempo de travessia medido, s |
| `sim.vehicles` `MLX_FN_SIM_VEHICLES` | 134 | `simulacao.ler` | hooks | nada → lista de 24 bytes: ponto, f32 rumo (rad), u8 tipo (0 fundo, 1 frota, 2 externo), 3 × u8 0 |
| **Drawing on the map (mapa.desenhar; hooks only)** | | | | |
| `draw.set_layer` `MLX_FN_DRAW_SET_LAYER` | 140 | `mapa.desenhar` | hooks | texto id da camada, então: linhas: lista de (u32 cor 0xAABBGGRR, f32 largura px, lista de pontos) marcadores: lista de (ponto, texto ícone, u32 cor, texto rótulo) áreas: lista de (u32 preenchimento, u32 contorno, f32 contorno px, lista de pontos) cores por trecho: lista de f32 (0 vermelho .. 1 verde; vazia: nenhuma) → nada |
| `draw.clear_layer` `MLX_FN_DRAW_CLEAR_LAYER` | 141 | `mapa.desenhar` | hooks | texto id → nada |
| **Interface (hooks only)** | | | | |
| `ui.panel` `MLX_FN_UI_PANEL` | 150 | none | hooks | texto painel, bytes (u32 tamanho + widgets) → nada |
| `ui.kpis` `MLX_FN_UI_KPIS` | 151 | none | hooks | lista (até 6): texto rótulo, texto valor, texto rótulo curto (celular; vazio: não aparece), texto dica, u8 tom (0 normal, 1 bom, 2 alerta) → nada |
| `ui.legend` `MLX_FN_UI_LEGEND` | 152 | none | hooks | lista (até 12): u32 cor, u8 quadrado, texto rótulo → nada |
| `ui.toast` `MLX_FN_UI_TOAST` | 153 | none | hooks | u8 tipo (0 info, 1 ok, 2 aviso), texto (até 6 por minuto) → nada |
| `ui.chart` `MLX_FN_UI_CHART` | 154 | none | hooks | texto gráfico, bytes (widgets) → nada |
| `ui.tool` `MLX_FN_UI_TOOL` | 155 | `mapa.ferramentas` | hooks | texto ferramenta, u8 ligada → nada |
| `ui.focus` `MLX_FN_UI_FOCUS` | 156 | none | hooks | ponto, f32 zoom (0: mantém) → nada: o mapa vai até lá |
| **Scenario (hooks only)** | | | | |
| `scenario.summary` `MLX_FN_SCENARIO_SUMMARY` | 160 | `mapa.ler` | hooks | nada → texto JSON {"nome", "itens": [{"id", "linha"}], "ajustes"} |
| `scenario.data_get` `MLX_FN_SCENARIO_DATA_GET` | 161 | none | hooks | nada → texto JSON da seção da extensão no cenário ("" sem) |
| `scenario.data_set` `MLX_FN_SCENARIO_DATA_SET` | 162 | none | hooks | texto JSON (até 1 MB; "" apaga) → nada |
| `scenario.propose` `MLX_FN_SCENARIO_PROPOSE` | 163 | `cenario.propor` | hooks | texto título, texto ações → texto relato (o usuário decide) |
| `scenario.change` `MLX_FN_SCENARIO_CHANGE` | 164 | `cenario.alterar` | hooks | texto título, texto ações → u32 aplicadas, texto relato; tudo ou nada: com uma ação ambígua ou não encontrada, nada muda |
| `scenario.undo` `MLX_FN_SCENARIO_UNDO` | 165 | `cenario.alterar` | hooks | nada → texto relato (desfaz a última mudança dela) |
| **Managed vehicles (veiculos.controlar; hooks only; up to 500)** | | | | |
| `vehicle.create` `MLX_FN_VEHICLE_CREATE` | 170 | `veiculos.controlar` | hooks | u32 nó, f32 fator de velocidade, u8 tipo (0 moto, 1 carro, 2 van, 3 caminhão), u8 cor → u32 veículo |
| `vehicle.depart` `MLX_FN_VEHICLE_DEPART` | 171 | `veiculos.controlar` | hooks | u32 veículo, lista de u32 trechos encadeados → nada |
| `vehicle.remove` `MLX_FN_VEHICLE_REMOVE` | 172 | `veiculos.controlar` | hooks | u32 veículo → nada |
| `vehicle.states` `MLX_FN_VEHICLE_STATES` | 173 | `veiculos.controlar` | hooks | nada → lista de 20 bytes: u32 veículo, u8 estado (0 surgindo, 1 estacionado, 2 rodando, 3 sem espaço), u8 oculto, 2 × u8 0, u32 trecho, f32 posição m, f32 m/s |
| `vehicle.hide` `MLX_FN_VEHICLE_HIDE` | 174 | `veiculos.controlar` | hooks | u32 veículo, u8 oculto → nada |
| **Background tasks (hooks only; up to 2 at a time)** | | | | |
| `task.start` `MLX_FN_TASK_START` | 180 | none | hooks | i32 tipo (>= 0), f64 tempo limite s, bytes entrada → u32 tarefa |
| `task.poll` `MLX_FN_TASK_POLL` | 181 | none | hooks | u32 tarefa → u8 estado (0 rodando, 1 pronta, 2 falhou), f64 progresso, i32 código de mlx_task, texto erro, bytes resultado (uma vez: depois de pronta, a tarefa sai) |
| `task.cancel` `MLX_FN_TASK_CANCEL` | 182 | none | hooks | u32 tarefa → nada |
| **Experiments (simulacao.ensaiar; hooks only)** | | | | |
| `experiment.start` `MLX_FN_EXPERIMENT_START` | 190 | `simulacao.ensaiar` | hooks | f64 minutos simulados (1..120), texto ações → u32 ensaio |
| `experiment.poll` `MLX_FN_EXPERIMENT_POLL` | 191 | `simulacao.ensaiar` | hooks | u32 ensaio → u8 estado (0 rodando, 1 pronto, 2 falhou), f64 progresso, texto erro, lista: texto rótulo, texto unidade, f64 como está, f64 com a mudança, i8 melhor (-1 menor, 1 maior, 0 nenhum) |
| `experiment.cancel` `MLX_FN_EXPERIMENT_CANCEL` | 192 | `simulacao.ensaiar` | hooks | u32 ensaio → nada |
| **VRP solvers (manifest: "resolvedor")** | | | | |
| `solver.declare` `MLX_FN_SOLVER_DECLARE` | 200 | none | hooks | texto id, u32 recursos (MLX_VRP_*), lista (até 32) de parâmetros: texto chave, texto rótulo, texto dica, f32 padrão, f32 mín, f32 máx, f32 passo, u8 casas, u8 avançado |
| `solver.offer` `MLX_FN_SOLVER_OFFER` | 201 | none | solvers | rotas: lista de (lista de u32 locais 1..m-1) → f64 custo (do app), u8 viável, u8 melhorou |
| `solver.params` `MLX_FN_SOLVER_PARAMS` | 202 | none | solvers | nada → u64 semente, f64 tempo limite s, f64 decorrido s, lista: texto chave, f32. Chaves do app na lista: "_copy" e "_copies" (esta cópia, de quantas: com MLX_VRP_PARALLEL), "_sliced" (1: o app pede fatias, ver MLX_TASK_SOLVE_STEP) |
| `solver.progress` `MLX_FN_SOLVER_PROGRESS` | 203 | none | solvers | u64 iterações desde o último aviso → nada (a Bancada mostra iterações/s) |
