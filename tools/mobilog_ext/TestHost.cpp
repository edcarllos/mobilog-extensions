// test: runs an extension without the app, in WAMR's interpreter, the way the app starts it and
// talks to it: _initialize, mlx_api_version, mlx_init, then frames with the events of what it
// contributes (panels, charts on screen; the network and the scenario once; a tap), then
// mlx_shutdown. There is no map or simulation here: those functions answer "unavailable", as the
// app does before an area is open, and the extension must cope. Calls to a function whose
// permission the manifest does not ask for are errors (the app would refuse them too).
//
// A script (--script roteiro.json) replaces the default run:
//   [{"frames": 30}, {"tap": [-39.27, -14.78]}, {"tool": "medir", "action": "tap", "at": [-39.27, -14.78]},
//    {"panel": "principal"}, {"click": 1, "in": "principal"}, {"chart": "historico"}, {"network": 2},
//    {"scenario": true}]
// The folder's test/roteiro.json is used when there is one.
#include "Tool.h"

#include "mlx/Abi.h"
#include "mlx/Api.h"
#include "mlx/Metering.h"
#include "mlx/Wire.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <random>

#if MLX_TEST_HOST
#include <wasm_export.h>
#endif

namespace mlxtool {

using namespace mobilog::ext;

#if !MLX_TEST_HOST

int cmdTest(const Args&) {
    std::fprintf(stderr, "%s\n", tr("este mobilog_ext foi compilado sem o hospedeiro de teste (MLX_TEST_HOST=OFF)",
                                    "this mobilog_ext was built without the test host (MLX_TEST_HOST=OFF)"));
    return 1;
}

#else

namespace {

constexpr uint32_t kStackBytes = 256u << 10;
constexpr double kHookBudgetMs = 2000;  // a hook that runs longer is stopped (the app: "travou")
constexpr uint32_t kMaxLogBytes = 4096, kMaxKeyBytes = 256, kMaxValueBytes = 16u << 20, kMaxRequestBytes = 64u << 20;
constexpr int kWasiOk = 0, kWasiBadf = 8, kWasiSpipe = 70;

double nowMs() {
    using namespace std::chrono;
    return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}

struct Stats {
    std::map<std::string, int> calls, denied, unavailable;
    std::map<std::string, std::pair<int, double>> hooks;  // name -> count, max ms
    std::vector<std::string> errors;
    int logs = 0;
    uint32_t subscribed = 0;
};

struct Host {
    Manifest manifest;
    Stats stats;
    bool verbose = false;
    std::map<std::string, Bytes> storage;
    std::mt19937 rng{12345};
    Bytes lastAnswer;
    std::string trap;
    double callStart = 0;

    wasm_module_t module = nullptr;
    wasm_module_inst_t inst = nullptr;
    wasm_exec_env_t exec = nullptr;
    Bytes bytes;

    ~Host() {
        if (exec) wasm_runtime_destroy_exec_env(exec);
        if (inst) wasm_runtime_deinstantiate(inst);
        if (module) wasm_runtime_unload(module);
    }

    void raise(std::string m) {
        if (trap.empty()) trap = std::move(m);
    }
    bool read(uint32_t off, uint32_t n, void* dst) {
        if (!wasm_runtime_validate_app_addr(inst, off, n)) return false;
        std::memcpy(dst, wasm_runtime_addr_app_to_native(inst, off), n);
        return true;
    }
    bool write(uint32_t off, const void* src, uint32_t n) {
        if (!wasm_runtime_validate_app_addr(inst, off, n)) return false;
        std::memcpy(wasm_runtime_addr_app_to_native(inst, off), src, n);
        return true;
    }
    bool readBytes(double ptr, double len, uint32_t max, std::string& out) {
        const uint32_t p = uint32_t(int64_t(ptr)), n = uint32_t(int64_t(len));
        if (n > max) return raise("bloco grande demais passado ao Mobilog"), false;
        out.resize(n);
        if (n && !read(p, n, out.data())) return raise("acesso fora da memória da extensão"), false;
        return true;
    }
    double answer(double out, double cap, const Bytes& data) {
        const uint32_t n = uint32_t(std::min<size_t>(data.size(), uint32_t(int64_t(cap))));
        if (n && !write(uint32_t(int64_t(out)), data.data(), n)) return raise("acesso fora da memória da extensão"), NAN;
        return double(data.size());
    }

