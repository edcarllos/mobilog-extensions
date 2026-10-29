/* Pontos críticos — onde o trânsito está mais lento agora.
 *
 * A cada 3 s de simulação lê a velocidade de cada trecho (relativa ao limite), escolhe os trechos
 * mais lentos (pesando comprimento e faixas, um por rua) e:
 *   - pinta esses trechos e os numera no mapa (camada "Pontos críticos");
 *   - lista-os no painel, cada um com "Ir" para o mapa ir até lá;
 *   - mostra a lentidão da rede como indicador na barra e num gráfico que dá para fixar no mapa.
 */
#define MLX_DEFINE_ALLOCATOR
#include "mobilog_ext.h"

#define TOP 8
#define HISTORY 120

/* A malha (lida uma vez por versão). */
typedef struct {
    uint32_t from, to, name;
    float limit, length;
    uint8_t lanes;
} Edge;
static Edge* edges_ = 0;
static uint32_t edgeCount_ = 0;
static double* nodes_ = 0; /* lon, lat */
static char* namesBlob_ = 0;
static uint32_t* nameAt_ = 0; /* início de cada nome no blob (terminado em 0) */
static uint32_t nameCount_ = 0;
static int ready_ = 0;

/* O que foi achado. */
typedef struct {
    uint32_t edge;
    float ratio, score;
} Spot;
static Spot top_[TOP];
static uint32_t topCount_ = 0;
static float index_ = 0;
static float histX_[HISTORY], histY_[HISTORY];
static uint32_t histCount_ = 0;
static double clock_ = 0, lastScan_ = -10;
static int drawnDirty_ = 1;

static uint8_t req_[1 << 16];
static uint8_t ui_[1 << 16];
static uint8_t scratch_[(1 << 16) + 128];

static void loadNetwork(void) {
    const uint8_t* a;
    int32_t n = mlx_call_big(MLX_FN_MAP_EDGES, 0, &a);
    if (n < 0) return;
    mlx_reader r;
    mlx_r_init(&r, a, (size_t)n);
    edgeCount_ = mlx_r_u32(&r);
    mlx_mfree(edges_);
    edges_ = (Edge*)mlx_malloc(edgeCount_ * sizeof(Edge));
    for (uint32_t e = 0; e < edgeCount_; ++e) {
        Edge* x = &edges_[e];
        x->from = mlx_r_u32(&r);
        x->to = mlx_r_u32(&r);
        mlx_r_u8(&r);
        x->lanes = mlx_r_u8(&r);
        mlx_r_u8(&r);
        mlx_r_u8(&r);
        x->limit = mlx_r_f32(&r);
        x->length = mlx_r_f32(&r);
        x->name = mlx_r_u32(&r);
    }
    n = mlx_call_big(MLX_FN_MAP_NODES, 0, &a);
    if (n < 0) return;
    mlx_r_init(&r, a, (size_t)n);
    const uint32_t nodes = mlx_r_u32(&r);
    mlx_mfree(nodes_);
    nodes_ = (double*)mlx_malloc((size_t)nodes * 16);
    mlx_r_raw(&r, nodes_, (size_t)nodes * 16);
    n = mlx_call_big(MLX_FN_MAP_NAMES, 0, &a);
    if (n < 0) return;
    mlx_mfree(namesBlob_);
    mlx_mfree(nameAt_);
    namesBlob_ = (char*)mlx_malloc((size_t)n);
    mlx_r_init(&r, a, (size_t)n);
    nameCount_ = mlx_r_u32(&r);
    nameAt_ = (uint32_t*)mlx_malloc(nameCount_ * 4);
    uint32_t at = 0;
    for (uint32_t i = 0; i < nameCount_; ++i) {
        uint32_t len;
        const char* s = mlx_r_str(&r, &len);
        nameAt_[i] = at;
        __builtin_memcpy(namesBlob_ + at, s, len);
        at += len;
        namesBlob_[at++] = 0;
    }
    ready_ = !r.failed && edgeCount_ > 0;
    topCount_ = 0;
    drawnDirty_ = 1;
}

static const char* nameOf(uint32_t e) {
    const uint32_t n = edges_[e].name;
    return n < nameCount_ ? namesBlob_ + nameAt_[n] : "via sem nome";
}

static void midpoint(uint32_t e, double* lon, double* lat) {
    const double* a = nodes_ + 2 * edges_[e].from;
    const double* b = nodes_ + 2 * edges_[e].to;
    *lon = (a[0] + b[0]) / 2;
    *lat = (a[1] + b[1]) / 2;
}

static int sameName(uint32_t a, uint32_t b) {
    return edges_[a].name == edges_[b].name && edges_[a].name < nameCount_;
}

