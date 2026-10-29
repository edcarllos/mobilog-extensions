/*
 * mobilog_ext.h — a interface (ABI v1) entre o Mobilog e uma extensão WebAssembly.
 *
 * Uma extensão é um módulo .wasm (WebAssembly 1.0 com bulk-memory, sign-ext, nontrapping-fptoint e
 * mutable-globals; sem SIMD, threads, exceções ou memory64) que exporta a sua memória, com tamanho
 * máximo declarado, e as funções abaixo. Ela só fala com o app pelas importações "mobilog" — quase
 * tudo por uma função só, mobilog.call(fn, pedido, tamanho, resposta, capacidade), com blocos
 * binários descritos aqui.
 *
 * Blocos: números little-endian um após o outro, sem preenchimento; texto = u32 tamanho + bytes
 * UTF-8; ponto = f64 longitude, f64 latitude (graus, WGS-84); lista = u32 quantidade + itens.
 * mlx_writer e mlx_reader (no fim deste arquivo) montam e leem esses blocos.
 *
 * Exportações que a extensão deve ter:
 *   int32_t mlx_api_version(void)                 → MLX_API_VERSION
 *   void*   mlx_alloc(int32_t tamanho)             memória para o app escrever pedidos e eventos
 *   void    mlx_free(void* p)
 *   int32_t mlx_init(void)                         uma vez, ao ligar: 0 = ok
 * Opcionais:
 *   int32_t mlx_event(int32_t tipo, const void* dados, int32_t tamanho)   na thread principal
 *   int32_t mlx_task(int32_t tipo, const void* dados, int32_t tamanho)    em segundo plano
 *   void    mlx_shutdown(void)                     ao desligar
 * MLX_DEFINE_ALLOCATOR (abaixo) define mlx_alloc/mlx_free para quem compila sem a biblioteca C
 * (-nostdlib -fno-builtin: sem -fno-builtin o compilador troca laços por chamadas a strlen/memset).
 *
 * Ganchos (mlx_init, mlx_event) rodam na thread principal e precisam ser rápidos: acima de 50 ms o
 * app anota no registro; acima de 250 ms três vezes, a extensão é desligada; acima de 2 s ela é
 * parada na hora. Trabalho longo vai para tarefas (task.start → mlx_task), que rodam em outra
 * instância do módulo, sem memória compartilhada com a dos ganchos: recebem bytes e devolvem bytes
 * (task.result), e leem o mapa como ele estava quando a tarefa começou.
 */
#ifndef MOBILOG_EXT_H
#define MOBILOG_EXT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MLX_API_VERSION 1

#define MLX_IMPORT(name) __attribute__((import_module("mobilog"), import_name(name)))
#define MLX_EXPORT(name) __attribute__((export_name(name)))

/* ---- Importações ------------------------------------------------------------------------------ */

/* Registro da extensão (Sistema → Extensões): nível 0 depuração, 1 info, 2 aviso, 3 erro. */
MLX_IMPORT("log") void mobilog_log(int32_t level, const char* text, int32_t length);
MLX_IMPORT("time_ms") double mobilog_time_ms(void); /* relógio monotônico, ms */
/* O gerador aleatório da extensão (semeado pelo id; nunca o da simulação). */
MLX_IMPORT("random") uint32_t mobilog_random(void);
/* Dados próprios guardados entre sessões (até 20 MB): tamanho do valor (-1 sem a chave). */
MLX_IMPORT("storage_get") int32_t mobilog_storage_get(const char* key, int32_t keyLen, void* out, int32_t outCap);
MLX_IMPORT("storage_set") int32_t mobilog_storage_set(const char* key, int32_t keyLen, const void* value, int32_t valueLen);
MLX_IMPORT("progress") void mobilog_progress(double fraction); /* tarefas: 0..1 */
MLX_IMPORT("cancelled") int32_t mobilog_cancelled(void);      /* tarefas: 1 = pediram para parar */
/* 1 quando este app tem a função ("map.info", "vehicle.create"...). */
MLX_IMPORT("has") int32_t mobilog_has(const char* name, int32_t length);
/* Uma função da API: tamanho da resposta (pode ser maior que outCap: chame mobilog_take com mais
 * espaço), ou um erro MLX_ERR_* (< 0). */
MLX_IMPORT("call") int32_t mobilog_call(int32_t fn, const void* in, int32_t inLen, void* out, int32_t outCap);
/* A última resposta de novo (quando não coube). */
MLX_IMPORT("take") int32_t mobilog_take(void* out, int32_t outCap);

/* ---- Erros ------------------------------------------------------------------------------------ */

#define MLX_ERR_UNKNOWN (-1)     /* o app não tem a função */
#define MLX_ERR_DENIED (-2)      /* a permissão não foi concedida (ou foi revogada) */
#define MLX_ERR_INVALID (-3)     /* pedido malformado */
#define MLX_ERR_LIMIT (-4)       /* acima de um limite */
#define MLX_ERR_UNAVAILABLE (-5) /* agora não (sem área aberta, na thread errada...) */
#define MLX_ERR_NOT_FOUND (-6)   /* item que não existe (veículo, tarefa, painel...) */

