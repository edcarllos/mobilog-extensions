# SDK de extensões do Mobilog

Extensões acrescentam ao Mobilog problemas, métodos de resolução, análises e ferramentas que usam a
simulação e o mapa, sem mudar nem recompilar o app. Uma extensão é um módulo **WebAssembly** num pacote
`.mlx`, que o usuário importa em **Sistema → Extensões**. Ela roda isolada e só faz o que as permissões
aceitas na instalação deixarem.

Este SDK tem:

| Arquivo | O que é |
|---|---|
| `mobilog_ext.h` | A interface (ABI v1) em C: funções, blocos, eventos, utilitários para montar e ler blocos e painéis, um alocador. É a referência completa da API. |
| `mobilog_ext.hpp` | Uma camada em C++ por cima (classes `Extension`, `Ui`, `Layer`, `Writer`, `Reader`), sem a biblioteca padrão. |
| `cmake/wasi-toolchain.cmake` | O toolchain do CMake para compilar extensões. |
| `cmake/MobilogExt.cmake` | `mobilog_ext_add(...)`: compila e empacota. |
| `template/` | Um projeto mínimo em C++ para copiar. |

Os quatro exemplos do Mobilog estão em `examples/extensions/` (em C; os pacotes prontos vão na pasta
`extensoes-exemplo/` dos pacotes do Mobilog e na página da versão): um resolvedor de
roteirização (**busca-local**), uma análise do trânsito com camada, painel e gráfico (**pontos-criticos**),
uma ferramenta de mapa com tarefa em segundo plano (**bases**) e veículos próprios na simulação
(**onibus**).

---

## 1. Começar

