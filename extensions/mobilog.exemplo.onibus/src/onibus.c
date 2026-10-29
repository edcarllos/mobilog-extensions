/* Ônibus em linha — uma linha circular com paradas marcadas no mapa e ônibus rodando nela, dentro da
 * simulação (veículos da extensão, no meio do trânsito).
 *
 * - Ferramenta "Paradas": cada toque marca uma parada (no cruzamento mais próximo); a linha liga as
 *   paradas na ordem e volta à primeira, pelo caminho mais rápido.
 * - "Iniciar": N ônibus saem da primeira parada a cada intervalo; em cada parada esperam o tempo de
 *   parada e seguem para a próxima.
 * - Indicadores: tempo de volta, intervalo real entre ônibus na primeira parada e a regularidade (quanto
 *   os intervalos variam: ônibus que se atrasam no trânsito acabam colados no da frente).
 */
#define MLX_DEFINE_ALLOCATOR
#include "mobilog_ext.h"

#define MAX_STOPS 16
#define MAX_BUSES 12
#define HIST 40

typedef struct {
    double lon, lat;
    uint32_t node;
} Stop;

typedef struct {
    uint32_t handle;
    uint32_t segment;  /* o trecho que vai rodar (da parada `segment` à seguinte) */
    int driving;
    double departAt, lapStart;
} Bus;

static Stop stops_[MAX_STOPS];
static uint32_t stopCount_ = 0;
static uint32_t* segEdges_ = 0;
static uint32_t segStart_[MAX_STOPS], segLen_[MAX_STOPS];
static float segTime_[MAX_STOPS], segMeters_[MAX_STOPS];
static int routed_ = 0;
static char status_[160] = "";

static Bus buses_[MAX_BUSES];
static uint32_t busCount_ = 0;
static int running_ = 0, toolOn_ = 0;
static int busesWanted_ = 4, headwayMin_ = 6, dwellS_ = 30;
static double now_ = 0, lastAtFirst_ = -1;
static float laps_[HIST], gaps_[HIST], gapX_[HIST], gapY_[HIST];
static uint32_t lapCount_ = 0, gapCount_ = 0;

static uint8_t req_[1 << 16];
static uint8_t ui_[1 << 16];
static uint8_t scratch_[(1 << 16) + 128];
static uint8_t small_[256];

static void push(float* list, uint32_t* count, float v) {
    if (*count == HIST) {
        for (uint32_t i = 1; i < HIST; ++i) list[i - 1] = list[i];
        --*count;
    }
    list[(*count)++] = v;
}

static double mean(const float* v, uint32_t n) {
    double s = 0;
    for (uint32_t i = 0; i < n; ++i) s += v[i];
    return n ? s / n : 0;
}

/* Coeficiente de variação: desvio padrão / média (0 = intervalos iguais). */
static double variation(const float* v, uint32_t n) {
    const double m = mean(v, n);
    if (n < 2 || m <= 0) return 0;
    double s = 0;
    for (uint32_t i = 0; i < n; ++i) s += (v[i] - m) * (v[i] - m);
    s /= n - 1;
    /* raiz quadrada por Newton (sem a biblioteca C) */
    double r = s > 1 ? s : 1;
    for (int i = 0; i < 40; ++i) r = 0.5 * (r + s / r);
    return r / m;
}

static int32_t nearestNode(double lon, double lat, uint32_t* node) {
    double p[2] = {lon, lat};
    const int32_t n = mobilog_call(MLX_FN_MAP_NEAREST_NODE, p, 16, small_, sizeof small_);
    if (n < 4) return n < 0 ? n : MLX_ERR_INVALID;
    __builtin_memcpy(node, small_, 4);
    return 0;
}

/* ---- As paradas no cenário: {"paradas":[[lon,lat],...]} --------------------------------------- */

static void saveStops(void) {
    static char json[MAX_STOPS * 48 + 32];
    size_t k = 0;
    for (const char* h = "{\"paradas\":["; *h;) json[k++] = *h++;
    for (uint32_t i = 0; i < stopCount_; ++i) {
        if (i) json[k++] = ',';
        json[k++] = '[';
        k += mlx_fmt_json(json + k, stops_[i].lon, 6);
        json[k++] = ',';
        k += mlx_fmt_json(json + k, stops_[i].lat, 6);
        json[k++] = ']';
    }
    json[k++] = ']';
    json[k++] = '}';
    mlx_writer w;
    mlx_w_init(&w, req_, sizeof req_);
    mlx_w_strn(&w, json, k);
    mobilog_call(MLX_FN_SCENARIO_DATA_SET, w.data, (int32_t)w.size, 0, 0);
}