/* ---- Funções (mobilog_call) ---------------------------------------------------------------------
 * Para cada uma: [onde] permissão — pedido → resposta.
 * Onde: G = ganchos (mlx_init, mlx_event), T = tarefas (mlx_task), R = resolvedores. */

/* A própria extensão */
#define MLX_FN_TASK_RESULT 1 /* [T] — bytes do resultado → nada */
#define MLX_FN_ECHO 2        /* [G T R] — bytes → os mesmos bytes */
#define MLX_FN_SUBSCRIBE 3   /* [G] — u32 máscara (1 << MLX_EV_*) dos eventos que vêm sem pedir → nada */

/* Mapa (mapa.ler). Trechos e nós são os da malha em uso (com as vias desenhadas no cenário); os ids
 * mudam quando a malha é refeita (evento MLX_EV_NETWORK). Para guardar uma referência, use edge_ref. */
#define MLX_FN_MAP_INFO 100 /* [G T] — nada → u32 nós, u32 trechos, u32 nomes, f64 lonMín, f64 latMín, f64 lonMáx,
                               f64 latMáx, texto área, u64 versão da malha */
#define MLX_FN_MAP_EDGES 101 /* [G T] — nada → lista de 20 bytes: u32 de, u32 para, u8 classe, u8 faixas,
                                u8 marcas (1 fechado, 2 desenhado, 4 alterado), u8 0, f32 limite m/s, f32 comprimento m,
                                u32 nome (0xFFFFFFFF sem nome) */
#define MLX_FN_MAP_EDGE_POINTS 102  /* [G T] — u32 trecho → lista de pontos */
#define MLX_FN_MAP_NODES 103        /* [G T] — nada → lista de pontos */
#define MLX_FN_MAP_NAMES 104        /* [G T] — nada → lista de textos */
#define MLX_FN_MAP_NEAREST_EDGE 105 /* [G T] — ponto, f32 metros no máximo → u32 trecho, f32 ao longo, f32 distância */
#define MLX_FN_MAP_NEAREST_NODE 106 /* [G T] — ponto → u32 nó */
#define MLX_FN_MAP_PLACES 107       /* [G] — nada → lista: texto nome, texto categoria, ponto, f32 viagens/dia, u8 próprio */
#define MLX_FN_MAP_ZONES 108        /* [G] — nada → lista: texto nome, f32 viagens/dia, lista de pontos */
#define MLX_FN_MAP_NEIGHBOURHOODS 109 /* [G] — nada → lista: texto nome, ponto centro, lista de u32 trechos */
#define MLX_FN_MAP_AREA 110         /* [G] — nada → lista de anéis (cada um lista de pontos) */
#define MLX_FN_MAP_EDGE_REF 111     /* [G T] — u32 trecho → u32 trecho real, f64 lon/lat início, f64 lon/lat fim */
#define MLX_FN_MAP_RESOLVE_EDGE 112 /* [G T] — a referência acima → u32 trecho na malha de agora */

/* Rotas (mapa.ler). Custo: 0 tempo esperado agora (s), 1 tempo com vias livres (s), 2 metros. */
#define MLX_FN_ROUTE 120            /* [G T] — u32 de (nó), u32 para, u8 custo → f32 custo, f32 metros, lista de u32 trechos */
#define MLX_FN_ROUTE_COSTS_FROM 121 /* [G T] — u32 de, u8 custo → lista de f32 por nó (infinito: inalcançável) */
#define MLX_FN_ROUTE_MATRIX 122     /* [G T] — u8 custo, lista de u32 nós → lista de n×n f32 (linha = origem);
                                       até 100 nós nos ganchos, 2000 nas tarefas */

/* Simulação (simulacao.ler; só ganchos) */
#define MLX_FN_SIM_INFO 130 /* nada → f64 tempo simulado s, f64 hora do dia, f64 ms por passo, i32 dia da semana,
                               u32 veículos ativos, u32 chegadas, u32 alvo, f32 km/h médios, u64 versão, u8 pausada */
#define MLX_FN_SIM_SPEED_RATIOS 131 /* nada → lista de f32 por trecho: velocidade / limite agora */
#define MLX_FN_SIM_PASSAGES 132     /* nada → lista de u32 por trecho: veículos desde o início */
#define MLX_FN_SIM_EDGE_TIMES 133   /* nada → lista de f32 por trecho: tempo de travessia medido, s */
#define MLX_FN_SIM_VEHICLES 134     /* nada → lista de 24 bytes: ponto, f32 rumo (rad), u8 tipo (0 fundo,
                                       1 frota, 2 externo), 3 × u8 0 */

/* Desenho no mapa (mapa.desenhar; só ganchos). Uma camada é a de uma contribuição "camada" do
 * manifesto, e o usuário a liga e desliga em Camadas do mapa. */