Precisa de um **clang com o alvo wasm32**: o do [wasi-sdk](https://github.com/WebAssembly/wasi-sdk)
(com a biblioteca C/C++) ou o do [emsdk](https://emscripten.org) (`$EMSDK/upstream/bin/clang`, sem biblioteca
C: use só o SDK). E do `mobilog_ext`, a ferramenta de pacotes, que vem com o Mobilog (`tools/`) ou é
compilada com ele.

```bash
cp -r sdk/template minha-extensao
cd minha-extensao
# troque o id e o nome em manifest.json
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=../sdk/cmake/wasi-toolchain.cmake \
      -DMOBILOG_SDK=../sdk -DMOBILOG_EXT_TOOL=/caminho/para/mobilog_ext
cmake --build build
```

Sai `build/minha-extensao.mlx`. No app: **Sistema → Extensões → Adicionar extensão → De um arquivo (.mlx)**. Ela aparece como
**Não verificada** (veja *Assinar*), com uma sub-aba na aba **Extensões**.

Sem CMake, a linha de compilação é esta (a do `examples/extensions/build.sh`):

```bash
clang --target=wasm32 -mcpu=mvp -mbulk-memory -msign-ext -mnontrapping-fptoint -O2 \
      -nostdlib -fno-builtin -I sdk -Wl,--no-entry -Wl,--strip-all -Wl,--max-memory=67108864 \
      -o pacote/extension.wasm minha.c
mobilog_ext pack pacote -o minha.mlx
```

`--max-memory` é obrigatório (o app recusa memória sem máximo) e deve ser o de `limits.memoryMB`.
Sem a biblioteca C, `-fno-builtin` impede o compilador de trocar laços por chamadas a `strlen`/`memset`.

**C++ com a biblioteca padrão** (`std::vector`, `std::sort`, `<random>`...): use o wasi-sdk (testado
com o 34; `WASI_SDK_PREFIX=<pasta>` no CMake). O toolchain já passa o que ele precisa:
`--target=wasm32-wasip1 -mexec-model=reactor`, `-fno-exceptions -fno-rtti`, `--no-wasm-opt` (sem ele, o
clang roda o `wasm-opt` que estiver no PATH e o resultado muda de máquina para máquina) e
`-Wl,--compress-relocations` (sem ele, as bibliotecas do wasi-sdk deixam um índice de tabela que o app
recusa: "o módulo usa várias memórias ou tabelas"). Exporte `mlx_alloc`/`mlx_free` com `malloc`/`free`
em vez de `MLX_DEFINE_ALLOCATOR`. O pacote Rotas e entregas (`examples/extensions/rotas`) é um exemplo
completo: cinco algoritmos de roteirização, cópias em paralelo e fatias (`solver.progress`,
`MLX_TASK_SOLVE_STEP`).

## 2. O pacote e o manifesto

Um `.mlx` é um zip com:

| Arquivo | |
|---|---|
| `manifest.json` | obrigatório |
| `extension.wasm` | o módulo (o nome está em `entry`) |
| `help.md` | o manual, que aparece na Ajuda do app (opcional) |
| `icon.svg` / `icon.png` | opcional |
| `LICENSE` | opcional |
| `signature.json` | a assinatura (`mobilog_ext sign`) |

```json
{
  "id": "br.ufsb.minha-extensao",
  "name": "Minha extensão",
  "version": "1.0.0",
  "apiVersion": 1,
  "minAppVersion": "0.12.0",
  "author": "Quem fez",
  "license": "MIT",
  "description": "Uma frase sobre o que ela faz.",
  "entry": "extension.wasm",
  "help": "help.md",
  "permissions": ["mapa.ler", "simulacao.ler", "mapa.desenhar"],
  "limits": {"memoryMB": 64},
  "contributes": [
    {"kind": "painel", "id": "principal", "title": "Minha extensão"},
    {"kind": "camada", "id": "desenho", "title": "Desenho da minha extensão"}
  ]
}
```

- **id:** notação reversa (`br.ufsb.nome`), letras minúsculas, números e `-`. Os ids `mobilog.*` são do Mobilog.
- **version:** semver. Uma versão maior instala como atualização, pedindo só as permissões novas. A
  anterior fica guardada até a nova ligar sem erro: se a nova falhar, o gerenciador volta à anterior.
- **contributes:** o que ela põe na interface. Tipos: `painel` (sub-aba da aba Extensões), `camada`
  (em Camadas do mapa), `ferramenta` (toques e arrastes no mapa), `grafico` (fixável no mapa) e
  `resolvedor` (VRP: aparece em Algoritmos e na Bancada). Indicadores na barra e itens de legenda não
  precisam ser declarados.

### Permissões

| Permissão | Libera |
|---|---|
| `mapa.ler` | malha, nomes, lugares, zonas, bairros, área, rotas e matrizes de tempo, resumo do cenário |
| `simulacao.ler` | estatísticas, velocidades, tempos e passagens por trecho, posições dos veículos |
| `mapa.desenhar` | as próprias camadas: linhas, marcadores, áreas, cores nas vias |
| `mapa.ferramentas` | ligar a própria ferramenta de mapa |
| `cenario.propor` | propor mudanças no cenário, que o usuário vê no Assistente e aplica (ou não) |
| `cenario.alterar` | aplicar mudanças no cenário na hora (com desfazer) |
| `veiculos.controlar` | criar veículos próprios na simulação, mandá-los por rotas e removê-los |
| `simulacao.ensaiar` | ensaios: a simulação copiada com e sem uma mudança, comparadas |
| `arquivos.exportar` / `arquivos.importar` | reservadas |

O usuário pode negar uma permissão na instalação ou revogá-la depois: a função que precisava dela
responde `MLX_ERR_DENIED` na hora. Trate isso (mostre um aviso no painel). Sem permissão nenhuma, a
extensão ainda tem o próprio painel, indicadores, dados guardados e tarefas.

### Limites

- Memória: a de `limits.memoryMB` (64 MB se omitido, até 512 MB), reservada sob demanda.
- Ganchos: acima de 50 ms o app anota no registro; acima de 250 ms três vezes, desliga a extensão; acima
  de 2 s, para na hora e a marca como travada.
- Tarefas: duas ao mesmo tempo, com tempo limite.
- Dados guardados: 20 MB. Veículos: 500. Mensagens: 6 por minuto. Registro: 50 linhas por segundo.
- Matriz de tempos: 100 locais nos ganchos, 2000 nas tarefas.

## 3. O ciclo de vida

O módulo exporta:

| Exportação | |
|---|---|
| `mlx_api_version()` | devolve `MLX_API_VERSION` |
| `mlx_alloc(n)` / `mlx_free(p)` | memória onde o app escreve pedidos e eventos (`MLX_DEFINE_ALLOCATOR` define as duas) |
| `mlx_init()` | ao ligar: 0 = ok. Declare resolvedores e se inscreva em eventos aqui. |
| `mlx_event(tipo, dados, n)` | os eventos (opcional) |
| `mlx_task(tipo, dados, n)` | trabalho em segundo plano (opcional) |
| `mlx_shutdown()` | ao desligar (opcional) |

**Ganchos** (`mlx_init`, `mlx_event`) rodam na thread principal, entre dois quadros: precisam ser rápidos.
**Tarefas** (`task.start` → `mlx_task`) rodam numa outra instância do módulo, em segundo plano: não
compartilham memória com a dos ganchos (recebem bytes, devolvem bytes com `task.result`) e leem o mapa
como ele estava quando começaram. Quando acaba, chega `MLX_EV_TASK_DONE`; `task.poll` traz o resultado.

### Eventos

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

## 4. Chamando o app

Quase tudo passa por `mobilog_call(fn, pedido, tamanho, resposta, capacidade)`. A resposta é o tamanho
(pode ser maior que a capacidade: `mobilog_take` a traz de novo; `mlx_call_big` faz isso por você) ou um
erro `MLX_ERR_*`. O formato de cada pedido e resposta está em `mobilog_ext.h`, ao lado do número da função.

```c
/* O cruzamento mais perto de um ponto */
double ponto[2] = {-39.28, -14.79};
uint8_t r[8];
if (mobilog_call(MLX_FN_MAP_NEAREST_NODE, ponto, 16, r, sizeof r) == 4) { /* u32 nó em r */ }
```

Em C++:

```cpp
const uint32_t no = mlx::nearestNode(-39.28, -14.79);
mlx::Answer a = mlx::call(MLX_FN_ROUTE, mlx::Writer().u32(de).u32(para).u8(0));
if (a.ok()) { float custo = a.in.f32(); /* ... */ }
```

Famílias de funções (veja os números e blocos em `mobilog_ext.h`):

- **Mapa:** `map.info`, `map.edges`, `map.nodes`, `map.names`, `map.edge_points`, `map.nearest_edge`,
  `map.nearest_node`, `map.places`, `map.zones`, `map.neighbourhoods`, `map.area`, `map.edge_ref` e
  `map.resolve_edge` (referência a um trecho que sobrevive a edições da malha).
- **Rotas:** `route.find`, `route.costs_from`, `route.matrix` — calculadas pelo app, em código nativo. Custo
  0 é o tempo esperado agora (cenário + trânsito medido), 1 com vias livres, 2 em metros.
- **Simulação:** `sim.info`, `sim.speed_ratios`, `sim.passages`, `sim.edge_times`, `sim.vehicles`.
- **Desenho:** `draw.set_layer` (o conteúdo inteiro de uma camada), `draw.clear_layer`.
- **Interface:** `ui.panel`, `ui.chart`, `ui.kpis`, `ui.legend`, `ui.toast`, `ui.tool`, `ui.focus`.
- **Cenário:** `scenario.summary`, `scenario.data_get`/`data_set` (a seção dela no cenário: vai junto quando
  o cenário é salvo, exportado ou sincronizado, mesmo onde a extensão não está instalada),
  `scenario.propose`, `scenario.change`, `scenario.undo`. As mudanças usam o formato de ações do
  Assistente (`{"acoes": [...]}`): o app acha ruas, cruzamentos e lugares pelos nomes.
- **Veículos:** `vehicle.create`, `vehicle.depart`, `vehicle.states`, `vehicle.hide`, `vehicle.remove`.
- **Tarefas e ensaios:** `task.start`, `task.poll`, `task.cancel`; `experiment.start`, `experiment.poll`,
  `experiment.cancel`.
- **Resolvedores:** `solver.declare` (em `mlx_init`), e na execução `solver.params` e `solver.offer`.

`mobilog_has("route.matrix", 12)` diz se o app tem uma função (versões mais novas acrescentam funções;
os números nunca mudam).

## 5. Painéis

O app desenha os painéis com a aparência dele. A extensão manda uma lista de widgets (texto, botões,
chaves, números, escolhas, campos, indicadores, tabelas, gráficos, progresso), com ids dela, e recebe o
que o usuário fez em `MLX_EV_UI`. O valor que o usuário muda aparece na hora; a resposta da extensão
chega no quadro seguinte.

```c
mlx_writer w;
mlx_w_init(&w, buffer, sizeof buffer);
mlx_ui_begin(&w);
mlx_ui_section(&w, 1, "Resultado", "", 1);
mlx_ui_kpi(&w, "Tempo médio", "12,4 min", 0);
mlx_ui_button(&w, 10, "Calcular", 1, 0);
mlx_ui_end_section(&w);
mlx_ui_send(MLX_FN_UI_PANEL, "principal", &w, scratch, sizeof scratch);
```

## 6. Resolvedores de roteirização

Declare no manifesto (`{"kind": "resolvedor", "id": "meu", "title": "Meu resolvedor"}`) e em `mlx_init`
com `solver.declare` (recursos `MLX_VRP_*` e parâmetros, que aparecem em Algoritmos). Cada execução chama
`mlx_task(MLX_TASK_SOLVE_VRP, ...)` numa instância nova, com a instância (matriz de tempos, demandas,
janelas, frota). Mande cada solução melhor com `solver.offer`: o app confere a viabilidade e calcula o
custo com as próprias contas. Pare quando `mobilog_cancelled()` disser 1 (tempo limite ou Cancelar).
Veja `examples/extensions/busca-local`.

**Em paralelo e em fatias** (como faz Rotas e entregas): com `MLX_VRP_PARALLEL` nos recursos, o app roda
uma cópia do módulo por thread (até 8), cada uma com a sua semente; `solver.params` traz também
`_copy` (o número desta cópia), `_copies` (quantas) e `_sliced` (1: o app pede fatias). Se
`MLX_TASK_SOLVE_VRP` devolver 1, a busca só se prepara e o app chama `mlx_task(MLX_TASK_SOLVE_STEP, 0,
ms)` repetidas vezes: trabalhe uns `ms` milissegundos e devolva 1 enquanto houver o que fazer (0 no
fim). Assim a simulação não perde as threads. Conte as iterações com `solver.progress` (um `u64` com
as feitas desde a última vez) para a convergência ao vivo; sem ele, cada `solver.offer` conta uma. A
instância vem no formato 2 (`mobilog_ext.h`: tipos de veículo com fator de pedágio e camada de
tempos, e uma solução inicial opcional no fim).

## 7. Assinar

```bash
mobilog_ext keygen minha-chave --name "Minha organização"   # minha-chave.secret e minha-chave.pub
mobilog_ext pack pasta --key minha-chave.secret -o minha.mlx
mobilog_ext verify minha.mlx --trust minha-chave.pub
mobilog_ext inspect minha.mlx                                 # manifesto, recursos e importações do módulo
```

Quem confia em você acrescenta o `.pub` em **Sistema → Extensões → Chaves confiáveis**; aí a extensão
aparece como **Verificada**. A assinatura cobre todos os arquivos do pacote: um arquivo trocado ou a mais
a invalida. Guarde o `.secret` fora do repositório.

## 8. Depurar

- **Sistema → Extensões → (a extensão) → Registro:** o que ela escreveu com `mobilog_log` (ou `printf`, com a
  biblioteca C) e os erros: chamada lenta, travou, trap (com a mensagem do motor).
- `mobilog_ext inspect` mostra por que um módulo é recusado: memória sem máximo, um recurso que o app não
  aceita (SIMD, threads, exceções, memory64, multivalue, reference-types) ou uma importação desconhecida.
- Uma extensão que falha é desligada sozinha; o app segue. Corrija, gere de novo e importe: a versão nova
  substitui a anterior.
- `undefined symbol: strlen` (ou `memset`, `memcmp`) ao ligar: falta `-fno-builtin`, ou use as funções do SDK
  (`mlx_strlen`, `mlx_str_is`).

## 9. Desempenho

O código da extensão roda compilado: no PC e no Android, pelo wasmtime (Cranelift); no navegador, pelo
motor dele (também no app de iPhone e iPad, que roda o Mobilog web pelo motor do Safari). Fica entre 1× e
2× o mesmo código em C compilado dentro do app. O que é pesado e comum (rotas, matrizes de tempo) o app faz em código nativo, pelas funções
`route.*`. Cada chamada ao app custa algumas dezenas de nanossegundos: prefira pedir listas inteiras a
pedir item por item num laço apertado. **Sistema → Extensões → Desempenho das extensões** mede tudo isso
no aparelho.
