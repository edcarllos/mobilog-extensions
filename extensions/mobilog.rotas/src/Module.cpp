// The package "Rotas e entregas" as a WebAssembly module (built with wasi-sdk: build.sh). It declares
// the solvers (the same names and parameters the app knew), and for each run the app makes it does
// one search of one copy: kTaskSolveVrp reads the instance, the plan to start from and the
// parameters (solver.params: the seed, the time, this copy) and prepares the search; each
// kTaskSolveStep works one slice and offers what improved (solver.offer: the app checks it). The
// app runs the copies in parallel and keeps the best.
#include "Search.h"
#include "mobilog/vrp/VrpWire.h"

#include "mobilog_ext.h"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace mobilog::ext {
// WireReader::str checks the text (the app's ext/pkg/Bytes.cpp is not in the module).
bool validUtf8(std::string_view s) {
    size_t i = 0;
    while (i < s.size()) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        const size_t n = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
        if (!n || i + n > s.size()) return false;
        for (size_t k = 1; k < n; ++k)
            if ((static_cast<unsigned char>(s[i + k]) & 0xC0) != 0x80) return false;
        i += n;
    }
    return true;
}
}  // namespace mobilog::ext

namespace mobilog::opt {

// ---- The module's side of SolverControl: the app keeps the incumbent ----------------------------

bool SolverControl::offer(const VrpSolution& s) {
    ext::WireWriter w;
    encodeRoutes(s.routes, w);
    const ext::Bytes& b = w.bytes();
    uint8_t answer[16] = {};
    const int32_t n = mobilog_call(MLX_FN_SOLVER_OFFER, b.data(), int32_t(b.size()), answer, sizeof answer);
    if (n < 10) return false;
    double cost = 0;
    std::memcpy(&cost, answer, 8);
    const bool improved = answer[9] != 0;
    if (improved || !hasBest_) {
        best_ = s;
        best_.cost = cost;
        best_.feasible = answer[8] != 0;
        hasBest_ = true;
        bestCost_ = cost;
        history_.push_back({elapsed(), cost});
    }
    return improved;
}

bool SolverControl::best(VrpSolution& out) const {
    if (hasBest_) out = best_;
    return hasBest_;
}

std::vector<std::pair<double, double>> SolverControl::history() const { return history_; }

}  // namespace mobilog::opt