    // An API function: what the app would answer with no area open.
    int32_t call(uint32_t fn, const std::string& in, Bytes& out) {
        const ApiFunctionInfo* f = apiFunction(fn);
        if (!f) return kErrUnknown;
        const std::string name = f->name;
        ++stats.calls[name];
        if (f->permission && !(manifest.permissions & f->permission)) {
            if (++stats.denied[name] == 1) {
                const PermissionInfo* p = permissionInfo(Permission(f->permission));
                stats.errors.push_back(name + tr(" sem a permissão \"", " without the permission \"") + (p ? p->key : "?") +
                                       tr("\" no manifesto (o app recusa)", "\" in the manifest (the app refuses it)"));
            }
            return kErrDenied;
        }
        if (!(f->where & kInHooks)) {
            ++stats.unavailable[name];
            return kErrUnavailable;  // tasks/solver only: not from a hook
        }
        switch (fn) {
        case kFnEcho: out.assign(in.begin(), in.end()); return 0;
        case kFnSubscribe:
            if (in.size() >= 4) std::memcpy(&stats.subscribed, in.data(), 4);
            return 0;
        default: break;
        }
        // Drawing and the interface: taken (nothing to show here). The map, the simulation, the
        // scenario, vehicles, tasks: not without an area.
        if (name.rfind("ui.", 0) == 0 || name.rfind("draw.", 0) == 0) return 0;
        ++stats.unavailable[name];
        return kErrUnavailable;
    }

    double import(int fn, const double* a) {
        const AbiFunction& imp = kHostImports[fn];
        const std::string_view n = imp.name;
        std::string s, k;
        if (n == "log") {
            if (!readBytes(a[1], a[2], kMaxLogBytes, s)) return NAN;
            ++stats.logs;
            static const char* const levels[] = {"debug", "info", "aviso", "erro"};
            std::printf("  [%s] %s\n", levels[std::clamp(int(a[0]), 0, 3)], s.c_str());
            return 0;
        }
        if (n == "time_ms") return nowMs();
        if (n == "random") return double(int32_t(rng()));
        if (n == "storage_get") {
            if (!readBytes(a[0], a[1], kMaxKeyBytes, k)) return NAN;
            const auto it = storage.find(k);
            return it == storage.end() ? -1 : answer(a[2], a[3], it->second);
        }
        if (n == "storage_set") {
            if (!readBytes(a[0], a[1], kMaxKeyBytes, k) || !readBytes(a[2], a[3], kMaxValueBytes, s)) return NAN;
            storage[k] = Bytes(s.begin(), s.end());
            return 0;
        }
        if (n == "progress") return 0;
        if (n == "cancelled") return 0;
        if (n == "check") {
            if (nowMs() - callStart > kHookBudgetMs) {
                raise(tr("travou: passou de 2 s sem responder", "hung: more than 2 s without answering"));
                return 0;
            }
            return kMeterSlice;
        }
        if (n == "has") {
            if (!readBytes(a[0], a[1], kMaxKeyBytes, k)) return NAN;
            return apiFunction(k) ? 1 : 0;
        }
        if (n == "call") {
            if (!readBytes(a[1], a[2], kMaxRequestBytes, s)) return NAN;
            lastAnswer.clear();
            const int32_t status = call(uint32_t(int64_t(a[0])), s, lastAnswer);
            if (status < 0) return status;
            return answer(a[3], a[4], lastAnswer);
        }
        if (n == "take") return answer(a[0], a[1], lastAnswer);
        // WASI: printing goes to the log; no files, arguments or environment.
        if (n == "fd_write") {
            const uint32_t fd = uint32_t(a[0]), iovs = uint32_t(a[1]), count = uint32_t(a[2]);
            if ((fd != 1 && fd != 2) || count > 64) return kWasiBadf;
            uint32_t total = 0;
            for (uint32_t i = 0; i < count; ++i) {
                uint32_t iov[2];
                if (!read(iovs + 8 * i, 8, iov)) return raise("acesso fora da memória da extensão"), NAN;
                std::string piece;
                if (!readBytes(iov[0], iov[1], kMaxLogBytes, piece)) return NAN;
                s += piece;
                total += iov[1];
            }
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
            if (!s.empty()) std::printf("  [%s] %s\n", fd == 2 ? "stderr" : "stdout", s.c_str());
            if (!write(uint32_t(a[3]), &total, 4)) return raise("acesso fora da memória da extensão"), NAN;
            return kWasiOk;
        }
        if (n == "fd_close") return uint32_t(a[0]) <= 2 ? kWasiOk : kWasiBadf;
        if (n == "fd_seek") return kWasiSpipe;
        if (n == "fd_fdstat_get") {
            uint8_t stat[24] = {};
            stat[0] = 2;
            return write(uint32_t(a[1]), stat, sizeof stat) ? kWasiOk : (raise("acesso fora da memória da extensão"), NAN);
        }
        if (n == "proc_exit") return raise("a extensão encerrou (código " + std::to_string(int(a[0])) + ")"), NAN;
        if (n == "clock_time_get") {
            const uint64_t ns = uint64_t(nowMs() * 1e6);
            return write(uint32_t(a[2]), &ns, 8) ? kWasiOk : (raise("acesso fora da memória da extensão"), NAN);
        }
        if (n == "random_get") {
            const uint32_t len = uint32_t(a[1]);
            if (len > (1u << 20)) return kWasiBadf;
            Bytes b(len);
            for (auto& x : b) x = uint8_t(rng());
            return write(uint32_t(a[0]), b.data(), len) ? kWasiOk : (raise("acesso fora da memória da extensão"), NAN);
        }
        if (n == "environ_sizes_get" || n == "args_sizes_get") {
            const uint32_t zero = 0;
            return write(uint32_t(a[0]), &zero, 4) && write(uint32_t(a[1]), &zero, 4) ? kWasiOk : (raise("acesso fora da memória"), NAN);
        }
        return kWasiOk;  // environ_get, args_get
    }