static void loadStops(void) {
    stopCount_ = 0;
    const int32_t n = mobilog_call(MLX_FN_SCENARIO_DATA_GET, 0, 0, ui_, sizeof ui_);
    if (n < 4) return;
    mlx_reader r;
    mlx_r_init(&r, ui_, (size_t)n);
    uint32_t len;
    const char* s = mlx_r_str(&r, &len);
    double v[MAX_STOPS * 2];
    uint32_t count = 0;
    for (uint32_t i = 0; i < len && count < MAX_STOPS * 2;) { /* os números do texto, na ordem */
        if (s[i] == '-' || (s[i] >= '0' && s[i] <= '9')) {
            const int neg = s[i] == '-';
            if (neg) ++i;
            double x = 0, scale = 0;
            for (; i < len && ((s[i] >= '0' && s[i] <= '9') || s[i] == '.'); ++i)
                if (s[i] == '.') scale = 1;
                else x = x * 10 + (s[i] - '0'), scale = scale ? scale * 10 : 0;
            v[count++] = (neg ? -x : x) / (scale ? scale : 1);
        } else {
            ++i;
        }
    }
    for (uint32_t i = 0; i + 1 < count; i += 2) {
        Stop* p = &stops_[stopCount_];
        p->lon = v[i];
        p->lat = v[i + 1];
        if (nearestNode(p->lon, p->lat, &p->node) == 0) ++stopCount_;
    }
}

/* ---- A linha: o caminho entre paradas seguidas ----------------------------------------------- */

static void routeLine(void) {
    routed_ = 0;
    status_[0] = 0;
    if (stopCount_ < 2) return;
    mlx_mfree(segEdges_);
    segEdges_ = 0;
    uint32_t total = 0, cap = 0;
    for (uint32_t i = 0; i < stopCount_; ++i) {
        uint8_t q[9];
        const uint32_t a = stops_[i].node, b = stops_[(i + 1) % stopCount_].node;
        __builtin_memcpy(q, &a, 4);
        __builtin_memcpy(q + 4, &b, 4);
        q[8] = 1; /* tempo com vias livres */
        mlx_writer w = {q, 9, 9, 0};
        const uint8_t* ans;
        const int32_t n = mlx_call_big(MLX_FN_ROUTE, &w, &ans);
        mlx_reader r;
        mlx_r_init(&r, ans, n > 0 ? (size_t)n : 0);
        segTime_[i] = mlx_r_f32(&r);
        segMeters_[i] = mlx_r_f32(&r);
        const uint32_t len = mlx_r_u32(&r);
        if (n < 0 || r.failed || len == 0) {
            char num[8];
            mlx_fmt_int(num, i + 1);
            mlx_cat(status_, sizeof status_, "Não há caminho saindo da parada ", num, " para a seguinte.", (const char*)0);
            return;
        }
        if (total + len > cap) {
            cap = (total + len) * 2;
            uint32_t* grown = (uint32_t*)mlx_malloc(cap * 4);
            if (segEdges_) __builtin_memcpy(grown, segEdges_, total * 4);
            mlx_mfree(segEdges_);
            segEdges_ = grown;
        }
        segStart_[i] = total;
        segLen_[i] = len;
        mlx_r_raw(&r, segEdges_ + total, (size_t)len * 4);
        total += len;
    }
    routed_ = 1;
}

static void draw(void) {
    mlx_writer w;
    mlx_w_init(&w, req_, sizeof req_);
    mlx_w_str(&w, "linha");
    mlx_w_u32(&w, routed_ ? stopCount_ : 0);
    for (uint32_t i = 0; routed_ && i < stopCount_; ++i) {
        mlx_w_u32(&w, 0xFF1E88E5u); /* laranja (0xAABBGGRR) */
        mlx_w_f32(&w, 6);
        uint8_t* countAt = w.data + w.size;
        mlx_w_u32(&w, 0);
        uint32_t points = 0;
        for (uint32_t k = 0; k < segLen_[i]; ++k) {
            uint8_t q[4];
            __builtin_memcpy(q, &segEdges_[segStart_[i] + k], 4);
            mlx_writer e = {q, 4, 4, 0};
            const uint8_t* ans;
            const int32_t n = mlx_call_big(MLX_FN_MAP_EDGE_POINTS, &e, &ans);
            if (n < 4) continue;
            uint32_t c;
            __builtin_memcpy(&c, ans, 4);
            const uint32_t skip = points ? 1 : 0; /* o primeiro ponto repete o último do trecho anterior */
            mlx_w_raw(&w, ans + 4 + skip * 16, (size_t)(c - skip) * 16);
            points += c - skip;
        }
        if (!w.failed) __builtin_memcpy(countAt, &points, 4);
    }
    mlx_w_u32(&w, stopCount_);
    for (uint32_t i = 0; i < stopCount_; ++i) {
        char label[8] = "P", num[8];
        mlx_fmt_int(num, i + 1);
        mlx_w_point(&w, stops_[i].lon, stops_[i].lat);
        mlx_w_str(&w, "pin");
        mlx_w_u32(&w, 0);
        mlx_w_str(&w, mlx_cat(label, sizeof label, "P", num, (const char*)0));
    }
    mlx_w_u32(&w, 0);
    mlx_w_u32(&w, 0);
    if (!w.failed) mobilog_call(MLX_FN_DRAW_SET_LAYER, w.data, (int32_t)w.size, 0, 0);
}