/* Lê a velocidade de cada trecho e escolhe os mais lentos. */
static void scan(double simMinutes) {
    const uint8_t* a;
    const int32_t n = mlx_call_big(MLX_FN_SIM_SPEED_RATIOS, 0, &a);
    if (n < 0) return;
    mlx_reader r;
    mlx_r_init(&r, a, (size_t)n);
    const uint32_t count = mlx_r_u32(&r);
    const float* ratio = (const float*)mlx_r_skip(&r, (size_t)count * 4);
    if (!ratio || count != edgeCount_) return;
    double lost = 0, weight = 0;
    uint32_t found = 0;
    Spot spots[TOP];
    for (uint32_t e = 0; e < count; ++e) {
        const float q = ratio[e] < 0 ? 0 : ratio[e] > 1 ? 1 : ratio[e];
        const double w = (double)edges_[e].length * (edges_[e].lanes ? edges_[e].lanes : 1);
        lost += w * (1.0 - q);
        weight += w;
        if (q >= 0.5f || edges_[e].length < 40) continue;
        const float score = (0.5f - q) * (float)w;
        /* um por rua: fica o pior trecho de cada uma */
        uint32_t slot = found;
        for (uint32_t i = 0; i < found; ++i)
            if (sameName(spots[i].edge, e)) {
                slot = spots[i].score >= score ? TOP : i;
                break;
            }
        if (slot == TOP) continue;
        if (slot == found) {
            if (found < TOP) ++found;
            else {
                uint32_t worst = 0;
                for (uint32_t i = 1; i < TOP; ++i)
                    if (spots[i].score < spots[worst].score) worst = i;
                if (spots[worst].score >= score) continue;
                slot = worst;
            }
        }
        spots[slot].edge = e;
        spots[slot].ratio = q;
        spots[slot].score = score;
    }
    for (uint32_t i = 1; i < found; ++i) /* do pior para o menos pior */
        for (uint32_t j = i; j > 0 && spots[j].score > spots[j - 1].score; --j) {
            const Spot t = spots[j];
            spots[j] = spots[j - 1];
            spots[j - 1] = t;
        }
    int changed = found != topCount_;
    for (uint32_t i = 0; i < found && !changed; ++i) changed = spots[i].edge != top_[i].edge;
    for (uint32_t i = 0; i < found; ++i) top_[i] = spots[i];
    topCount_ = found;
    drawnDirty_ |= changed;
    index_ = weight > 0 ? (float)(lost / weight) : 0;
    if (histCount_ == HISTORY) {
        for (uint32_t i = 1; i < HISTORY; ++i) histX_[i - 1] = histX_[i], histY_[i - 1] = histY_[i];
        --histCount_;
    }
    histX_[histCount_] = (float)simMinutes;
    histY_[histCount_] = index_ * 100.0f;
    ++histCount_;
}

static void draw(void) {
    mlx_writer w;
    mlx_w_init(&w, req_, sizeof req_);
    mlx_w_str(&w, "criticos");
    mlx_w_u32(&w, topCount_); /* linhas: o trecho, vermelho e grosso */
    for (uint32_t i = 0; i < topCount_; ++i) {
        mlx_writer q;
        uint8_t qb[8];
        mlx_w_init(&q, qb, sizeof qb);
        mlx_w_u32(&q, top_[i].edge);
        const uint8_t* a;
        const int32_t n = mlx_call_big(MLX_FN_MAP_EDGE_POINTS, &q, &a);
        mlx_w_u32(&w, 0xE02020FFu);
        mlx_w_f32(&w, 8);
        if (n < 0) {
            mlx_w_u32(&w, 0);
            continue;
        }
        mlx_w_raw(&w, a, (size_t)n); /* a resposta já é uma lista de pontos */
    }
    mlx_w_u32(&w, topCount_); /* marcadores numerados */
    for (uint32_t i = 0; i < topCount_; ++i) {
        double lon, lat;
        midpoint(top_[i].edge, &lon, &lat);
        mlx_w_point(&w, lon, lat);
        mlx_w_str(&w, "dot");
        mlx_w_u32(&w, 0xFF2020E0u);
        char label[8];
        mlx_fmt_int(label, i + 1);
        mlx_w_str(&w, label);
    }
    mlx_w_u32(&w, 0); /* áreas */
    mlx_w_u32(&w, 0); /* cores por trecho */
    if (!w.failed) mobilog_call(MLX_FN_DRAW_SET_LAYER, w.data, (int32_t)w.size, 0, 0);
    drawnDirty_ = 0;
}

static void kpis(void) {
    mlx_writer w;
    mlx_w_init(&w, req_, sizeof req_);
    char value[16];
    mlx_fmt_num(value, index_ * 100.0, 0);
    mlx_cat(value + mlx_strlen(value), 4, "%", (const char*)0);
    mlx_w_u32(&w, 1);
    mlx_w_str(&w, "Lentidão");
    mlx_w_str(&w, value);
    mlx_w_str(&w, "lentidão");
    mlx_w_str(&w, "Quanto da velocidade permitida se perde na rede, pesado pelo tamanho das vias (Pontos críticos)");
    mlx_w_u8(&w, index_ > 0.35f ? 2 : 0);
    mobilog_call(MLX_FN_UI_KPIS, w.data, (int32_t)w.size, 0, 0);
}