#define MLX_FN_DRAW_SET_LAYER 140 /* texto id da camada, então:
                                     linhas: lista de (u32 cor 0xAABBGGRR, f32 largura px, lista de pontos)
                                     marcadores: lista de (ponto, texto ícone, u32 cor, texto rótulo)
                                     áreas: lista de (u32 preenchimento, u32 contorno, f32 contorno px, lista de pontos)
                                     cores por trecho: lista de f32 (0 vermelho .. 1 verde; vazia: nenhuma) → nada */
#define MLX_FN_DRAW_CLEAR_LAYER 141 /* texto id → nada */

/* Interface (só ganchos). Painéis e gráficos são os do manifesto ("painel", "grafico"): o app avisa
 * quando estão na tela (MLX_EV_PANEL, MLX_EV_CHART) e a extensão manda o conteúdo (veja "Widgets"). */
#define MLX_FN_UI_PANEL 150  /* texto painel, bytes (u32 tamanho + widgets) → nada */
#define MLX_FN_UI_KPIS 151   /* lista (até 6): texto rótulo, texto valor, texto rótulo curto (celular; vazio: não
                                aparece), texto dica, u8 tom (0 normal, 1 bom, 2 alerta) → nada */
#define MLX_FN_UI_LEGEND 152 /* lista (até 12): u32 cor, u8 quadrado, texto rótulo → nada */
#define MLX_FN_UI_TOAST 153  /* u8 tipo (0 info, 1 ok, 2 aviso), texto (até 6 por minuto) → nada */
#define MLX_FN_UI_CHART 154  /* texto gráfico, bytes (widgets) → nada */
#define MLX_FN_UI_TOOL 155   /* [mapa.ferramentas] texto ferramenta, u8 ligada → nada */
#define MLX_FN_UI_FOCUS 156  /* ponto, f32 zoom (0: mantém) → nada: o mapa vai até lá */

/* Cenário (só ganchos). Ações no formato do Assistente: {"acoes": [...]} (docs/assistente.md). */
#define MLX_FN_SCENARIO_SUMMARY 160  /* [mapa.ler] nada → texto JSON {"nome", "itens": [{"id", "linha"}], "ajustes"} */
#define MLX_FN_SCENARIO_DATA_GET 161 /* nada → texto JSON da seção da extensão no cenário ("" sem) */
#define MLX_FN_SCENARIO_DATA_SET 162 /* texto JSON (até 1 MB; "" apaga) → nada */
#define MLX_FN_SCENARIO_PROPOSE 163  /* [cenario.propor] texto título, texto ações → texto relato (o usuário decide) */
#define MLX_FN_SCENARIO_CHANGE 164   /* [cenario.alterar] texto título, texto ações → u32 aplicadas, texto relato;
                                        tudo ou nada: com uma ação ambígua ou não encontrada, nada muda */
#define MLX_FN_SCENARIO_UNDO 165     /* [cenario.alterar] nada → texto relato (desfaz a última mudança dela) */

/* Veículos gerenciados (veiculos.controlar; só ganchos; até 500): estacionados num nó até partirem;
 * no fim da rota estacionam de novo (evento MLX_EV_VEHICLE_ARRIVED). Somem quando a malha é refeita. */
#define MLX_FN_VEHICLE_CREATE 170 /* u32 nó, f32 fator de velocidade, u8 tipo (0 moto, 1 carro, 2 van, 3 caminhão),
                                     u8 cor → u32 veículo */
#define MLX_FN_VEHICLE_DEPART 171 /* u32 veículo, lista de u32 trechos encadeados → nada */
#define MLX_FN_VEHICLE_REMOVE 172 /* u32 veículo → nada */
#define MLX_FN_VEHICLE_STATES 173 /* nada → lista de 20 bytes: u32 veículo, u8 estado (0 surgindo, 1 estacionado,
                                     2 rodando, 3 sem espaço), u8 oculto, 2 × u8 0, u32 trecho, f32 posição m, f32 m/s */
#define MLX_FN_VEHICLE_HIDE 174   /* u32 veículo, u8 oculto → nada */

/* Tarefas em segundo plano (só ganchos; até 2 ao mesmo tempo) */
#define MLX_FN_TASK_START 180  /* i32 tipo (>= 0), f64 tempo limite s, bytes entrada → u32 tarefa */
#define MLX_FN_TASK_POLL 181   /* u32 tarefa → u8 estado (0 rodando, 1 pronta, 2 falhou), f64 progresso, i32 código de
                                  mlx_task, texto erro, bytes resultado (uma vez: depois de pronta, a tarefa sai) */
#define MLX_FN_TASK_CANCEL 182 /* u32 tarefa → nada */

/* Ensaios (simulacao.ensaiar; só ganchos): a simulação copiada como está e com a mudança, rodadas
 * em segundo plano pelo mesmo tempo, comparadas. */
#define MLX_FN_EXPERIMENT_START 190  /* f64 minutos simulados (1..120), texto ações → u32 ensaio */
#define MLX_FN_EXPERIMENT_POLL 191   /* u32 ensaio → u8 estado (0 rodando, 1 pronto, 2 falhou), f64 progresso, texto erro,
                                        lista: texto rótulo, texto unidade, f64 como está, f64 com a mudança,
                                        i8 melhor (-1 menor, 1 maior, 0 nenhum) */