/* ---- Operação --------------------------------------------------------------------------------- */

static void stopOperation(void) {
    for (uint32_t i = 0; i < busCount_; ++i) mobilog_call(MLX_FN_VEHICLE_REMOVE, &buses_[i].handle, 4, 0, 0);
    busCount_ = 0;
    running_ = 0;
}

static void startOperation(void) {
    stopOperation();
    if (!routed_) return;
    static uint8_t info[128]; /* o relógio de agora (depois de a simulação recomeçar, ele voltou ao início) */
    if (mobilog_call(MLX_FN_SIM_INFO, 0, 0, info, sizeof info) >= 8) __builtin_memcpy(&now_, info, 8);
    lapCount_ = gapCount_ = 0;
    lastAtFirst_ = -1;
    for (int k = 0; k < busesWanted_ && k < MAX_BUSES; ++k) {
        uint8_t q[10];
        __builtin_memcpy(q, &stops_[0].node, 4);
        const float speed = 0.9f; /* um pouco mais devagar que um carro */
        __builtin_memcpy(q + 4, &speed, 4);
        q[8] = 2;           /* van: o ícone mais próximo de um ônibus */
        q[9] = (uint8_t)k;  /* cor */
        if (mobilog_call(MLX_FN_VEHICLE_CREATE, q, 10, small_, sizeof small_) < 4) break;
        Bus* b = &buses_[busCount_++];
        __builtin_memcpy(&b->handle, small_, 4);
        b->segment = 0;
        b->driving = 0;
        b->departAt = now_ + (double)k * headwayMin_ * 60.0;
        b->lapStart = -1;
    }
    running_ = busCount_ > 0;
}

static void depart(Bus* b) {
    mlx_writer w;
    mlx_w_init(&w, req_, sizeof req_);
    mlx_w_u32(&w, b->handle);
    mlx_w_u32(&w, segLen_[b->segment]);
    mlx_w_raw(&w, segEdges_ + segStart_[b->segment], (size_t)segLen_[b->segment] * 4);
    if (mobilog_call(MLX_FN_VEHICLE_DEPART, w.data, (int32_t)w.size, 0, 0) == 0) {
        b->driving = 1;
        if (b->segment == 0 && b->lapStart < 0) b->lapStart = now_;
    }
}

static void arrived(uint32_t handle) {
    for (uint32_t i = 0; i < busCount_; ++i) {
        Bus* b = &buses_[i];
        if (b->handle != handle) continue;
        b->driving = 0;
        b->segment = (b->segment + 1) % stopCount_;
        if (b->segment == 0) { /* de volta à primeira parada */
            if (b->lapStart >= 0) push(laps_, &lapCount_, (float)(now_ - b->lapStart));
            b->lapStart = now_;
            if (lastAtFirst_ >= 0) { /* o intervalo desde o ônibus anterior, e quando foi (para o gráfico) */
                const float gap = (float)(now_ - lastAtFirst_);
                if (gapCount_ == HIST) {
                    for (uint32_t k = 1; k < HIST; ++k) gaps_[k - 1] = gaps_[k], gapX_[k - 1] = gapX_[k], gapY_[k - 1] = gapY_[k];
                    --gapCount_;
                }
                gaps_[gapCount_] = gap;
                gapX_[gapCount_] = (float)(now_ / 60.0);
                gapY_[gapCount_] = gap / 60.0f;
                ++gapCount_;
            }
            lastAtFirst_ = now_;
        }
        b->departAt = now_ + dwellS_;
        return;
    }
}