    bool hook(const char* name, std::initializer_list<int32_t> args, int32_t* result) {
        wasm_function_inst_t f = wasm_runtime_lookup_function(inst, name);
        if (!f) return true;
        uint32_t argv[4] = {};
        uint32_t argc = 0;
        for (int32_t v : args) argv[argc++] = uint32_t(v);
        trap.clear();
        callStart = nowMs();
        const bool ok = wasm_runtime_call_wasm(exec, f, argc, argv);
        const double ms = nowMs() - callStart;
        auto& h = stats.hooks[name];
        ++h.first;
        h.second = std::max(h.second, ms);
        if (!ok) {
            std::string e = trap.empty() ? std::string(wasm_runtime_get_exception(inst) ? wasm_runtime_get_exception(inst) : "?") : trap;
            if (e.rfind("Exception: ", 0) == 0) e = e.substr(11);
            wasm_runtime_clear_exception(inst);
            stats.errors.push_back(std::string(name) + ": " + e);
            return false;
        }
        if (result) *result = int32_t(argv[0]);
        return true;
    }

    bool event(int32_t kind, const Bytes& data, int32_t* answerOut = nullptr) {
        if (!wasm_runtime_lookup_function(inst, "mlx_event")) return true;
        int32_t ptr = 0;
        if (!data.empty()) {
            if (!hook("mlx_alloc", {int32_t(data.size())}, &ptr)) return false;
            if (ptr <= 0 || !write(uint32_t(ptr), data.data(), uint32_t(data.size()))) {
                stats.errors.push_back(tr("mlx_alloc não reservou memória para um evento", "mlx_alloc did not reserve memory for an event"));
                return false;
            }
        }
        int32_t rc = 0;
        const bool ok = hook("mlx_event", {kind, ptr, int32_t(data.size())}, &rc);
        if (ok && answerOut) *answerOut = rc;
        if (ok && ptr > 0) hook("mlx_free", {ptr}, nullptr);
        return ok;
    }
};

thread_local Host* gHost = nullptr;

// "iii:" -> "(iii)", ":d" -> "()F"
std::string wamrSignature(const char* sig) {
    std::string out = "(";
    const auto letter = [](char c) { return c == 'd' ? 'F' : c; };
    const char* p = sig;
    for (; *p && *p != ':'; ++p) out += letter(*p);
    out += ')';
    if (*p == ':') ++p;
    for (; *p; ++p) out += letter(*p);
    return out;
}

void rawImport(wasm_exec_env_t env, uint64_t* args) {
    const int fn = int(reinterpret_cast<intptr_t>(wasm_runtime_get_function_attachment(env)));
    Host* h = gHost;
    const char* sig = kHostImports[fn].signature;
    double a[5] = {};
    int k = 0;
    const char* p = sig;
    for (; *p && *p != ':' && k < 5; ++p, ++k) {
        switch (*p) {
        case 'i': {
            int32_t v;
            std::memcpy(&v, &args[k], 4);
            a[k] = v;
            break;
        }
        case 'I': {
            int64_t v;
            std::memcpy(&v, &args[k], 8);
            a[k] = double(v);
            break;
        }
        case 'f': {
            float v;
            std::memcpy(&v, &args[k], 4);
            a[k] = v;
            break;
        }
        default: std::memcpy(&a[k], &args[k], 8); break;
        }
    }
    const double r = h->import(fn, a);
    if (!h->trap.empty()) {
        wasm_runtime_set_exception(h->inst, h->trap.c_str());
        return;
    }
    const char ret = *p == ':' ? p[1] : 0;
    if (ret == 'i') {
        const int32_t v = int32_t(std::isfinite(r) ? r : 0);
        std::memcpy(&args[0], &v, 4);
    } else if (ret == 'd') {
        std::memcpy(&args[0], &r, 8);
    } else if (ret == 'I') {
        const int64_t v = int64_t(r);
        std::memcpy(&args[0], &v, 8);
    }
}

bool initRuntime() {
    static bool ok = false, done = false;
    static std::vector<std::string> sigs;
    static std::vector<NativeSymbol> mob, wasi;
    if (done) return ok;
    done = true;
    RuntimeInitArgs args;
    std::memset(&args, 0, sizeof args);
    args.mem_alloc_type = Alloc_With_System_Allocator;
    if (!wasm_runtime_full_init(&args)) return false;
    const size_t n = sizeof(kHostImports) / sizeof(kHostImports[0]);
    sigs.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        sigs.push_back(wamrSignature(kHostImports[i].signature));
        NativeSymbol s{kHostImports[i].name, reinterpret_cast<void*>(&rawImport), sigs.back().c_str(), reinterpret_cast<void*>(i)};
        (std::string_view(kHostImports[i].module) == "mobilog" ? mob : wasi).push_back(s);
    }
    ok = wasm_runtime_register_natives_raw("mobilog", mob.data(), uint32_t(mob.size())) &&
         wasm_runtime_register_natives_raw("wasi_snapshot_preview1", wasi.data(), uint32_t(wasi.size()));
    return ok;
}

Bytes strBlock(const std::string& s) {
    WireWriter w;
    w.str(s);
    return w.bytes();
}

}  // namespace