#define MLX_FN_EXPERIMENT_CANCEL 192 /* u32 ensaio → nada */

/* Resolvedores VRP (manifesto: "resolvedor"). Declarado em mlx_init, ele aparece em Algoritmos e
 * na Bancada; cada execução chama mlx_task(MLX_TASK_SOLVE_VRP, ...) numa instância própria. */
#define MLX_FN_SOLVER_DECLARE 200 /* [G] texto id, u32 recursos (MLX_VRP_*), lista (até 32) de parâmetros: texto chave,
                                     texto rótulo, texto dica, f32 padrão, f32 mín, f32 máx, f32 passo, u8 casas, u8 avançado */
#define MLX_FN_SOLVER_OFFER 201   /* [R] rotas: lista de (lista de u32 locais 1..m-1) → f64 custo (do app), u8 viável,
                                     u8 melhorou */
#define MLX_FN_SOLVER_PARAMS 202  /* [R] nada → u64 semente, f64 tempo limite s, f64 decorrido s, lista: texto chave, f32.
                                     Chaves do app na lista: "_copy" e "_copies" (esta cópia, de quantas: com
                                     MLX_VRP_PARALLEL), "_sliced" (1: o app pede fatias, ver MLX_TASK_SOLVE_STEP) */
#define MLX_FN_SOLVER_PROGRESS 203 /* [R] u64 iterações desde o último aviso → nada (a Bancada mostra iterações/s) */

#define MLX_VRP_CAPACITY 1
#define MLX_VRP_TIME_WINDOWS 2
#define MLX_VRP_PICKUP_DELIVERY 4
#define MLX_VRP_DYNAMIC 8
#define MLX_VRP_HETEROGENEOUS 16
#define MLX_VRP_PARALLEL 0x40000000 /* em solver.declare: o app roda várias cópias em paralelo (uma busca cada,
                                       sementes diferentes) e fica com a melhor */

/* mlx_task: tipos que o próprio app usa. */
#define MLX_TASK_SOLVE_STEP (-2) /* resolvedor em fatias ("_sliced" = 1 nos parâmetros): MLX_TASK_SOLVE_VRP só
     prepara e devolve 1; cada MLX_TASK_SOLVE_STEP trabalha uns <tamanho> ms e devolve 1 (há mais) ou 0 (acabou).
     Assim as threads voltam para a simulação entre as fatias. */
#define MLX_TASK_SOLVE_VRP (-1) /* texto id do resolvedor, então a instância:
     u32 n (pedidos 1..n; 0 é o depósito), u32 m (locais), i32 veículos, i32 capacidade, f32 valor do tempo R$/h,
     u8 marcas (1 rotas abertas, 2 pago por entrega), m × i32 demanda,
     lista de m×m f32 tempos s (linha = origem),
     lista de f32 abertura, lista de f32 fechamento, lista de f32 serviço (m ou 0 itens: sem janelas),
     lista de i32 par (coleta e entrega; m ou 0), lista de i32 veículo fixo (m ou 0),
     lista de (u32 local de partida, f64 hora de partida s, i32 carga a bordo) (veículos já na rua),
     lista de tipos: i32 capacidade, f32 fator de velocidade, f64 fixo R$, f64 R$/km, f64 R$/h, f64 R$/entrega,
     f64 R$/km com carga; lista de u8 tipo por veículo, lista de m×m f32 metros (ou 0), lista de m×m f32 pedágios;
     então (desde a 0.14, quem não lê para aqui) u8 2, por tipo: f32 fator do pedágio, u8 camada; lista de camadas:
     lista de m×m f32 tempos, f32 parte dirigida, f32 metros, f32 pedágios, u8 bloqueado; e o plano de partida:
     u8 0, ou 1 e as rotas (lista de listas de u32 locais) */

/* ---- Eventos (mlx_event) --------------------------------------------------------------------- */

#define MLX_EV_FRAME 1          /* f64 segundos desde o anterior (inscrito) */
#define MLX_EV_PANEL 2          /* texto painel: está na tela agora; mande o conteúdo (MLX_FN_UI_PANEL) */
#define MLX_EV_UI 3             /* texto painel, então u32 quantidade e por ação: u32 widget, u8 tipo (1 clique,
                                   2 chave, 3 número, 4 escolha, 5 texto), f64 valor, texto */
#define MLX_EV_TOOL 4           /* texto ferramenta, u8 ação (MLX_TOOL_*), ponto: responda 1 = tomado */
#define MLX_EV_TAP 5            /* ponto de um toque que nenhuma ferramenta tomou: responda 1 = usado */
#define MLX_EV_TASK_DONE 6      /* u32 tarefa */
#define MLX_EV_VEHICLE_ARRIVED 7 /* u32 quantidade, então os veículos que estacionaram (inscrito) */
#define MLX_EV_NETWORK 8        /* u64 versão: a malha foi refeita (ids de trechos podem ter mudado) */
#define MLX_EV_SCENARIO 9       /* o cenário mudou */
#define MLX_EV_EXPERIMENT_DONE 10 /* u32 ensaio */
#define MLX_EV_CHART 11         /* texto gráfico: está na tela agora; mande o conteúdo (MLX_FN_UI_CHART) */

