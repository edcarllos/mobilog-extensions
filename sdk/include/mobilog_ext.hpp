// mobilog_ext.hpp — a camada C++ do SDK sobre mobilog_ext.h.
//
// Não usa a biblioteca padrão nem exceções: compila do mesmo jeito enxuto que o C (-nostdlib
// -fno-builtin -fno-exceptions -fno-rtti) e também com o wasi-sdk completo.
//
//   #define MLX_DEFINE_ALLOCATOR      // num arquivo só (ou use a biblioteca C do wasi-sdk)
//   #include "mobilog_ext.hpp"
//
//   struct MinhaExtensao : mlx::Extension<MinhaExtensao> {
//       int cliques = 0;                                   // só inicializadores constantes (ver abaixo)
//       int32_t init() { mlx::log(1, "pronta"); return 0; }
//       void panel(mlx::Str id) {
//           mlx::Ui ui;
//           ui.text("Cliques: ").sameLine().text(mlx::Num(cliques));
//           ui.button(1, "Clique aqui", true);
//           ui.send(id);
//       }
//       void ui(mlx::Str panel, const mlx::UiAction& a) { if (a.id == 1) ++cliques; }
//   };
//   MLX_EXTENSION(MinhaExtensao)
//
// A extensão é um objeto estático: os membros precisam de inicializadores constantes (números, ponteiros,
// arrays fixos), porque nada roda antes de mlx_init. Os ganchos são métodos com estes nomes; os que a
// classe não define ficam com o padrão de mlx::Extension (não fazem nada):
//   init(), shutdown(), frame(dt), panel(id), chart(id), ui(panel, ação), tool(id, ação, lon, lat),
//   tap(lon, lat), taskDone(tarefa), vehiclesArrived(veículos, n), network(versão), scenario(),
//   experimentDone(ensaio), task(tipo, entrada) e solve(id, instância).
#pragma once

#include "mobilog_ext.h"

