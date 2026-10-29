/* Busca local 2-opt / Or-opt — um resolvedor de roteirização (VRP) como extensão.
 *
 * Aparece em Extensões → Rotas e entregas → Algoritmos e na Bancada, ao lado dos resolvedores do
 * app. Cada execução: um plano inicial pelo vizinho mais próximo, depois busca local (2-opt dentro
 * de cada rota; mover um cliente para outra posição ou outra rota — Or-opt) e, enquanto houver
 * tempo, perturbações aleatórias seguidas de nova busca (busca local iterada). Cada melhoria vai ao
 * app (solver.offer), que a avalia com as próprias contas.
 *
 * Trata capacidade e frota mista (cada rota é de um veículo, com a sua capacidade e os seus custos).
 */
#define MLX_DEFINE_ALLOCATOR
#include "mobilog_ext.h"

static uint8_t answer_[256];

MLX_EXPORT("mlx_api_version") int32_t api_version(void) { return MLX_API_VERSION; }

MLX_EXPORT("mlx_init") int32_t init(void) {
    static uint8_t buf[1024];
    mlx_writer w;
    mlx_w_init(&w, buf, sizeof buf);
    mlx_w_str(&w, "busca-local");
    mlx_w_u32(&w, MLX_VRP_CAPACITY | MLX_VRP_HETEROGENEOUS);
    mlx_w_u32(&w, 2);
    mlx_w_str(&w, "perturbacao");
    mlx_w_str(&w, "Clientes movidos por perturbação");
    mlx_w_str(&w, "Quantos clientes mudam de lugar ao acaso quando a busca para de melhorar. Mais: explora mais longe.");
    mlx_w_f32(&w, 4);
    mlx_w_f32(&w, 1);
    mlx_w_f32(&w, 30);
    mlx_w_f32(&w, 1);
    mlx_w_u8(&w, 0);
    mlx_w_u8(&w, 0);
    mlx_w_str(&w, "pioraAceita");
    mlx_w_str(&w, "Piora aceita (%)");
    mlx_w_str(&w, "Uma perturbação que deixa o plano até este tanto pior que o melhor continua dela (0: volta sempre ao melhor).");
    mlx_w_f32(&w, 1);
    mlx_w_f32(&w, 0);
    mlx_w_f32(&w, 10);
    mlx_w_f32(&w, 0.5f);
    mlx_w_u8(&w, 1);
    mlx_w_u8(&w, 1);
    return mlx_call(MLX_FN_SOLVER_DECLARE, &w, answer_, sizeof answer_) < 0 ? 1 : 0;
}

/* ---- A instância --------------------------------------------------------------------------- */

typedef struct {
    uint32_t n, m;
    int32_t vehicles;
    const int32_t* demand;
    const float *time, *dist;
    int32_t* cap;          /* por veículo */
    double *fixed, *perKm, *perHour;
    float* speed;
    int mixed;
} Inst;

/* Lê uma lista de f32 (u32 quantidade + valores): os valores, ou 0 quando vazia. */
static const float* floats(mlx_reader* r, uint32_t expected) {
    const uint32_t n = mlx_r_u32(r);
    if (n == 0) return 0;
    if (n != expected) r->failed = 1;
    return (const float*)mlx_r_skip(r, (size_t)n * 4);
}