#define MLX_TOOL_TAP 0
#define MLX_TOOL_GRAB 1 /* um arraste vai começar aqui: responda 1 para ele ser da ferramenta */
#define MLX_TOOL_DRAG_BEGIN 2
#define MLX_TOOL_DRAG_MOVE 3
#define MLX_TOOL_DRAG_END 4
#define MLX_TOOL_DRAG_CANCEL 5
#define MLX_TOOL_OFF 6

/* ---- Widgets (MLX_FN_UI_PANEL, MLX_FN_UI_CHART) -------------------------------------------------
 * "MLXU", u16 versão 1, então itens: u8 tipo e os campos. Ids de widget (u32) são da extensão e
 * voltam nos eventos MLX_EV_UI. Seções abrem e fecham (MLX_UI_END_SECTION). */
#define MLX_UI_SECTION 1     /* u32 id, texto título, texto resumo, u8 aberta */
#define MLX_UI_END_SECTION 2
#define MLX_UI_TEXT 3        /* u8 estilo (0 normal, 1 apagado, 2 título, 3 bom, 4 alerta, 5 ruim), texto */
#define MLX_UI_BUTTON 4      /* u32 id, texto, u8 principal, u8 desativado */
#define MLX_UI_TOGGLE 5      /* u32 id, texto, u8 valor, texto dica */
#define MLX_UI_NUMBER 6      /* u32 id, texto, f64 valor, f64 mín, f64 máx, f64 passo, u8 casas, texto dica */
#define MLX_UI_CHOICE 7      /* u32 id, lista de textos, u32 escolhido (botões lado a lado) */
#define MLX_UI_SELECT 8      /* u32 id, texto, lista de textos, u32 escolhido */
#define MLX_UI_INPUT 9       /* u32 id, texto, texto valor, u32 bytes máx */
#define MLX_UI_KPI 10        /* texto rótulo, texto valor, u8 tom (0 normal, 1 bom, 2 alerta) */
#define MLX_UI_TABLE 11      /* u32 colunas, u32 linhas, colunas textos cabeçalho, linhas × colunas textos */
#define MLX_UI_LINE_CHART 12 /* u32 id, texto título, u32 séries, cada uma: texto nome, u32 cor, u32 n, n f32 x, n f32 y */
#define MLX_UI_BARS 13       /* u32 id, texto título, u32 n, n textos, n f32 */
#define MLX_UI_PROGRESS 14   /* f32 0..1, texto */
#define MLX_UI_SEPARATOR 15
#define MLX_UI_SAME_LINE 16

/* ---- Montar e ler blocos ---------------------------------------------------------------------- */

/* Sem depender da biblioteca C (memcpy e memset viram memory.copy/fill com bulk-memory). */
static inline size_t mlx_strlen(const char* s) {
    size_t n = 0;
    while (s[n]) ++n;
    return n;
}
/* Um texto lido de um bloco (sem zero final) é igual a `b`? */
static inline int mlx_str_is(const char* a, uint32_t length, const char* b) {
    uint32_t i = 0;
    for (; i < length; ++i)
        if (!b[i] || a[i] != b[i]) return 0;
    return b[i] == 0;
}

typedef struct mlx_writer {
    uint8_t* data;
    size_t size, capacity;
    int failed; /* 1 quando não coube */
} mlx_writer;

static inline void mlx_w_init(mlx_writer* w, void* buffer, size_t capacity) {
    w->data = (uint8_t*)buffer;
    w->size = 0;
    w->capacity = capacity;
    w->failed = 0;
}
static inline void mlx_w_raw(mlx_writer* w, const void* p, size_t n) {
    if (w->failed || n > w->capacity - w->size) {
        w->failed = 1;
        return;
    }
    __builtin_memcpy(w->data + w->size, p, n);
    w->size += n;
}
static inline void mlx_w_u8(mlx_writer* w, uint8_t v) { mlx_w_raw(w, &v, 1); }
static inline void mlx_w_u32(mlx_writer* w, uint32_t v) { mlx_w_raw(w, &v, 4); }
static inline void mlx_w_i32(mlx_writer* w, int32_t v) { mlx_w_raw(w, &v, 4); }
static inline void mlx_w_u64(mlx_writer* w, uint64_t v) { mlx_w_raw(w, &v, 8); }
static inline void mlx_w_f32(mlx_writer* w, float v) { mlx_w_raw(w, &v, 4); }
static inline void mlx_w_f64(mlx_writer* w, double v) { mlx_w_raw(w, &v, 8); }
static inline void mlx_w_strn(mlx_writer* w, const char* s, size_t n) {
    mlx_w_u32(w, (uint32_t)n);
    mlx_w_raw(w, s, n);
}
static inline void mlx_w_str(mlx_writer* w, const char* s) { mlx_w_strn(w, s, mlx_strlen(s)); }
static inline void mlx_w_point(mlx_writer* w, double lon, double lat) {
    mlx_w_f64(w, lon);
    mlx_w_f64(w, lat);
}