namespace {

using namespace mobilog;
using namespace mobilog::opt;

// The run of this instance (one search).
std::unique_ptr<VrpInstance> gInst;
std::unique_ptr<SolverControl> gCtl;
std::unique_ptr<SlicedSearch> gSearch;
uint64_t gReported = 0;

void reportProgress() {
    const uint64_t now = gCtl->iterations.load();
    const uint64_t delta = now - gReported;
    if (!delta) return;
    gReported = now;
    mobilog_call(MLX_FN_SOLVER_PROGRESS, &delta, 8, nullptr, 0);
}

// The run's parameters: seed, time left, the algorithm's values and the app's own keys.
bool readParams(uint64_t& seed, double& timeLeft, std::vector<std::pair<std::string, float>>& params, uint32_t& copy,
                uint32_t& copies, bool& sliced) {
    std::vector<uint8_t> buf(64 * 1024);
    int32_t n = mobilog_call(MLX_FN_SOLVER_PARAMS, nullptr, 0, buf.data(), int32_t(buf.size()));
    if (n > int32_t(buf.size())) {
        buf.resize(size_t(n));
        n = mobilog_take(buf.data(), n);
    }
    if (n < 0) return false;
    ext::WireReader r(buf.data(), size_t(n));
    seed = r.u64();
    const double limit = r.f64(), elapsed = r.f64();
    timeLeft = std::max(0.0, limit - elapsed);
    const uint32_t count = r.count(8, 256);
    copy = 0, copies = 1, sliced = false;
    for (uint32_t i = 0; i < count && r.ok(); ++i) {
        std::string key = r.str(64);
        const float v = r.f32();
        if (key == "_copy") copy = uint32_t(v);
        else if (key == "_copies") copies = std::max(1u, uint32_t(v));
        else if (key == "_sliced") sliced = v != 0;
        else params.push_back({std::move(key), v});
    }
    return r.ok();
}

int32_t startRun(const void* data, int32_t size) {
    ext::WireReader r(static_cast<const uint8_t*>(data), size_t(size));
    const std::string id = r.str(64);
    auto inst = std::make_unique<VrpInstance>();
    if (!decodeVrpInstance(r, *inst, false)) return 2;
    std::vector<std::vector<uint32_t>> warm;
    bool hasWarm = false;
    if (!decodeWarmStart(r, *inst, warm, hasWarm)) return 2;
    uint64_t seed = 1;
    double timeLeft = 0;
    std::vector<std::pair<std::string, float>> params;
    uint32_t copy = 0, copies = 1;
    bool sliced = false;
    if (!readParams(seed, timeLeft, params, copy, copies, sliced)) return 3;
    gInst = std::move(inst);
    gCtl = std::make_unique<SolverControl>(timeLeft, seed);
    gCtl->params = std::move(params);
    if (hasWarm) {
        auto initial = std::make_shared<VrpSolution>();
        initial->routes = std::move(warm);
        evaluate(*gInst, *initial);
        gCtl->initial = std::move(initial);
    }
    gReported = 0;
    const uint64_t none = 0;
    mobilog_call(MLX_FN_SOLVER_PROGRESS, &none, 8, nullptr, 0);  // it counts its own iterations: an offer isn't one
    gSearch = makeSearch(id, *gInst, *gCtl, copy, copies);
    if (!gSearch) return 4;
    if (sliced) return 1;  // the app asks for the slices
    // An app without slices: the whole run here.
    const int slice = int(gCtl->param("sliceMs", 40.0f));
    while (!gCtl->shouldStop() && gSearch->slice(slice, *gCtl)) reportProgress();
    reportProgress();
    return 0;
}

int32_t stepRun(int32_t ms) {
    if (!gSearch) return 0;
    const bool more = gSearch->slice(ms > 0 ? ms : 40, *gCtl);
    reportProgress();
    return more && !gCtl->shouldStop() ? 1 : 0;
}

}  // namespace

extern "C" {

MLX_EXPORT("mlx_api_version") int32_t mlx_api_version_(void) { return MLX_API_VERSION; }
MLX_EXPORT("mlx_alloc") void* mlx_alloc(int32_t size) { return std::malloc(size > 0 ? size_t(size) : 1); }
MLX_EXPORT("mlx_free") void mlx_free(void* p) { std::free(p); }

MLX_EXPORT("mlx_init") int32_t mlx_init_(void) {
    // The solvers, as the app listed them before they became a package: names and parameters the same
    // (the values the user set are kept by those names).
    for (const SolverDecl& d : routingSolvers()) {
        ext::WireWriter w;
        w.str(d.id).u32(d.info.features | (d.parallel ? uint32_t(MLX_VRP_PARALLEL) : 0u)).u32(uint32_t(d.info.params.size()));
        for (const SolverParam& p : d.info.params) {
            int decimals = 0;
            if (const size_t dot = p.format.find('.'); dot != std::string::npos && dot + 1 < p.format.size())
                decimals = p.format[dot + 1] - '0';
            w.str(p.key).str(p.label).str(p.tip).f32(p.def).f32(p.min).f32(p.max).f32(p.step).u8(uint8_t(decimals)).u8(p.advanced ? 1 : 0);
        }
        const ext::Bytes& b = w.bytes();
        if (mobilog_call(MLX_FN_SOLVER_DECLARE, b.data(), int32_t(b.size()), nullptr, 0) < 0) {
            const std::string msg = std::string("resolvedor recusado: ") + d.id;
            mobilog_log(2, msg.c_str(), int32_t(msg.size()));
        }
    }
    return 0;
}

MLX_EXPORT("mlx_shutdown") void mlx_shutdown_(void) {}
MLX_EXPORT("mlx_event") int32_t mlx_event_(int32_t, const void*, int32_t) { return 0; }

MLX_EXPORT("mlx_task") int32_t mlx_task_(int32_t kind, const void* data, int32_t size) {
    if (kind == MLX_TASK_SOLVE_VRP) return startRun(data, size);
    if (kind == MLX_TASK_SOLVE_STEP) return stepRun(size);
    return 1;
}

}  // extern "C"