static int readInstance(mlx_reader* r, Inst* in) {
    uint32_t len;
    mlx_r_str(r, &len); /* o id do resolvedor (só há um) */
    in->n = mlx_r_u32(r);
    in->m = mlx_r_u32(r);
    in->vehicles = mlx_r_i32(r);
    const int32_t capacity = mlx_r_i32(r);
    mlx_r_f32(r);
    mlx_r_u8(r);
    const uint32_t m = in->m, mm = m * m;
    in->demand = (const int32_t*)mlx_r_skip(r, (size_t)m * 4);
    in->time = floats(r, mm);
    if (floats(r, m) || floats(r, m) || floats(r, m)) return 0; /* janelas: não tratadas aqui */
    if (mlx_r_u32(r) || mlx_r_u32(r) || mlx_r_u32(r)) return 0; /* pares, fixos, veículos na rua */
    const uint32_t types = mlx_r_u32(r);
    int32_t tcap[64];
    double tfixed[64], tkm[64], thour[64];
    float tspeed[64];
    for (uint32_t t = 0; t < types && t < 64; ++t) {
        tcap[t] = mlx_r_i32(r);
        tspeed[t] = mlx_r_f32(r);
        tfixed[t] = mlx_r_f64(r);
        tkm[t] = mlx_r_f64(r);
        thour[t] = mlx_r_f64(r);
        mlx_r_f64(r);
        mlx_r_f64(r);
    }
    const uint32_t typeOf = mlx_r_u32(r);
    const uint8_t* kinds = (const uint8_t*)mlx_r_skip(r, typeOf);
    in->dist = floats(r, mm);
    if (r->failed || !in->time || in->vehicles < 1 || types > 64) return 0;
    const uint32_t k = (uint32_t)in->vehicles;
    in->cap = (int32_t*)mlx_malloc(k * 4);
    in->speed = (float*)mlx_malloc(k * 4);
    in->fixed = (double*)mlx_malloc(k * 8);
    in->perKm = (double*)mlx_malloc(k * 8);
    in->perHour = (double*)mlx_malloc(k * 8);
    in->mixed = types > 0;
    for (uint32_t v = 0; v < k; ++v) {
        const uint32_t t = in->mixed && v < typeOf ? kinds[v] : 0;
        in->cap[v] = in->mixed ? tcap[t] : capacity;
        in->speed[v] = in->mixed && tspeed[t] > 0 ? tspeed[t] : 1.0f;
        in->fixed[v] = in->mixed ? tfixed[t] : 0;
        in->perKm[v] = in->mixed ? tkm[t] : 0;
        in->perHour[v] = in->mixed ? thour[t] : 0;
    }
    return 1;
}

/* ---- Planos: uma rota por veículo ------------------------------------------------------------ */

typedef struct {
    uint32_t** route; /* clientes de cada veículo */
    uint32_t* len;
    double* cost;     /* de cada rota */
    double total;
} Plan;

static double routeCost(const Inst* in, uint32_t v, const uint32_t* r, uint32_t len) {
    if (!len) return 0;
    double t = 0, d = 0;
    uint32_t at = 0;
    for (uint32_t i = 0; i <= len; ++i) {
        const uint32_t to = i < len ? r[i] : 0;
        t += in->time[at * in->m + to];
        if (in->dist) d += in->dist[at * in->m + to];
        at = to;
    }
    if (!in->mixed) return t;
    t /= in->speed[v]; /* aproximação: o app recalcula com as contas exatas */
    return in->fixed[v] + in->perKm[v] * d / 1000.0 + in->perHour[v] * t / 3600.0;
}

static int32_t routeLoad(const Inst* in, const uint32_t* r, uint32_t len) {
    int32_t q = 0;
    for (uint32_t i = 0; i < len; ++i) q += in->demand[r[i]];
    return q;
}

static void planNew(const Inst* in, Plan* p) {
    const uint32_t k = (uint32_t)in->vehicles;
    p->route = (uint32_t**)mlx_malloc(k * sizeof(uint32_t*));
    p->len = (uint32_t*)mlx_malloc(k * 4);
    p->cost = (double*)mlx_malloc(k * 8);
    for (uint32_t v = 0; v < k; ++v) {
        p->route[v] = (uint32_t*)mlx_malloc((in->n + 1) * 4);
        p->len[v] = 0;
        p->cost[v] = 0;
    }
    p->total = 0;
}

static void planCopy(const Inst* in, Plan* to, const Plan* from) {
    for (uint32_t v = 0; v < (uint32_t)in->vehicles; ++v) {
        to->len[v] = from->len[v];
        to->cost[v] = from->cost[v];
        __builtin_memcpy(to->route[v], from->route[v], from->len[v] * 4);
    }
    to->total = from->total;
}

static void planCost(const Inst* in, Plan* p) {
    p->total = 0;
    for (uint32_t v = 0; v < (uint32_t)in->vehicles; ++v) p->total += p->cost[v] = routeCost(in, v, p->route[v], p->len[v]);
}