typedef struct mlx_reader {
    const uint8_t* data;
    size_t size, at;
    int failed; /* 1 quando o bloco acabou antes */
} mlx_reader;

static inline void mlx_r_init(mlx_reader* r, const void* data, size_t size) {
    r->data = (const uint8_t*)data;
    r->size = size;
    r->at = 0;
    r->failed = 0;
}
static inline int mlx_r_raw(mlx_reader* r, void* out, size_t n) {
    if (r->failed || n > r->size - r->at) {
        r->failed = 1;
        __builtin_memset(out, 0, n);
        return 0;
    }
    __builtin_memcpy(out, r->data + r->at, n);
    r->at += n;
    return 1;
}
static inline uint8_t mlx_r_u8(mlx_reader* r) {
    uint8_t v;
    mlx_r_raw(r, &v, 1);
    return v;
}
static inline uint32_t mlx_r_u32(mlx_reader* r) {
    uint32_t v;
    mlx_r_raw(r, &v, 4);
    return v;
}
static inline int32_t mlx_r_i32(mlx_reader* r) {
    int32_t v;
    mlx_r_raw(r, &v, 4);
    return v;
}
static inline uint64_t mlx_r_u64(mlx_reader* r) {
    uint64_t v;
    mlx_r_raw(r, &v, 8);
    return v;
}
static inline float mlx_r_f32(mlx_reader* r) {
    float v;
    mlx_r_raw(r, &v, 4);
    return v;
}
static inline double mlx_r_f64(mlx_reader* r) {
    double v;
    mlx_r_raw(r, &v, 8);
    return v;
}
/* Um texto: aponta para dentro do bloco (sem o zero final); *length recebe o tamanho. */
static inline const char* mlx_r_str(mlx_reader* r, uint32_t* length) {
    const uint32_t n = mlx_r_u32(r);
    if (r->failed || n > r->size - r->at) {
        r->failed = 1;
        *length = 0;
        return "";
    }
    const char* s = (const char*)(r->data + r->at);
    r->at += n;
    *length = n;
    return s;
}
/* Pula `n` bytes (ou um item de tamanho conhecido). */
static inline const void* mlx_r_skip(mlx_reader* r, size_t n) {
    if (r->failed || n > r->size - r->at) {
        r->failed = 1;
        return 0;
    }
    const void* p = r->data + r->at;
    r->at += n;
    return p;
}

/* mobilog_call com a resposta num buffer da extensão: o tamanho da resposta, ou um erro. */
static inline int32_t mlx_call(int32_t fn, const mlx_writer* request, void* answer, int32_t capacity) {
    return mobilog_call(fn, request ? request->data : 0, request ? (int32_t)request->size : 0, answer, capacity);
}

static inline void mlx_log(int32_t level, const char* text) { mobilog_log(level, text, (int32_t)mlx_strlen(text)); }

/* ---- Painéis -------------------------------------------------------------------------------------
 * mlx_ui_begin num mlx_writer, os widgets, e mlx_ui_send para mandar (MLX_FN_UI_PANEL ou
 * MLX_FN_UI_CHART). */
