# Resolvedores de roteirização (VRP)

Um resolvedor é um algoritmo de roteirização que aparece em **Extensões → Algoritmos** e na
**Bancada**, ao lado dos outros. O app monta a instância (matriz de tempos do trânsito de agora,
demandas, janelas, frota), avalia cada solução com as próprias contas e mostra as rotas no mapa.

## Declarar

No manifesto:

```json
"contributes": [{"kind": "resolvedor", "id": "meu", "title": "Meu resolvedor"}]
```

Em `mlx_init`, `solver.declare` com o id, os recursos (`MLX_VRP_CAPACITY`, `MLX_VRP_TIME_WINDOWS`,
`MLX_VRP_PICKUP_DELIVERY`, `MLX_VRP_HETEROGENEOUS`, `MLX_VRP_DYNAMIC`, `MLX_VRP_PARALLEL`...) e até 32
parâmetros (chave, rótulo, dica, padrão, mínimo, máximo, passo, casas), que aparecem em Algoritmos.

## Resolver

Cada execução chama `mlx_task(MLX_TASK_SOLVE_VRP, dados, n)` numa **instância nova** do módulo. Os dados
são o id do resolvedor e a instância (formato em `sdk/include/mobilog_ext.h`; em C++,
`mobilog/vrp/VrpWire.h` faz a leitura: `decodeVrpInstance`). `solver.params` traz a semente, o tempo
limite, o tempo decorrido e os parâmetros.

Mande cada solução melhor com `solver.offer` (as rotas: listas de locais 1..m-1). O app confere a
viabilidade e responde o custo dele, se é viável e se melhorou. Pare quando `mobilog_cancelled()`
disser 1 (tempo limite ou Cancelar).

## Em paralelo e em fatias

Com `MLX_VRP_PARALLEL`, o app roda uma cópia do módulo por thread (até 8), cada uma com a sua semente;
`solver.params` traz `_copy` (esta cópia), `_copies` (quantas) e `_sliced` (1: o app pede fatias).

Se `MLX_TASK_SOLVE_VRP` devolver 1, a busca só se prepara, e o app chama
`mlx_task(MLX_TASK_SOLVE_STEP, 0, ms)` repetidas vezes: trabalhe uns `ms` milissegundos e devolva 1
enquanto houver o que fazer (0 no fim). Assim a simulação não perde as threads. Conte as iterações com
`solver.progress`, para a Bancada mostrar a convergência.

## Compilar

Resolvedores em C++ costumam usar a biblioteca padrão. No `build.json`:

```json
{"runtime": "wasi", "optimize": "O3", "libraries": ["vrp"]}
```

`"vrp"` junta o modelo de roteirização do SDK (`sdk/contracts`): `Vrp.h` (instância, solução,
`evaluate`), `VrpWire.h` (os blocos) e `Solver.h` (a interface dos resolvedores).

## Testar

`mobilog_ext test` roda cada resolvedor declarado num CVRP pequeno (20 clientes, 6 veículos) por 1 s e
confere que ele oferece soluções viáveis. No app, a **Bancada** compara resolvedores em várias sementes.

## Exemplos

- `extensions/mobilog.exemplo.busca-local`: em C, sem biblioteca: vizinho mais próximo, 2-opt, Or-opt.
- `extensions/mobilog.rotas`: em C++, cinco algoritmos (Clarke-Wright, ALNS, HGS...), em paralelo e em
  fatias.