static void chart(mlx_writer* w) {
    mlx_ui_line_chart(w, 1, "Lentidão da rede (%)", "lentidão", 0, histX_, histY_, histCount_);
}

static void panel(void) {
    mlx_writer w;
    mlx_w_init(&w, ui_, sizeof ui_);
    mlx_ui_begin(&w);
    char text[256], num[32], pct[32];
    mlx_fmt_num(num, index_ * 100.0, 0);
    mlx_ui_kpi(&w, "Lentidão da rede", mlx_cat(text, sizeof text, num, "%", (const char*)0), index_ > 0.35f ? 2 : 0);
    mlx_ui_section(&w, 10, "Mais lentos agora", topCount_ ? "" : "nenhum", 1);
    if (topCount_) mlx_ui_text(&w, 1, "Toque no número para ver o trecho no mapa.");
    if (!ready_) mlx_ui_text(&w, 1, "Abra uma área e deixe a simulação andar.");
    else if (!topCount_) mlx_ui_text(&w, 1, "Nenhum trecho abaixo da metade do limite de velocidade.");
    for (uint32_t i = 0; i < topCount_; ++i) {
        const Edge* e = &edges_[top_[i].edge];
        mlx_fmt_int(num, i + 1);
        char kmh[16];
        mlx_fmt_num(kmh, (double)top_[i].ratio * e->limit * 3.6, 0);
        mlx_fmt_num(pct, (double)top_[i].ratio * 100.0, 0);
        mlx_ui_button(&w, 100 + i, num, 0, 0); /* "Ir": o número do ponto, que leva o mapa até ele */
        mlx_ui_same_line(&w);
        mlx_ui_text(&w, i < 3 ? 5 : 4, mlx_cat(text, sizeof text, nameOf(top_[i].edge), " — ", kmh, " km/h (", pct, "% do limite)", (const char*)0));
    }
    mlx_ui_end_section(&w);
    mlx_ui_section(&w, 11, "Ao longo do tempo", "", 1);
    chart(&w);
    mlx_ui_text(&w, 1, "Fixe este gráfico no mapa pelo menu de gráficos (Métricas).");
    mlx_ui_end_section(&w);
    mlx_ui_send(MLX_FN_UI_PANEL, "ranking", &w, scratch_, sizeof scratch_);
}

MLX_EXPORT("mlx_api_version") int32_t api_version(void) { return MLX_API_VERSION; }

MLX_EXPORT("mlx_init") int32_t init(void) {
    uint32_t mask = 1u << MLX_EV_FRAME;
    mobilog_call(MLX_FN_SUBSCRIBE, &mask, 4, 0, 0);
    return 0;
}

MLX_EXPORT("mlx_event") int32_t event(int32_t kind, const void* data, int32_t size) {
    mlx_reader r;
    mlx_r_init(&r, data, (size_t)size);
    switch (kind) {
    case MLX_EV_NETWORK: ready_ = 0; return 0;
    case MLX_EV_FRAME: {
        clock_ += mlx_r_f64(&r);
        if (clock_ - lastScan_ < 1.0) return 0; /* olha a cada segundo de relógio... */
        lastScan_ = clock_;
        static uint8_t info[128];
        const int32_t n = mobilog_call(MLX_FN_SIM_INFO, 0, 0, info, sizeof info);
        if (n < 0) return 0;
        mlx_reader s;
        mlx_r_init(&s, info, (size_t)n);
        const double simTime = mlx_r_f64(&s);
        static double lastSim = -100;
        if (simTime - lastSim < 3.0 && simTime >= lastSim) return 0; /* ...e mede a cada 3 s de simulação */
        lastSim = simTime;
        if (!ready_) loadNetwork();
        if (!ready_) return 0;
        scan(simTime / 60.0);
        if (drawnDirty_) draw();
        kpis();
        return 0;
    }
    case MLX_EV_PANEL: panel(); return 0;
    case MLX_EV_CHART: {
        mlx_writer w;
        mlx_w_init(&w, ui_, sizeof ui_);
        mlx_ui_begin(&w);
        chart(&w);
        mlx_ui_send(MLX_FN_UI_CHART, "lentidao", &w, scratch_, sizeof scratch_);
        return 0;
    }
    case MLX_EV_UI: {
        uint32_t len;
        mlx_r_str(&r, &len);
        for (uint32_t i = 0, count = mlx_r_u32(&r); i < count && !r.failed; ++i) {
            const uint32_t id = mlx_r_u32(&r);
            mlx_r_u8(&r);
            mlx_r_f64(&r);
            mlx_r_str(&r, &len);
            if (id >= 100 && id < 100 + topCount_) {
                double p[2];
                midpoint(top_[id - 100].edge, &p[0], &p[1]);
                uint8_t q[20];
                __builtin_memcpy(q, p, 16);
                const float zoom = 16.5f;
                __builtin_memcpy(q + 16, &zoom, 4);
                mobilog_call(MLX_FN_UI_FOCUS, q, 20, 0, 0);
            }
        }
        return 0;
    }
    default: return 0;
    }
}