static inline void mlx_ui_begin(mlx_writer* w) {
    mlx_w_raw(w, "MLXU", 4);
    mlx_w_u8(w, 1);
    mlx_w_u8(w, 0);
}
static inline void mlx_ui_section(mlx_writer* w, uint32_t id, const char* title, const char* summary, int open) {
    mlx_w_u8(w, MLX_UI_SECTION);
    mlx_w_u32(w, id);
    mlx_w_str(w, title);
    mlx_w_str(w, summary ? summary : "");
    mlx_w_u8(w, open ? 1 : 0);
}
static inline void mlx_ui_end_section(mlx_writer* w) { mlx_w_u8(w, MLX_UI_END_SECTION); }
static inline void mlx_ui_text(mlx_writer* w, int style, const char* text) {
    mlx_w_u8(w, MLX_UI_TEXT);
    mlx_w_u8(w, (uint8_t)style);
    mlx_w_str(w, text);
}
static inline void mlx_ui_button(mlx_writer* w, uint32_t id, const char* label, int primary, int disabled) {
    mlx_w_u8(w, MLX_UI_BUTTON);
    mlx_w_u32(w, id);
    mlx_w_str(w, label);
    mlx_w_u8(w, primary ? 1 : 0);
    mlx_w_u8(w, disabled ? 1 : 0);
}
static inline void mlx_ui_toggle(mlx_writer* w, uint32_t id, const char* label, int value, const char* tip) {
    mlx_w_u8(w, MLX_UI_TOGGLE);
    mlx_w_u32(w, id);
    mlx_w_str(w, label);
    mlx_w_u8(w, value ? 1 : 0);
    mlx_w_str(w, tip ? tip : "");
}
static inline void mlx_ui_number(mlx_writer* w, uint32_t id, const char* label, double value, double min, double max, double step,
                                 int decimals, const char* tip) {
    mlx_w_u8(w, MLX_UI_NUMBER);
    mlx_w_u32(w, id);
    mlx_w_str(w, label);
    mlx_w_f64(w, value);
    mlx_w_f64(w, min);
    mlx_w_f64(w, max);
    mlx_w_f64(w, step);
    mlx_w_u8(w, (uint8_t)decimals);
    mlx_w_str(w, tip ? tip : "");
}
static inline void mlx_ui_choice(mlx_writer* w, uint32_t id, const char* const* options, uint32_t count, uint32_t selected) {
    mlx_w_u8(w, MLX_UI_CHOICE);
    mlx_w_u32(w, id);
    mlx_w_u32(w, count);
    for (uint32_t i = 0; i < count; ++i) mlx_w_str(w, options[i]);
    mlx_w_u32(w, selected);
}
static inline void mlx_ui_kpi(mlx_writer* w, const char* label, const char* value, int tone) {
    mlx_w_u8(w, MLX_UI_KPI);
    mlx_w_str(w, label);
    mlx_w_str(w, value);
    mlx_w_u8(w, (uint8_t)tone);
}
/* Uma tabela: `cells` tem linhas × colunas textos, linha a linha. */
static inline void mlx_ui_table(mlx_writer* w, const char* const* headers, uint32_t columns, const char* const* cells, uint32_t rows) {
    mlx_w_u8(w, MLX_UI_TABLE);
    mlx_w_u32(w, columns);
    mlx_w_u32(w, rows);
    for (uint32_t i = 0; i < columns; ++i) mlx_w_str(w, headers[i]);
    for (uint32_t i = 0; i < rows * columns; ++i) mlx_w_str(w, cells[i]);
}
/* Um gráfico de linha com uma série (cor 0: a do tema). */
static inline void mlx_ui_line_chart(mlx_writer* w, uint32_t id, const char* title, const char* series, uint32_t color, const float* xs,
                                     const float* ys, uint32_t n) {
    mlx_w_u8(w, MLX_UI_LINE_CHART);
    mlx_w_u32(w, id);
    mlx_w_str(w, title);
    mlx_w_u32(w, 1);
    mlx_w_str(w, series);
    mlx_w_u32(w, color);
    mlx_w_u32(w, n);
    mlx_w_raw(w, xs, (size_t)n * 4);
    mlx_w_raw(w, ys, (size_t)n * 4);
}
static inline void mlx_ui_bars(mlx_writer* w, uint32_t id, const char* title, const char* const* labels, const float* values, uint32_t n) {
    mlx_w_u8(w, MLX_UI_BARS);
    mlx_w_u32(w, id);
    mlx_w_str(w, title);
    mlx_w_u32(w, n);
    for (uint32_t i = 0; i < n; ++i) mlx_w_str(w, labels[i]);
    mlx_w_raw(w, values, (size_t)n * 4);
}
static inline void mlx_ui_progress(mlx_writer* w, float fraction, const char* text) {
    mlx_w_u8(w, MLX_UI_PROGRESS);
    mlx_w_f32(w, fraction);
    mlx_w_str(w, text);
}
static inline void mlx_ui_separator(mlx_writer* w) { mlx_w_u8(w, MLX_UI_SEPARATOR); }
static inline void mlx_ui_same_line(mlx_writer* w) { mlx_w_u8(w, MLX_UI_SAME_LINE); }
/* Manda o painel (fn = MLX_FN_UI_PANEL) ou gráfico (MLX_FN_UI_CHART) `id`, montado em `ui`; `scratch`
 * guarda o pedido (precisa de ui->size + 80 bytes). */
static inline int32_t mlx_ui_send(int32_t fn, const char* id, const mlx_writer* ui, void* scratch, size_t scratchCap) {
    mlx_writer r;
    mlx_w_init(&r, scratch, scratchCap);
    mlx_w_str(&r, id);
    mlx_w_u32(&r, (uint32_t)ui->size);
    mlx_w_raw(&r, ui->data, ui->size);
    if (r.failed || ui->failed) return MLX_ERR_LIMIT;
    return mobilog_call(fn, r.data, (int32_t)r.size, 0, 0);
}

/* Números em texto sem a biblioteca C: inteiro, ou com `decimals` casas (mlx_fmt_num: vírgula
 * decimal, para mostrar; mlx_fmt_json: ponto). */