static void kpis(void) {
    mlx_writer w;
    mlx_w_init(&w, req_, sizeof req_);
    if (!running_ || (!lapCount_ && !gapCount_)) {
        mlx_w_u32(&w, 0);
    } else {
        char lap[24], gap[24], reg[24];
        mlx_fmt_num(lap, mean(laps_, lapCount_) / 60.0, 1);
        mlx_cat(lap + mlx_strlen(lap), 8, " min", (const char*)0);
        mlx_fmt_num(gap, mean(gaps_, gapCount_) / 60.0, 1);
        mlx_cat(gap + mlx_strlen(gap), 8, " min", (const char*)0);
        const double cv = variation(gaps_, gapCount_);
        mlx_fmt_num(reg, cv * 100.0, 0);
        mlx_cat(reg + mlx_strlen(reg), 4, "%", (const char*)0);
        mlx_w_u32(&w, 3);
        mlx_w_str(&w, "Volta");
        mlx_w_str(&w, lapCount_ ? lap : "—");
        mlx_w_str(&w, "volta");
        mlx_w_str(&w, "Tempo médio de uma volta completa da linha (Ônibus em linha)");
        mlx_w_u8(&w, 0);
        mlx_w_str(&w, "Intervalo");
        mlx_w_str(&w, gapCount_ ? gap : "—");
        mlx_w_str(&w, "intervalo");
        mlx_w_str(&w, "Tempo médio entre dois ônibus na primeira parada");
        mlx_w_u8(&w, 0);
        mlx_w_str(&w, "Variação");
        mlx_w_str(&w, gapCount_ > 1 ? reg : "—");
        mlx_w_str(&w, "");
        mlx_w_str(&w, "Quanto os intervalos variam (desvio padrão / média): acima de 30% os ônibus andam colados");
        mlx_w_u8(&w, cv > 0.3 ? 2 : 1);
    }
    mobilog_call(MLX_FN_UI_KPIS, w.data, (int32_t)w.size, 0, 0);
}

static void setTool(int on) {
    uint8_t q[16];
    mlx_writer w;
    mlx_w_init(&w, q, sizeof q);
    mlx_w_str(&w, "paradas");
    mlx_w_u8(&w, (uint8_t)on);
    if (mobilog_call(MLX_FN_UI_TOOL, w.data, (int32_t)w.size, 0, 0) == 0) toolOn_ = on;
}

static void panel(void) {
    mlx_writer w;
    mlx_w_init(&w, ui_, sizeof ui_);
    mlx_ui_begin(&w);
    char text[200], a[24], b[24];
    mlx_fmt_int(a, stopCount_);
    mlx_ui_section(&w, 1, "Linha", mlx_cat(text, sizeof text, a, stopCount_ == 1 ? " parada" : " paradas", (const char*)0), 1);
    mlx_ui_text(&w, 1, "Toque no mapa para marcar as paradas, na ordem. A linha volta da última à primeira.");
    mlx_ui_button(&w, 10, toolOn_ ? "Parar de marcar" : "Marcar paradas", !toolOn_, running_);
    mlx_ui_same_line(&w);
    mlx_ui_button(&w, 11, "Limpar", 0, running_ || !stopCount_);
    if (routed_) {
        double meters = 0, seconds = 0;
        for (uint32_t i = 0; i < stopCount_; ++i) meters += segMeters_[i], seconds += segTime_[i];
        mlx_fmt_num(a, meters / 1000.0, 1);
        mlx_fmt_num(b, (seconds + (double)dwellS_ * stopCount_) / 60.0, 0);
        mlx_ui_text(&w, 0, mlx_cat(text, sizeof text, "Percurso de ", a, " km; volta de uns ", b, " min com vias livres e as paradas.",
                                   (const char*)0));
    }
    if (status_[0]) mlx_ui_text(&w, 4, status_);
    mlx_ui_end_section(&w);

    mlx_ui_section(&w, 2, "Operação", running_ ? "rodando" : "", 1);
    mlx_ui_number(&w, 20, "Ônibus", busesWanted_, 1, MAX_BUSES, 1, 0, "Quantos ônibus rodam na linha");
    mlx_ui_number(&w, 21, "Intervalo (min)", headwayMin_, 2, 30, 1, 0, "De quanto em quanto tempo sai um ônibus da primeira parada");
    mlx_ui_number(&w, 22, "Tempo de parada (s)", dwellS_, 10, 120, 5, 0, "Quanto cada ônibus espera em cada parada");
    if (!running_) mlx_ui_button(&w, 23, "Iniciar operação", 1, !routed_);
    else mlx_ui_button(&w, 24, "Parar operação", 0, 0);
    if (running_) {
        mlx_fmt_num(a, mean(laps_, lapCount_) / 60.0, 1);
        mlx_ui_kpi(&w, "Volta média", lapCount_ ? mlx_cat(text, sizeof text, a, " min", (const char*)0) : "aguardando", 0);
        mlx_fmt_num(b, mean(gaps_, gapCount_) / 60.0, 1);
        mlx_ui_kpi(&w, "Intervalo real", gapCount_ ? mlx_cat(text, sizeof text, b, " min", (const char*)0) : "aguardando", 0);
        if (gapCount_ > 1) mlx_ui_line_chart(&w, 30, "Intervalo na primeira parada (min)", "intervalo", 0, gapX_, gapY_, gapCount_);
    }
    mlx_ui_end_section(&w);
    mlx_ui_send(MLX_FN_UI_PANEL, "linha", &w, scratch_, sizeof scratch_);
}