/* Vizinho mais próximo, veículo a veículo (os de maior capacidade primeiro numa frota mista). */
static int initial(const Inst* in, Plan* p) {
    uint8_t* done = (uint8_t*)mlx_malloc(in->m);
    __builtin_memset(done, 0, in->m);
    uint32_t left = in->n;
    for (uint32_t step = 0; step < (uint32_t)in->vehicles && left; ++step) {
        uint32_t v = step;
        if (in->mixed) { /* o maior veículo ainda vazio */
            int32_t best = -1;
            for (uint32_t u = 0; u < (uint32_t)in->vehicles; ++u)
                if (!p->len[u] && in->cap[u] > best) best = in->cap[u], v = u;
        }
        int32_t load = 0;
        uint32_t at = 0;
        for (;;) {
            uint32_t pick = 0;
            float bestT = 3.0e38f;
            for (uint32_t c = 1; c <= in->n; ++c)
                if (!done[c] && load + in->demand[c] <= in->cap[v] && in->time[at * in->m + c] < bestT) pick = c, bestT = in->time[at * in->m + c];
            if (!pick) break;
            done[pick] = 1;
            load += in->demand[pick];
            p->route[v][p->len[v]++] = pick;
            at = pick;
            --left;
        }
        if (!p->len[v]) break;
    }
    mlx_mfree(done);
    planCost(in, p);
    return left == 0;
}

/* 2-opt dentro de uma rota: inverte trechos enquanto melhora. */
static int twoOpt(const Inst* in, Plan* p, uint32_t v) {
    uint32_t* r = p->route[v];
    const uint32_t len = p->len[v];
    int improved = 0;
    for (int again = 1; again;) {
        again = 0;
        for (uint32_t i = 0; i + 1 < len; ++i)
            for (uint32_t j = i + 1; j < len; ++j) {
                for (uint32_t a = i, b = j; a < b; ++a, --b) {
                    const uint32_t t = r[a];
                    r[a] = r[b];
                    r[b] = t;
                }
                const double c = routeCost(in, v, r, len);
                if (c + 1e-9 < p->cost[v]) {
                    p->total += c - p->cost[v];
                    p->cost[v] = c;
                    again = improved = 1;
                } else {
                    for (uint32_t a = i, b = j; a < b; ++a, --b) {
                        const uint32_t t = r[a];
                        r[a] = r[b];
                        r[b] = t;
                    }
                }
            }
    }
    return improved;
}

static void removeAt(uint32_t* r, uint32_t* len, uint32_t i) {
    for (uint32_t k = i; k + 1 < *len; ++k) r[k] = r[k + 1];
    --*len;
}
static void insertAt(uint32_t* r, uint32_t* len, uint32_t i, uint32_t c) {
    for (uint32_t k = *len; k > i; --k) r[k] = r[k - 1];
    r[i] = c;
    ++*len;
}

/* Or-opt: cada cliente para a melhor posição, nesta ou noutra rota (respeitando a capacidade). */
static int relocate(const Inst* in, Plan* p) {
    int improved = 0;
    const uint32_t k = (uint32_t)in->vehicles;
    for (uint32_t v = 0; v < k; ++v)
        for (uint32_t i = 0; i < p->len[v]; ++i) {
            const uint32_t c = p->route[v][i];
            removeAt(p->route[v], &p->len[v], i);
            const double without = routeCost(in, v, p->route[v], p->len[v]);
            double bestGain = 1e-9;
            uint32_t bestU = v, bestJ = i;
            for (uint32_t u = 0; u < k; ++u) {
                if (u != v && routeLoad(in, p->route[u], p->len[u]) + in->demand[c] > in->cap[u]) continue;
                const double base = u == v ? without : p->cost[u];
                for (uint32_t j = 0; j <= p->len[u]; ++j) {
                    if (u == v && j == i) continue;
                    insertAt(p->route[u], &p->len[u], j, c);
                    const double with = routeCost(in, u, p->route[u], p->len[u]);
                    removeAt(p->route[u], &p->len[u], j);
                    const double gain = u == v ? p->cost[v] - with : (p->cost[v] - without) - (with - base);
                    if (gain > bestGain) bestGain = gain, bestU = u, bestJ = j;
                }
            }
            insertAt(p->route[bestU], &p->len[bestU], bestJ, c);
            if (bestU != v || bestJ != i) {
                p->cost[v] = routeCost(in, v, p->route[v], p->len[v]);
                p->cost[bestU] = routeCost(in, bestU, p->route[bestU], p->len[bestU]);
                planCost(in, p);
                improved = 1;
            }
        }
    return improved;
}

