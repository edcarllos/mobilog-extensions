#pragma once
// The WebAssembly interface (ABI) between the app and an extension, version 1: what a module may
// import from the app and what it must export. Checked at install time (WasmInspect), so a module
// that would only fail later (a missing function called in the middle of a run) is refused first,
// the same way in the native runtime and in the browser.
//
// Signatures are written "params:results" with i = i32, I = i64, f = f32, d = f64 ("iii:" takes three
// i32 and returns nothing). Most of the API goes through one function, mobilog.call(fn, in, inLen,
// out, outCap), with binary request and answer blocks described in sdk/include/mobilog_ext.h: new API
// functions are new `fn` numbers, not new imports.
#include <string_view>

namespace mobilog::ext {

struct AbiFunction {
    const char* module;
    const char* name;
    const char* signature;
};

// What the app provides. "mobilog": the API; "wasi_snapshot_preview1": the few WASI calls the C
// library uses (printing goes to the extension's log; no files, no environment).
inline constexpr AbiFunction kHostImports[] = {
    {"mobilog", "log", "iii:"},           // level, text, length
    {"mobilog", "time_ms", ":d"},         // monotonic milliseconds
    {"mobilog", "random", ":i"},          // the extension's own generator (never the simulation's)
    {"mobilog", "storage_get", "iiii:i"},  // key, keyLen, out, outCap -> size or -1
    {"mobilog", "storage_set", "iiii:i"},  // key, keyLen, value, valueLen -> 0 or error
    {"mobilog", "progress", "d:"},        // tasks: 0..1
    {"mobilog", "cancelled", ":i"},       // tasks: 1 when asked to stop
    {"mobilog", "check", ":i"},           // inserted by the metering pass (not called by hand)
    {"mobilog", "has", "ii:i"},           // name, length -> 1 when this app has the API function
    {"mobilog", "call", "iiiii:i"},       // fn, in, inLen, out, outCap -> answer size or error (<0)
    {"mobilog", "take", "ii:i"},          // out, outCap: the last answer again (when it did not fit)
    {"wasi_snapshot_preview1", "fd_write", "iiii:i"},
    {"wasi_snapshot_preview1", "fd_close", "i:i"},
    {"wasi_snapshot_preview1", "fd_seek", "iIii:i"},
    {"wasi_snapshot_preview1", "fd_fdstat_get", "ii:i"},
    {"wasi_snapshot_preview1", "proc_exit", "i:"},
    {"wasi_snapshot_preview1", "clock_time_get", "iIi:i"},
    {"wasi_snapshot_preview1", "random_get", "ii:i"},
    {"wasi_snapshot_preview1", "environ_sizes_get", "ii:i"},
    {"wasi_snapshot_preview1", "environ_get", "ii:i"},
    {"wasi_snapshot_preview1", "args_sizes_get", "ii:i"},
    {"wasi_snapshot_preview1", "args_get", "ii:i"},
};

struct AbiExport {
    const char* name;
    const char* signature;
    bool required;
};
inline constexpr AbiExport kExtensionExports[] = {
    {"mlx_api_version", ":i", true},  // the API level the extension was built for
    {"mlx_alloc", "i:i", true},       // buffers the app fills (requests, events)
    {"mlx_free", "i:", true},
    {"mlx_init", ":i", true},         // once, after loading: 0 = ok
    {"mlx_event", "iii:i", false},    // kind, data, length (hooks on the main thread)
    {"mlx_task", "iii:i", false},     // kind, input, length (background work)
    {"mlx_shutdown", ":", false},
    {"_initialize", ":", false},      // WASI reactor constructor (C/C++ static initializers)
};

inline const AbiFunction* findHostImport(std::string_view module, std::string_view name) {
    for (const auto& f : kHostImports)
        if (module == f.module && name == f.name) return &f;
    return nullptr;
}

}  // namespace mobilog::ext
