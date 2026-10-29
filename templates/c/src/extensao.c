/* Um ponto de partida em C: um painel com o número de veículos na simulação e um botão que marca o
 * centro do mapa numa camada própria. Veja docs/pt/guia-rapido.md (en: docs/en/quick-start.md).
 *
 * Sem a biblioteca C: só o SDK (mobilog_ext.h). Variáveis globais guardam o estado; buffers estáticos
 * guardam os pedidos e as respostas. */
#define MLX_DEFINE_ALLOCATOR
#include "mobilog_ext.h"

static int marcas_ = 0;
static uint8_t ui_[4096];
static uint8_t scratch_[4096 + 128];

MLX_EXPORT("mlx_api_version") int32_t api_version(void) { return MLX_API_VERSION; }

MLX_EXPORT("mlx_init") int32_t init(void) {
    mlx_log(1, "{{NAME}} pronta");
    return 0;
}

/* O painel está na tela: mande o conteúdo dele. */
static void panel(void) {
    uint8_t info[128];
    char veiculos[32] = "—";
    const int32_t n = mobilog_call(MLX_FN_SIM_INFO, 0, 0, info, sizeof info);
    if (n >= 36) { /* f64 tempo, f64 hora, f64 ms, i32 dia, u32 veículos ativos, ... */
        mlx_reader r;
        mlx_r_init(&r, info, (size_t)n);
        mlx_r_skip(&r, 8 + 8 + 8 + 4);
        mlx_fmt_int(veiculos, mlx_r_u32(&r));
    }
    mlx_writer w;
    mlx_w_init(&w, ui_, sizeof ui_);
    mlx_ui_begin(&w);
    mlx_ui_section(&w, 1, "Agora", 0, 1);
    mlx_ui_kpi(&w, "Veículos", veiculos, 0);
    mlx_ui_end_section(&w);
    mlx_ui_button(&w, 1, "Marcar o centro do mapa", 1, 0);
    if (marcas_) {
        char t[48];
        mlx_fmt_int(t, marcas_);
        mlx_ui_text(&w, 1, mlx_cat(t + 24, 24, t, marcas_ == 1 ? " marca" : " marcas", (const char*)0));
    }
    mlx_ui_send(MLX_FN_UI_PANEL, "principal", &w, scratch_, sizeof scratch_);
}

/* O botão: lê os limites da área e põe uma marca no meio, na camada "marcas". */
static void marcar(void) {
    uint8_t info[256];
    const int32_t n = mobilog_call(MLX_FN_MAP_INFO, 0, 0, info, sizeof info);
    if (n < 44) return; /* sem área aberta */
    mlx_reader r;
    mlx_r_init(&r, info, (size_t)n);
    mlx_r_skip(&r, 12);
    const double minLon = mlx_r_f64(&r), minLat = mlx_r_f64(&r), maxLon = mlx_r_f64(&r), maxLat = mlx_r_f64(&r);
    ++marcas_;
    uint8_t req[256];
    mlx_writer w;
    mlx_w_init(&w, req, sizeof req);
    mlx_w_str(&w, "marcas");
    mlx_w_u32(&w, 0); /* linhas: nenhuma */
    mlx_w_u32(&w, 1); /* marcadores: um */
    mlx_w_point(&w, (minLon + maxLon) / 2, (minLat + maxLat) / 2);
    mlx_w_str(&w, "flag");
    mlx_w_u32(&w, 0);
    mlx_w_str(&w, "centro");
    mlx_w_u32(&w, 0); /* áreas: nenhuma */
    mlx_w_u32(&w, 0); /* cores por trecho: nenhuma */
    mlx_call(MLX_FN_DRAW_SET_LAYER, &w, 0, 0);
}

MLX_EXPORT("mlx_event") int32_t event(int32_t kind, const void* data, int32_t size) {
    mlx_reader r;
    mlx_r_init(&r, data, (size_t)size);
    switch (kind) {
    case MLX_EV_PANEL: panel(); return 0;
    case MLX_EV_UI: { /* texto painel, u32 quantidade, e por ação: u32 widget, u8 tipo, f64 valor, texto */
        uint32_t len;
        mlx_r_str(&r, &len);
        for (uint32_t i = 0, count = mlx_r_u32(&r); i < count && !r.failed; ++i) {
            const uint32_t id = mlx_r_u32(&r);
            mlx_r_u8(&r);
            mlx_r_f64(&r);
            mlx_r_str(&r, &len);
            if (id == 1) marcar();
        }
        return 0;
    }
    default: return 0;
    }
}