namespace mlx {

// ---- Texto ------------------------------------------------------------------------------------

// Um texto que não é dono dos bytes (de um bloco, ou um literal).
struct Str {
    const char* data = "";
    uint32_t size = 0;
    constexpr Str() = default;
    Str(const char* s) : data(s), size((uint32_t)mlx_strlen(s)) {}
    constexpr Str(const char* s, uint32_t n) : data(s), size(n) {}
    bool operator==(const char* s) const { return mlx_str_is(data, size, s) != 0; }
    bool empty() const { return size == 0; }
};

// Um texto montado num buffer fixo: Text<64> t; t << "Rua " << Num(3) << " km".
template <unsigned N>
struct Text {
    char data[N] = {};
    uint32_t size = 0;
    Text& operator<<(Str s) {
        for (uint32_t i = 0; i < s.size && size + 1 < N; ++i) data[size++] = s.data[i];
        data[size] = 0;
        return *this;
    }
    operator Str() const { return Str(data, size); }
    const char* c_str() const { return data; }
};

// Um número em texto (vírgula decimal): Num(3.25, 1) → "3,3".
struct Num {
    char data[32];
    uint32_t size;
    Num(double v, int decimals = 0) { size = (uint32_t)mlx_fmt_num(data, v, decimals); }
    operator Str() const { return Str(data, size); }
};

// ---- Blocos ----------------------------------------------------------------------------------

// Monta um pedido num buffer: o seu, ou o interno de 64 KB, que todo Writer() sem buffer reaproveita
// (monte um pedido de cada vez).
class Writer {
public:
    Writer() { mlx_w_init(&w_, scratch(), kScratch); }
    Writer(void* buffer, size_t capacity) { mlx_w_init(&w_, buffer, capacity); }
    Writer& u8(uint8_t v) { return mlx_w_u8(&w_, v), *this; }
    Writer& u32(uint32_t v) { return mlx_w_u32(&w_, v), *this; }
    Writer& i32(int32_t v) { return mlx_w_i32(&w_, v), *this; }
    Writer& u64(uint64_t v) { return mlx_w_u64(&w_, v), *this; }
    Writer& f32(float v) { return mlx_w_f32(&w_, v), *this; }
    Writer& f64(double v) { return mlx_w_f64(&w_, v), *this; }
    Writer& str(Str s) { return mlx_w_strn(&w_, s.data, s.size), *this; }
    Writer& point(double lon, double lat) { return mlx_w_point(&w_, lon, lat), *this; }
    Writer& raw(const void* p, size_t n) { return mlx_w_raw(&w_, p, n), *this; }
    // Uma lista de números: u32 quantidade + os valores.
    Writer& u32s(const uint32_t* v, uint32_t n) { return u32(n).raw(v, (size_t)n * 4); }
    bool ok() const { return !w_.failed; }
    mlx_writer* c() { return &w_; }
    const mlx_writer* c() const { return &w_; }

private:
    static constexpr size_t kScratch = 1 << 16;
    static uint8_t* scratch() {
        static uint8_t buffer[kScratch];
        return buffer;
    }
    mlx_writer w_;
};

class Reader {
public:
    Reader() { mlx_r_init(&r_, 0, 0); }
    Reader(const void* data, size_t size) { mlx_r_init(&r_, data, size); }
    uint8_t u8() { return mlx_r_u8(&r_); }
    uint32_t u32() { return mlx_r_u32(&r_); }
    int32_t i32() { return mlx_r_i32(&r_); }
    uint64_t u64() { return mlx_r_u64(&r_); }
    float f32() { return mlx_r_f32(&r_); }
    double f64() { return mlx_r_f64(&r_); }
    Str str() {
        uint32_t n;
        const char* s = mlx_r_str(&r_, &n);
        return Str(s, n);
    }
    const void* skip(size_t n) { return mlx_r_skip(&r_, n); }
    bool ok() const { return !r_.failed; }
    size_t left() const { return r_.size - r_.at; }

private:
    mlx_reader r_;
};

// O resultado de uma chamada: o código (>= 0: tamanho da resposta) e a resposta para ler. A resposta
// vale até a próxima chamada.
struct Answer {
    int32_t status;
    Reader in;
    bool ok() const { return status >= 0; }
};

inline Answer call(int32_t fn, const Writer& request) {
    const uint8_t* a = 0;
    const int32_t n = mlx_call_big(fn, request.c(), &a);
    return Answer{n, n >= 0 ? Reader(a, (size_t)n) : Reader()};
}
inline Answer call(int32_t fn) {
    const uint8_t* a = 0;
    const int32_t n = mlx_call_big(fn, 0, &a);
    return Answer{n, n >= 0 ? Reader(a, (size_t)n) : Reader()};
}

inline void log(int32_t level, Str text) { mobilog_log(level, text.data, (int32_t)text.size); }
inline void toast(Str text, int kind = 0) { call(MLX_FN_UI_TOAST, Writer().u8((uint8_t)kind).str(text)); }
inline bool has(Str name) { return mobilog_has(name.data, (int32_t)name.size) != 0; }
inline void subscribe(uint32_t eventMask) { call(MLX_FN_SUBSCRIBE, Writer().u32(eventMask)); }
inline uint32_t random() { return mobilog_random(); }

// ---- O mapa e a simulação --------------------------------------------------------------------

struct MapInfo {
    uint32_t nodes = 0, edges = 0, names = 0;
    double minLon = 0, minLat = 0, maxLon = 0, maxLat = 0;
    uint64_t version = 0;
};
inline bool mapInfo(MapInfo& m) {
    Answer a = call(MLX_FN_MAP_INFO);
    if (!a.ok()) return false;
    m.nodes = a.in.u32(), m.edges = a.in.u32(), m.names = a.in.u32();
    m.minLon = a.in.f64(), m.minLat = a.in.f64(), m.maxLon = a.in.f64(), m.maxLat = a.in.f64();
    a.in.str();
    m.version = a.in.u64();
    return a.in.ok();
}
// O nó (cruzamento) mais perto de um ponto; UINT32_MAX quando não há malha.
inline uint32_t nearestNode(double lon, double lat) {
    Answer a = call(MLX_FN_MAP_NEAREST_NODE, Writer().point(lon, lat));
    return a.ok() ? a.in.u32() : 0xFFFFFFFFu;
}

struct SimInfo {
    double time = 0, hour = 0, stepMs = 0;
    int32_t weekday = 0;
    uint32_t active = 0, arrived = 0, target = 0;
    float meanSpeedKmh = 0;
    bool paused = false;
};
inline bool simInfo(SimInfo& s) {
    Answer a = call(MLX_FN_SIM_INFO);
    if (!a.ok()) return false;
    s.time = a.in.f64(), s.hour = a.in.f64(), s.stepMs = a.in.f64();
    s.weekday = a.in.i32();
    s.active = a.in.u32(), s.arrived = a.in.u32(), s.target = a.in.u32();
    s.meanSpeedKmh = a.in.f32();
    a.in.u64();
    s.paused = a.in.u8() != 0;
    return a.in.ok();
}

inline void focus(double lon, double lat, float zoom = 0) { call(MLX_FN_UI_FOCUS, Writer().point(lon, lat).f32(zoom)); }
inline void setTool(Str tool, bool on) { call(MLX_FN_UI_TOOL, Writer().str(tool).u8(on ? 1 : 0)); }

// ---- Desenho -----------------------------------------------------------------------------------

// O conteúdo de uma camada: as linhas, depois os marcadores, depois as áreas (como o bloco pede).
// Usa um buffer próprio de 1 MB (ou o que você der).
class Layer {
public:
    explicit Layer(Str id, void* buffer = 0, size_t capacity = 0) : w_(buffer ? Writer(buffer, capacity) : Writer(own(), kOwn)) {
        w_.str(id);
        lines_ = mark();
    }
    Layer& line(const double* lonLat, uint32_t points, uint32_t rgba, float widthPx) {
        ++*count(lines_);
        w_.u32(rgba).f32(widthPx).u32(points).raw(lonLat, (size_t)points * 16);
        return *this;
    }
    Layer& marker(double lon, double lat, Str icon, uint32_t rgba = 0, Str label = Str()) {
        if (!markers_) markers_ = mark();
        ++*count(markers_);
        w_.point(lon, lat).str(icon).u32(rgba).str(label);
        return *this;
    }
    Layer& area(const double* lonLat, uint32_t points, uint32_t fill, uint32_t stroke, float strokePx = 2) {
        if (!markers_) markers_ = mark();
        if (!areas_) areas_ = mark();
        ++*count(areas_);
        w_.u32(fill).u32(stroke).f32(strokePx).u32(points).raw(lonLat, (size_t)points * 16);
        return *this;
    }
    int32_t send() {
        if (!markers_) markers_ = mark();
        if (!areas_) areas_ = mark();
        w_.u32(0);  // cores por trecho: nenhuma
        return w_.ok() ? call(MLX_FN_DRAW_SET_LAYER, w_).status : MLX_ERR_LIMIT;
    }

private:
    static constexpr size_t kOwn = 1 << 20;  // 1 MB: ~65 mil pontos
    static uint8_t* own() {
        static uint8_t b[kOwn];
        return b;
    }
    size_t mark() {
        const size_t at = w_.c()->size;
        w_.u32(0);
        return at;
    }
    uint32_t* count(size_t at) { return reinterpret_cast<uint32_t*>(w_.c()->data + at); }
    Writer w_;
    size_t lines_ = 0, markers_ = 0, areas_ = 0;
};

// ---- Painéis -----------------------------------------------------------------------------------

// Os widgets de um painel ou gráfico. send() o manda (MLX_EV_PANEL pede o painel; MLX_EV_CHART, o gráfico).
class Ui {
public:
    Ui() {
        mlx_w_init(&w_, buffer(), kSize);
        mlx_ui_begin(&w_);
    }
    Ui& section(uint32_t id, Str title, Str summary = Str(), bool open = true) {
        mlx_w_u8(&w_, MLX_UI_SECTION);
        mlx_w_u32(&w_, id);
        str(title).str(summary);
        return mlx_w_u8(&w_, open ? 1 : 0), *this;
    }
    Ui& endSection() { return mlx_ui_end_section(&w_), *this; }
    Ui& text(Str s, int style = 0) {
        mlx_w_u8(&w_, MLX_UI_TEXT);
        mlx_w_u8(&w_, (uint8_t)style);
        return str(s);
    }
    Ui& muted(Str s) { return text(s, 1); }
    Ui& button(uint32_t id, Str label, bool primary = false, bool disabled = false) {
        mlx_w_u8(&w_, MLX_UI_BUTTON);
        mlx_w_u32(&w_, id);
        str(label);
        mlx_w_u8(&w_, primary ? 1 : 0);
        return mlx_w_u8(&w_, disabled ? 1 : 0), *this;
    }
    Ui& toggle(uint32_t id, Str label, bool value, Str tip = Str()) {
        mlx_w_u8(&w_, MLX_UI_TOGGLE);
        mlx_w_u32(&w_, id);
        str(label);
        mlx_w_u8(&w_, value ? 1 : 0);
        return str(tip);
    }
    Ui& number(uint32_t id, Str label, double value, double min, double max, double step = 1, int decimals = 0, Str tip = Str()) {
        mlx_w_u8(&w_, MLX_UI_NUMBER);
        mlx_w_u32(&w_, id);
        str(label);
        mlx_w_f64(&w_, value), mlx_w_f64(&w_, min), mlx_w_f64(&w_, max), mlx_w_f64(&w_, step);
        mlx_w_u8(&w_, (uint8_t)decimals);
        return str(tip);
    }
    Ui& kpi(Str label, Str value, int tone = 0) {
        mlx_w_u8(&w_, MLX_UI_KPI);
        str(label).str(value);
        return mlx_w_u8(&w_, (uint8_t)tone), *this;
    }
    Ui& lineChart(uint32_t id, Str title, Str series, const float* xs, const float* ys, uint32_t n, uint32_t rgba = 0) {
        mlx_w_u8(&w_, MLX_UI_LINE_CHART);
        mlx_w_u32(&w_, id);
        str(title);
        mlx_w_u32(&w_, 1);
        str(series);
        mlx_w_u32(&w_, rgba);
        mlx_w_u32(&w_, n);
        mlx_w_raw(&w_, xs, (size_t)n * 4);
        return mlx_w_raw(&w_, ys, (size_t)n * 4), *this;
    }
    Ui& progress(float fraction, Str label) {
        mlx_w_u8(&w_, MLX_UI_PROGRESS);
        mlx_w_f32(&w_, fraction);
        return str(label);
    }
    Ui& separator() { return mlx_ui_separator(&w_), *this; }
    Ui& sameLine() { return mlx_ui_same_line(&w_), *this; }
    int32_t send(Str panel) { return submit(MLX_FN_UI_PANEL, panel); }
    int32_t sendChart(Str chart) { return submit(MLX_FN_UI_CHART, chart); }

private:
    static constexpr size_t kSize = 1 << 16;
    static uint8_t* buffer() {
        static uint8_t b[kSize];
        return b;
    }
    Ui& str(Str s) { return mlx_w_strn(&w_, s.data, s.size), *this; }
    int32_t submit(int32_t fn, Str id) {
        if (w_.failed) return MLX_ERR_LIMIT;
        Writer r;
        r.str(id).u32((uint32_t)w_.size).raw(w_.data, w_.size);
        return r.ok() ? call(fn, r).status : MLX_ERR_LIMIT;
    }
    mlx_writer w_;
};

// Uma ação do usuário num painel (MLX_EV_UI).
struct UiAction {
    uint32_t id;
    uint8_t kind;  // 1 clique, 2 chave, 3 número, 4 escolha, 5 texto
    double value;
    Str text;
};

// ---- A extensão --------------------------------------------------------------------------------

template <class T>
struct Extension {
    int32_t init() { return 0; }
    void shutdown() {}
    void frame(double) {}
    void panel(Str) {}
    void chart(Str) {}
    void ui(Str, const UiAction&) {}
    bool tool(Str, uint8_t, double, double) { return false; }
    bool tap(double, double) { return false; }
    void taskDone(uint32_t) {}
    void vehiclesArrived(const uint32_t*, uint32_t) {}
    void network(uint64_t) {}
    void scenario() {}
    void experimentDone(uint32_t) {}
    int32_t task(int32_t, Reader&) { return 1; }
    void solve(Str, Reader&) {}