static void localSearch(const Inst* in, Plan* p) {
    for (int round = 0; round < 50 && !mobilog_cancelled(); ++round) {
        int better = 0;
        for (uint32_t v = 0; v < (uint32_t)in->vehicles; ++v) better |= twoOpt(in, p, v);
        better |= relocate(in, p);
        if (!better) break;
    }
}

/* Perturbação: `count` clientes ao acaso para posições ao acaso (onde couberem). */
static void perturb(const Inst* in, Plan* p, uint32_t count) {
    const uint32_t k = (uint32_t)in->vehicles;
    for (uint32_t s = 0; s < count; ++s) {
        const uint32_t v = mobilog_random() % k;
        if (!p->len[v]) continue;
        const uint32_t i = mobilog_random() % p->len[v];
        const uint32_t c = p->route[v][i];
        const uint32_t u = mobilog_random() % k;
        if (u != v && routeLoad(in, p->route[u], p->len[u]) + in->demand[c] > in->cap[u]) continue;
        removeAt(p->route[v], &p->len[v], i);
        insertAt(p->route[u], &p->len[u], mobilog_random() % (p->len[u] + 1), c);
    }
    planCost(in, p);
}

static uint8_t offerBuf_[1 << 16];

static void offer(const Inst* in, const Plan* p) {
    mlx_writer w;
    mlx_w_init(&w, offerBuf_, sizeof offerBuf_);
    const uint32_t k = (uint32_t)in->vehicles;
    /* Frota mista: a rota k é do veículo k. Senão as vazias saem. */
    uint32_t routes = 0;
    for (uint32_t v = 0; v < k; ++v) routes += in->mixed || p->len[v] > 0;
    mlx_w_u32(&w, routes);
    for (uint32_t v = 0; v < k; ++v) {
        if (!in->mixed && !p->len[v]) continue;
        mlx_w_u32(&w, p->len[v]);
        mlx_w_raw(&w, p->route[v], p->len[v] * 4);
    }
    if (!w.failed) mlx_call(MLX_FN_SOLVER_OFFER, &w, answer_, sizeof answer_);
}

/* Os parâmetros desta execução (Algoritmos). */
static void params(float* perturbation, float* worse) {
    static uint8_t buf[1024];
    const int32_t n = mobilog_call(MLX_FN_SOLVER_PARAMS, 0, 0, buf, sizeof buf);
    if (n < 0) return;
    mlx_reader r;
    mlx_r_init(&r, buf, (size_t)n);
    mlx_r_u64(&r);
    mlx_r_f64(&r);
    mlx_r_f64(&r);
    for (uint32_t i = 0, count = mlx_r_u32(&r); i < count && !r.failed; ++i) {
        uint32_t len;
        const char* key = mlx_r_str(&r, &len);
        const float v = mlx_r_f32(&r);
        if (mlx_str_is(key, len, "perturbacao")) *perturbation = v;
        if (mlx_str_is(key, len, "pioraAceita")) *worse = v;
    }
}

MLX_EXPORT("mlx_task") int32_t task(int32_t kind, const void* data, int32_t size) {
    if (kind != MLX_TASK_SOLVE_VRP) return 1;
    mlx_reader r;
    mlx_r_init(&r, data, (size_t)size);
    Inst in;
    if (!readInstance(&r, &in)) {
        mlx_log(3, "instância com recursos que este resolvedor não trata");
        return 2;
    }
    float perturbation = 4, worse = 1;
    params(&perturbation, &worse);
    Plan best, cur;
    planNew(&in, &best);
    planNew(&in, &cur);
    if (!initial(&in, &cur)) mlx_log(2, "a frota não comporta todos os pedidos");
    localSearch(&in, &cur);
    planCopy(&in, &best, &cur);
    offer(&in, &best);
    /* Busca local iterada até o app pedir para parar (tempo limite, Cancelar). */
    while (!mobilog_cancelled()) {
        perturb(&in, &cur, (uint32_t)perturbation);
        localSearch(&in, &cur);
        if (cur.total + 1e-9 < best.total) {
            planCopy(&in, &best, &cur);
            offer(&in, &best);
        } else if (cur.total > best.total * (1.0 + worse / 100.0)) {
            planCopy(&in, &cur, &best);
        }
    }
    return 0;
}