static inline size_t mlx_fmt_int(char* out, int64_t v) {
    char tmp[24];
    size_t n = 0, k = 0;
    const int neg = v < 0;
    uint64_t u = neg ? (uint64_t)(-v) : (uint64_t)v;
    do {
        tmp[n++] = (char)('0' + u % 10);
        u /= 10;
    } while (u);
    if (neg) out[k++] = '-';
    while (n) out[k++] = tmp[--n];
    out[k] = 0;
    return k;
}
static inline size_t mlx_fmt_num_sep(char* out, double v, int decimals, char point) {
    double scale = 1;
    for (int i = 0; i < decimals; ++i) scale *= 10;
    const int neg = v < 0;
    if (neg) v = -v;
    const int64_t whole = (int64_t)(v * scale + 0.5);
    size_t k = 0;
    if (neg && whole) out[k++] = '-';
    k += mlx_fmt_int(out + k, whole / (int64_t)scale);
    if (decimals > 0) {
        out[k++] = point;
        int64_t frac = whole % (int64_t)scale;
        for (int i = decimals - 1; i >= 0; --i) {
            int64_t d = frac;
            for (int j = 0; j < i; ++j) d /= 10;
            out[k++] = (char)('0' + d % 10);
        }
    }
    out[k] = 0;
    return k;
}
static inline size_t mlx_fmt_num(char* out, double v, int decimals) { return mlx_fmt_num_sep(out, v, decimals, ','); }
/* Com ponto decimal (JSON). */
static inline size_t mlx_fmt_json(char* out, double v, int decimals) { return mlx_fmt_num_sep(out, v, decimals, '.'); }
/* Junta textos em `out` (capacidade `cap`); a lista termina em 0. */
static inline const char* mlx_cat(char* out, size_t cap, const char* first, ...) __attribute__((sentinel));
#include <stdarg.h>
static inline const char* mlx_cat(char* out, size_t cap, const char* first, ...) {
    size_t k = 0;
    va_list ap;
    va_start(ap, first);
    for (const char* s = first; s; s = va_arg(ap, const char*))
        while (*s && k + 1 < cap) out[k++] = *s++;
    va_end(ap);
    out[k] = 0;
    return out;
}

/* ---- Alocador simples (para quem compila sem a biblioteca C) ------------------------------------
 * #define MLX_DEFINE_ALLOCATOR antes de incluir, num arquivo só: define mlx_alloc/mlx_free e
 * mlx_malloc/mlx_mfree (lista de blocos livres; a memória cresce com memory.grow). */
#ifdef MLX_DEFINE_ALLOCATOR
typedef struct mlx_block_ {
    uint32_t size; /* bytes úteis */
    struct mlx_block_* next;
} mlx_block_;
static mlx_block_* mlx_free_list_ = 0;
static uint8_t* mlx_heap_top_ = 0;
static uint8_t* mlx_heap_end_ = 0;
extern uint8_t __heap_base;

void* mlx_malloc(size_t size) {
    size = (size + 15u) & ~(size_t)15u;
    mlx_block_** link = &mlx_free_list_;
    for (mlx_block_* b = mlx_free_list_; b; link = &b->next, b = b->next)
        if (b->size >= size) {
            *link = b->next;
            return (uint8_t*)b + 16;
        }
    if (!mlx_heap_top_) mlx_heap_top_ = mlx_heap_end_ = (uint8_t*)(((uintptr_t)&__heap_base + 15u) & ~(uintptr_t)15u);
    if (mlx_heap_end_ < (uint8_t*)(uintptr_t)(__builtin_wasm_memory_size(0) * 65536u))
        mlx_heap_end_ = (uint8_t*)(uintptr_t)(__builtin_wasm_memory_size(0) * 65536u);
    while ((size_t)(mlx_heap_end_ - mlx_heap_top_) < size + 16) {
        const size_t pages = (size + 16 + 65535u) / 65536u;
        if (__builtin_wasm_memory_grow(0, pages) == (size_t)-1) return 0;
        mlx_heap_end_ += pages * 65536u;
    }
    mlx_block_* b = (mlx_block_*)mlx_heap_top_;
    b->size = (uint32_t)size;
    mlx_heap_top_ += size + 16;
    return (uint8_t*)b + 16;
}
void mlx_mfree(void* p) {
    if (!p) return;
    mlx_block_* b = (mlx_block_*)((uint8_t*)p - 16);
    b->next = mlx_free_list_;
    mlx_free_list_ = b;
}
MLX_EXPORT("mlx_alloc") void* mlx_alloc(int32_t size) { return mlx_malloc(size > 0 ? (size_t)size : 1); }
MLX_EXPORT("mlx_free") void mlx_free(void* p) { mlx_mfree(p); }

/* mobilog_call com a resposta num buffer que cresce quando ela não cabe: *answer aponta para ela
 * (vale até a próxima chamada desta função). Devolve o tamanho, ou um erro. */
static __attribute__((unused)) uint8_t* mlx_big_ = 0;
static __attribute__((unused)) size_t mlx_big_cap_ = 0;
static inline int32_t mlx_call_big(int32_t fn, const mlx_writer* request, const uint8_t** answer) {
    if (!mlx_big_) {
        mlx_big_cap_ = 1u << 16;
        mlx_big_ = (uint8_t*)mlx_malloc(mlx_big_cap_);
    }
    int32_t n = mlx_call(fn, request, mlx_big_, (int32_t)mlx_big_cap_);
    if (n > (int32_t)mlx_big_cap_) {
        mlx_mfree(mlx_big_);
        mlx_big_cap_ = (size_t)n;
        mlx_big_ = (uint8_t*)mlx_malloc(mlx_big_cap_);
        if (!mlx_big_) return MLX_ERR_LIMIT;
        n = mobilog_take(mlx_big_, (int32_t)mlx_big_cap_);
    }
    *answer = mlx_big_;
    return n;
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* MOBILOG_EXT_H */