static void changed(void) {
    routeLine();
    draw();
}

MLX_EXPORT("mlx_api_version") int32_t api_version(void) { return MLX_API_VERSION; }

MLX_EXPORT("mlx_init") int32_t init(void) {
    uint32_t mask = (1u << MLX_EV_FRAME) | (1u << MLX_EV_VEHICLE_ARRIVED);
    mobilog_call(MLX_FN_SUBSCRIBE, &mask, 4, 0, 0);
    loadStops();
    changed();
    return 0;
}

MLX_EXPORT("mlx_shutdown") void shutdown(void) { stopOperation(); }

MLX_EXPORT("mlx_event") int32_t event(int32_t kind, const void* data, int32_t size) {
    mlx_reader r;
    mlx_r_init(&r, data, (size_t)size);
    switch (kind) {
    case MLX_EV_PANEL: panel(); return 0;
    case MLX_EV_FRAME: {
        if (!running_) return 0;
        static uint8_t info[128];
        const int32_t n = mobilog_call(MLX_FN_SIM_INFO, 0, 0, info, sizeof info);
        if (n < 8) return 0;
        __builtin_memcpy(&now_, info, 8);
        for (uint32_t i = 0; i < busCount_; ++i)
            if (!buses_[i].driving && now_ >= buses_[i].departAt) depart(&buses_[i]);
        static double lastKpi = -10;
        if (now_ - lastKpi > 2 || now_ < lastKpi) lastKpi = now_, kpis();
        return 0;
    }
    case MLX_EV_VEHICLE_ARRIVED:
        for (uint32_t i = 0, count = mlx_r_u32(&r); i < count && !r.failed; ++i) arrived(mlx_r_u32(&r));
        return 0;
    case MLX_EV_NETWORK: { /* os veículos ficaram com a malha antiga: a operação recomeça na nova */
        const int was = running_;
        busCount_ = 0;
        running_ = 0;
        loadStops();
        changed();
        if (was && routed_) startOperation();
        kpis();
        return 0;
    }
    case MLX_EV_TOOL: {
        uint32_t len;
        mlx_r_str(&r, &len);
        const uint8_t action = mlx_r_u8(&r);
        const double lon = mlx_r_f64(&r), lat = mlx_r_f64(&r);
        if (action == MLX_TOOL_OFF) {
            toolOn_ = 0;
            return 0;
        }
        if (action != MLX_TOOL_TAP || running_) return 0;
        if (stopCount_ >= MAX_STOPS) return 1;
        Stop* s = &stops_[stopCount_];
        if (nearestNode(lon, lat, &s->node) != 0) return 1;
        s->lon = lon;
        s->lat = lat;
        ++stopCount_;
        saveStops();
        changed();
        return 1;
    }
    case MLX_EV_UI: {
        uint32_t len;
        mlx_r_str(&r, &len);
        for (uint32_t i = 0, count = mlx_r_u32(&r); i < count && !r.failed; ++i) {
            const uint32_t id = mlx_r_u32(&r);
            mlx_r_u8(&r);
            const double value = mlx_r_f64(&r);
            mlx_r_str(&r, &len);
            if (id == 10) setTool(!toolOn_);
            else if (id == 11) {
                stopCount_ = 0;
                saveStops();
                changed();
            } else if (id == 20) busesWanted_ = (int)value;
            else if (id == 21) headwayMin_ = (int)value;
            else if (id == 22) dwellS_ = (int)value;
            else if (id == 23) {
                if (toolOn_) setTool(0);
                startOperation();
            } else if (id == 24) {
                stopOperation();
                kpis();
            }
        }
        return 0;
    }
    default: return 0;
    }
}