int cmdTest(const Args& a) {
    if (a.positional.size() > 1) return usage();
    const fs::path target = a.positional.empty() ? fs::current_path() : fs::path(a.positional[0]);
    std::error_code ec;
    Bytes mlx;
    if (fs::is_regular_file(target, ec)) {
        if (!readFile(target, mlx)) return 1;
    } else {
        Args quiet = a;
        quiet.quiet = true;
        Built b;
        if (!buildFolder(quiet, target, b) || !readFile(b.mlx, mlx)) return 1;
    }
    ZipReader zip;
    const PackageCheck c = checkPackage(mlx, {"", true}, zip);
    if (report(c, target.filename().string())) return 1;
    if (c.manifest.native()) {
        std::fprintf(stderr, "%s\n", tr("extensões nativas não rodam aqui", "native extensions do not run here"));
        return 1;
    }
    Bytes module, metered;
    std::string err;
    if (!zip.read(c.manifest.entry, module, &err) || !meterModule(module, metered, &err)) {
        std::fprintf(stderr, "%s: %s\n", tr("erro", "error"), err.c_str());
        return 1;
    }
    if (!initRuntime()) {
        std::fprintf(stderr, "%s\n", tr("o interpretador não iniciou", "the interpreter did not start"));
        return 1;
    }
    Host h;
    gHost = &h;
    h.manifest = c.manifest;
    h.bytes = std::move(metered);
    char errbuf[256] = {};
    h.module = wasm_runtime_load(h.bytes.data(), uint32_t(h.bytes.size()), errbuf, sizeof errbuf);
    if (!h.module) {
        std::fprintf(stderr, "%s: %s\n", tr("módulo recusado", "module refused"), errbuf);
        return 1;
    }
    h.inst = wasm_runtime_instantiate(h.module, kStackBytes, 0, errbuf, sizeof errbuf);
    if (!h.inst || !(h.exec = wasm_runtime_create_exec_env(h.inst, kStackBytes))) {
        std::fprintf(stderr, "%s: %s\n", tr("não foi possível iniciar o módulo", "could not start the module"), errbuf);
        return 1;
    }
    const Manifest& m = c.manifest;
    std::printf("%s %s (%s) · %s\n", m.name.c_str(), m.version.c_str(), m.id.c_str(),
                tr("rodando sem o app (sem mapa nem simulação)", "running without the app (no map or simulation)"));
    // The script: --script, or the folder's test/roteiro.json.
    fs::path script = a.script;
    if (script.empty() && fs::is_directory(target, ec) && fs::is_regular_file(target / "test" / "roteiro.json", ec))
        script = target / "test" / "roteiro.json";
    if (!script.empty() && !a.quiet) std::printf("%s %s\n", tr("Roteiro:", "Script:"), script.string().c_str());
    const double t0 = nowMs();
    int32_t api = 0, rc = -1;
    bool ok = h.hook("_initialize", {}, nullptr) && h.hook("mlx_api_version", {}, &api);
    if (ok && (api < 1 || api > kApiVersion)) {
        h.stats.errors.push_back("mlx_api_version: " + std::to_string(api) + tr(" (este app tem a API ", " (this app has API ") +
                                 std::to_string(kApiVersion) + ")");
        ok = false;
    }
    ok = ok && h.hook("mlx_init", {}, &rc);
    if (ok && rc != 0) {
        h.stats.errors.push_back(tr("mlx_init devolveu ", "mlx_init returned ") + std::to_string(rc) + tr(" (o app não liga a extensão)", " (the app does not start it)"));
        ok = false;
    }
    std::vector<std::string> panels, charts, tools;
    for (const Contribution& con : m.contributes) {
        if (con.kind == ContributionKind::Panel) panels.push_back(con.id);
        if (con.kind == ContributionKind::Chart) charts.push_back(con.id);
        if (con.kind == ContributionKind::Tool) tools.push_back(con.id);
    }
    const auto frame = [&](int i) {
        bool good = true;
        if (h.stats.subscribed & (1u << kEvFrame)) {
            WireWriter w;
            w.f64(1.0 / 60);
            good = good && h.event(kEvFrame, w.bytes());
        }
        for (const auto& p : panels) good = good && h.event(kEvPanel, strBlock(p));
        for (const auto& ch : charts) good = good && h.event(kEvChart, strBlock(ch));
        (void)i;
        return good;
    };
    if (ok && script.empty()) {
        WireWriter net;
        net.u64(1);
        ok = h.event(kEvNetwork, net.bytes()) && h.event(kEvScenario, {});
        for (int i = 0; ok && i < a.frames; ++i) {
            ok = frame(i);
            if (ok && i == a.frames / 2) {
                WireWriter tap;
                tap.lonLat(-39.2780, -14.7862);
                int32_t taken = 0;
                ok = h.event(kEvTap, tap.bytes(), &taken);
                for (const auto& t : tools) {
                    WireWriter w;
                    w.str(t).u8(kToolTap).lonLat(-39.2780, -14.7862);
                    ok = ok && h.event(kEvTool, w.bytes());
                }
            }
        }
    } else if (ok) {
        std::string text;
        if (!readText(script, text)) {
            std::fprintf(stderr, "%s %s\n", tr("erro: não foi possível ler", "error: could not read"), script.string().c_str());
            return 1;
        }
        const nlohmann::json steps = nlohmann::json::parse(text, nullptr, false);
        if (!steps.is_array()) {
            std::fprintf(stderr, "%s\n", tr("--script: deve ser uma lista JSON", "--script: must be a JSON list"));
            return 1;
        }
        int i = 0;
        for (const auto& step : steps) {
            if (!ok) break;
            if (!step.is_object()) continue;
            if (step.contains("frames"))
                for (int n = std::max(0, step.value("frames", 0)); ok && n > 0; --n) ok = frame(i++);
            const auto at = [&](const char* key) {
                const auto& v = step[key];
                return std::pair<double, double>(v.is_array() && v.size() == 2 ? v[0].get<double>() : 0.0,
                                                 v.is_array() && v.size() == 2 ? v[1].get<double>() : 0.0);
            };
            if (step.contains("tap")) {
                const auto [lon, lat] = at("tap");
                WireWriter w;
                w.lonLat(lon, lat);
                ok = h.event(kEvTap, w.bytes());
            }
            if (step.contains("tool")) {
                const auto [lon, lat] = at("at");
                static const std::map<std::string, ApiToolAction> actions = {
                    {"tap", kToolTap}, {"grab", kToolGrab}, {"begin", kToolDragBegin}, {"move", kToolDragMove},
                    {"end", kToolDragEnd}, {"cancel", kToolDragCancel}, {"off", kToolOff}};
                const auto act = actions.find(step.value("action", "tap"));
                WireWriter w;
                w.str(step.value("tool", "")).u8(act == actions.end() ? kToolTap : act->second).lonLat(lon, lat);
                ok = h.event(kEvTool, w.bytes());
            }
            if (step.contains("panel")) ok = ok && h.event(kEvPanel, strBlock(step.value("panel", "")));
            if (step.contains("click")) {  // {"click": 1, "in": "principal"}: a button of a panel
                WireWriter w;
                w.str(step.value("in", panels.empty() ? std::string() : panels.front())).u32(1);
                w.u32(step.value("click", 0u)).u8(1).f64(0).str("");
                ok = ok && h.event(kEvUi, w.bytes());
            }
            if (step.contains("chart")) ok = ok && h.event(kEvChart, strBlock(step.value("chart", "")));
            if (step.contains("network")) {
                WireWriter w;
                w.u64(step.value("network", uint64_t(1)));
                ok = ok && h.event(kEvNetwork, w.bytes());
            }
            if (step.contains("scenario")) ok = ok && h.event(kEvScenario, {});
        }
    }
    if (ok) h.hook("mlx_shutdown", {}, nullptr);
    const double total = nowMs() - t0;
    gHost = nullptr;

    // The report.
    std::printf("\n%s\n", tr("Ganchos (chamadas · pior tempo, interpretado):", "Hooks (calls · worst time, interpreted):"));
    for (const auto& [name, v] : h.stats.hooks)
        if (name != "mlx_alloc" && name != "mlx_free") std::printf("  %-16s %6d · %.2f ms\n", name.c_str(), v.first, v.second);
    if (!h.stats.calls.empty()) std::printf("%s\n", tr("Funções do Mobilog chamadas:", "Mobilog functions called:"));
    for (const auto& [name, n] : h.stats.calls) {
        const int un = h.stats.unavailable.count(name) ? h.stats.unavailable.at(name) : 0;
        std::printf("  %-22s %6d%s\n", name.c_str(), n, un ? tr("  (indisponível aqui: sem mapa/simulação)", "  (unavailable here: no map/simulation)") : "");
    }
    wasm_memory_inst_t mem = wasm_runtime_get_default_memory(h.inst);
    const uint64_t memBytes = mem ? wasm_memory_get_cur_page_count(mem) * wasm_memory_get_bytes_per_page(mem) : 0;
    std::printf(tr("Memória: %.1f MB de %u MB · %d linhas de log · %.1f ms no total\n", "Memory: %.1f MB of %u MB · %d log lines · %.1f ms in total\n"),
                double(memBytes) / 1048576.0, m.memoryMB, h.stats.logs, total);
    std::fflush(stdout);
    for (const auto& e : h.stats.errors) std::fprintf(stderr, "%s: %s\n", tr("erro", "error"), e.c_str());
    if (!h.stats.errors.empty()) {
        std::fprintf(stderr, "%s\n", tr("FALHOU", "FAILED"));
        return 1;
    }
    std::printf("OK\n");
    return 0;
}

#endif

}  // namespace mlxtool