    // Liga os eventos às funções da classe (chamado pelas exportações de MLX_EXTENSION).
    static int32_t dispatch(T& self, int32_t kind, const void* data, int32_t size) {
        Reader in(data, (size_t)size);
        switch (kind) {
        case MLX_EV_FRAME: self.frame(in.f64()); return 0;
        case MLX_EV_PANEL: self.panel(in.str()); return 0;
        case MLX_EV_CHART: self.chart(in.str()); return 0;
        case MLX_EV_UI: {
            const Str panel = in.str();
            for (uint32_t i = 0, n = in.u32(); i < n && in.ok(); ++i) {
                UiAction a;
                a.id = in.u32();
                a.kind = in.u8();
                a.value = in.f64();
                a.text = in.str();
                self.ui(panel, a);
            }
            return 0;
        }
        case MLX_EV_TOOL: {
            const Str id = in.str();
            const uint8_t action = in.u8();
            const double lon = in.f64(), lat = in.f64();
            return self.tool(id, action, lon, lat) ? 1 : 0;
        }
        case MLX_EV_TAP: {
            const double lon = in.f64(), lat = in.f64();
            return self.tap(lon, lat) ? 1 : 0;
        }
        case MLX_EV_TASK_DONE: self.taskDone(in.u32()); return 0;
        case MLX_EV_VEHICLE_ARRIVED: {
            const uint32_t n = in.u32();
            const uint32_t* v = static_cast<const uint32_t*>(in.skip((size_t)n * 4));
            if (v) self.vehiclesArrived(v, n);
            return 0;
        }
        case MLX_EV_NETWORK: self.network(in.u64()); return 0;
        case MLX_EV_SCENARIO: self.scenario(); return 0;
        case MLX_EV_EXPERIMENT_DONE: self.experimentDone(in.u32()); return 0;
        default: return 0;
        }
    }
    static int32_t runTask(T& self, int32_t kind, const void* data, int32_t size) {
        Reader in(data, (size_t)size);
        if (kind == MLX_TASK_SOLVE_VRP) {
            const Str id = in.str();
            self.solve(id, in);
            return 0;
        }
        return self.task(kind, in);
    }
};

}  // namespace mlx

// As exportações do módulo para a classe da extensão (num arquivo só).
#define MLX_EXTENSION(T)                                                                                                         \
    static T mlx_extension_;                                                                                                     \
    extern "C" {                                                                                                                 \
    MLX_EXPORT("mlx_api_version") int32_t mlx_api_version_(void) { return MLX_API_VERSION; }                                     \
    MLX_EXPORT("mlx_init") int32_t mlx_init_(void) { return mlx_extension_.init(); }                                             \
    MLX_EXPORT("mlx_shutdown") void mlx_shutdown_(void) { mlx_extension_.shutdown(); }                                           \
    MLX_EXPORT("mlx_event") int32_t mlx_event_(int32_t kind, const void* data, int32_t size) {                                   \
        return mlx::Extension<T>::dispatch(mlx_extension_, kind, data, size);                                                    \
    }                                                                                                                            \
    MLX_EXPORT("mlx_task") int32_t mlx_task_(int32_t kind, const void* data, int32_t size) {                                     \
        return mlx::Extension<T>::runTask(mlx_extension_, kind, data, size);                                                     \
    }                                                                                                                            \
    }
