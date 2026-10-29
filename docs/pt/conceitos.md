# Conceitos

## O módulo e o que ele exporta

| Exportação | Quando o app chama |
|---|---|
| `mlx_api_version()` | ao carregar: devolva `MLX_API_VERSION` |
| `mlx_alloc(n)` / `mlx_free(p)` | memória onde o app escreve pedidos e eventos (`MLX_DEFINE_ALLOCATOR` define as duas em C; com a biblioteca C, use `malloc`/`free`) |
| `mlx_init()` | ao ligar: 0 = ok. Declare resolvedores e se inscreva em eventos aqui |
| `mlx_event(tipo, dados, n)` | os eventos (opcional) |
| `mlx_task(tipo, dados, n)` | trabalho em segundo plano (opcional) |
| `mlx_shutdown()` | ao desligar (opcional) |

Em C++, a classe `mlx::Extension` (`mobilog_ext.hpp`) faz isso por você: escreva `init()`,
`panel(id)`, `ui(id, ação)`, `tool(...)`, `task(...)` e use `MLX_EXTENSION(SuaClasse)`.

## Ganchos, tarefas e resolvedores

- **Ganchos** (`mlx_init`, `mlx_event`) rodam na thread principal, entre dois quadros. Precisam ser
  rápidos: acima de 50 ms o app anota no registro; acima de 250 ms três vezes, desliga a extensão;
  acima de 2 s, para na hora e a marca como travada.
- **Tarefas** (`task.start` → `mlx_task`) rodam numa **outra instância** do módulo, em segundo plano.
  Não compartilham memória com a dos ganchos: recebem bytes e devolvem bytes (`task.result`). Leem o
  mapa como ele estava quando começaram. Ao terminar chega `MLX_EV_TASK_DONE`, e `task.poll` traz o
  resultado. Duas ao mesmo tempo, com tempo limite.
- **Resolvedores** de rotas rodam como tarefas que o próprio app inicia (veja
  [resolvedores.md](resolvedores.md)).

A coluna **Onde** da [referência da API](api.md) diz de onde cada função pode ser chamada; fora do
lugar, ela responde `MLX_ERR_UNAVAILABLE`.

## Eventos

| Evento | Quando | Resposta |
|---|---|---|
| `MLX_EV_PANEL` | um painel dela está na tela (a cada quadro) | mande o conteúdo (`ui.panel`) |
| `MLX_EV_UI` | o usuário mexeu num widget | |
| `MLX_EV_CHART` | um gráfico dela está na tela | mande o conteúdo (`ui.chart`) |
| `MLX_EV_TOOL` | toque ou arraste com a ferramenta dela ligada | 1 = tomado |
| `MLX_EV_TAP` | um toque que nenhuma ferramenta tomou | 1 = usado |
| `MLX_EV_FRAME` | a cada quadro (inscrição) | |
| `MLX_EV_VEHICLE_ARRIVED` | veículos dela chegaram ao fim da rota (inscrição) | |
| `MLX_EV_TASK_DONE` / `MLX_EV_EXPERIMENT_DONE` | uma tarefa ou ensaio terminou | |
| `MLX_EV_NETWORK` | a malha foi refeita: ids de trechos e nós podem ter mudado; os veículos dela sumiram | |
| `MLX_EV_SCENARIO` | o cenário mudou | |

Inscrição: `mobilog_call(MLX_FN_SUBSCRIBE, &mascara, 4, 0, 0)`, com `mascara = (1u << MLX_EV_FRAME) | ...`.

## Chamando o app

Quase tudo passa por `mobilog_call(fn, pedido, tamanho, resposta, capacidade)`. A resposta é o tamanho
(pode ser maior que a capacidade: `mobilog_take` a traz de novo; `mlx_call_big` faz isso por você) ou um
erro `MLX_ERR_*` (< 0):

| Erro | |
|---|---|
| `MLX_ERR_UNKNOWN` | este app não tem a função (confira antes com `mobilog_has`) |
| `MLX_ERR_DENIED` | a permissão não foi concedida, ou foi revogada |
| `MLX_ERR_INVALID` | pedido malformado |
| `MLX_ERR_LIMIT` | acima de um limite |
| `MLX_ERR_UNAVAILABLE` | agora não (sem área aberta, na thread errada) |
| `MLX_ERR_NOT_FOUND` | item que não existe |

```c
double ponto[2] = {-39.28, -14.79};
uint8_t r[8];
if (mobilog_call(MLX_FN_MAP_NEAREST_NODE, ponto, 16, r, sizeof r) == 4) { /* u32 nó em r */ }
```

```cpp
const uint32_t no = mlx::nearestNode(-39.28, -14.79);
mlx::Answer a = mlx::call(MLX_FN_ROUTE, mlx::Writer().u32(de).u32(para).u8(0));
if (a.ok()) { float custo = a.in.f32(); }
```

`mobilog_has("route.matrix", 12)` diz se o app tem uma função. Versões novas acrescentam funções; os
números nunca mudam.

## Memória e limites

- Memória: a de `limits.memoryMB` (64 MB se omitido, até 512 MB), reservada sob demanda.
- Dados guardados (`storage_get`/`storage_set`): 20 MB.
- Veículos próprios: 500. Mensagens (`ui.toast`): 6 por minuto. Registro: 50 linhas por segundo.
- Matriz de tempos: 100 locais nos ganchos, 2000 nas tarefas.

## O que a extensão guarda

- **Dados dela** (`storage_*`): ficam no aparelho, entre as sessões.
- **Seção dela no cenário** (`scenario.data_get`/`data_set`): vai junto quando o cenário é salvo,
  exportado ou sincronizado, até para quem não tem a extensão instalada.

## Linguagens

O SDK é em C (`mobilog_ext.h`) com uma camada em C++ (`mobilog_ext.hpp`). Qualquer linguagem que gere
um módulo WebAssembly 1.0 (com bulk memory, sign-ext, conversões sem trap e mutable globals), com as
exportações acima e importando só o módulo `mobilog` (e as poucas funções WASI listadas em
`mlx/include/mlx/Abi.h`), funciona. Veja a [FAQ](faq.md).
