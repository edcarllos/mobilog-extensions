#pragma once
// Reads an extension's .wasm module at install time and checks that it stays inside what both
// runtimes (wasmtime in the native app, the browser's engine on the web) run the same way:
// - WebAssembly 1.0 plus bulk memory, sign extension, non-trapping float-to-int and mutable
//   globals; refused: SIMD, threads/atomics, exceptions, tail calls, reference types, multi-value,
//   memory64 and anything unknown (every instruction of every function is decoded);
// - one memory of its own, exported as "memory", with a declared maximum within the extension's
//   limit; no imported memory, table or global; no start function;
// - imports only from the app's list (Abi.h), with the right signatures; the required exports.
// The parse also keeps where each function body lies, for the metering pass.
#include "mlx/Bytes.h"

#include <string>
#include <vector>

namespace mobilog::ext {

struct WasmFuncType {
    std::vector<uint8_t> params, results;  // 0x7F i32, 0x7E i64, 0x7D f32, 0x7C f64
};
struct WasmImport {
    std::string module, name;
    uint8_t kind = 0;  // 0 function, 1 table, 2 memory, 3 global
    uint32_t type = 0;  // functions: type index
};
struct WasmExport {
    std::string name;
    uint8_t kind = 0;
    uint32_t index = 0;
};
struct WasmSectionInfo {
    uint8_t id = 0;
    size_t start = 0, size = 0;  // payload
};
struct WasmBody {
    size_t start = 0, size = 0;  // after the size prefix: locals + code
};

struct WasmModule {
    std::vector<WasmFuncType> types;
    std::vector<WasmImport> imports;
    uint32_t importedFunctions = 0;
    std::vector<uint32_t> functions;  // type of each defined function
    bool hasMemory = false, memoryHasMax = false;
    uint32_t memoryMin = 0, memoryMax = 0;  // 64 KiB pages
    uint32_t globals = 0;
    std::vector<WasmExport> exports;
    std::vector<WasmSectionInfo> sections;
    std::vector<WasmBody> bodies;
    uint32_t loops = 0;  // loop instructions in all functions

    const WasmFuncType* functionType(uint32_t funcIndex) const;
    const WasmExport* findExport(const std::string& name) const;
};

// Signature text of a function type ("ii:i"; see Abi.h).
std::string signatureOf(const WasmFuncType& t);

// Structure and the instruction subset; false with a message in Portuguese.
bool parseWasm(const Bytes& wasm, WasmModule& out, std::string* error);

// parseWasm plus the app's rules (imports, exports, memory). Problems are listed in Portuguese.
bool inspectWasm(const Bytes& wasm, uint32_t memoryLimitMB, WasmModule& out, std::vector<std::string>& problems);

}  // namespace mobilog::ext
