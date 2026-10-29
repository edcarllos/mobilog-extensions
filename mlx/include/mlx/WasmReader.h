#pragma once
// Decoding WebAssembly bytecode (internal to mlx/): a bounds-checked reader and one
// instruction at a time, for the install check (WasmInspect) and the metering pass (Metering).
// Errors are kept in the reader (first one wins), in Portuguese.
#include "mlx/Bytes.h"

#include <string>

namespace mobilog::ext::wasmbin {

inline constexpr uint32_t kMaxCount = 200000;  // types, functions, globals, exports... (sanity)
inline constexpr uint32_t kMaxLocals = 50000;  // per function

struct Reader {
    const uint8_t* p;
    const uint8_t* end;
    std::string err;

    bool ok() const { return err.empty(); }
    bool failed(const char* msg) {
        if (err.empty()) err = msg;
        return false;
    }
    size_t left() const { return size_t(end - p); }
    uint8_t byte() {
        if (p >= end) {
            failed("módulo truncado");
            return 0;
        }
        return *p++;
    }
    uint32_t u32() {
        uint64_t v = 0;
        for (int shift = 0; shift < 35; shift += 7) {
            const uint8_t b = byte();
            if (!ok()) return 0;
            v |= uint64_t(b & 0x7F) << shift;
            if (!(b & 0x80)) {
                if (v > 0xFFFFFFFFull) failed("número inválido no módulo");
                return uint32_t(v);
            }
        }
        failed("número inválido no módulo");
        return 0;
    }
    // Signed LEB128 of up to `bits` bits; only its validity matters here.
    void sleb(int bits) {
        const int maxBytes = (bits + 6) / 7;
        for (int i = 0; i < maxBytes; ++i) {
            const uint8_t b = byte();
            if (!ok() || !(b & 0x80)) return;
        }
        failed("número inválido no módulo");
    }
    void skip(size_t n) {
        if (n > left()) {
            failed("módulo truncado");
            p = end;
            return;
        }
        p += n;
    }
    std::string name() {
        const uint32_t n = u32();
        if (!ok() || n > left()) {
            failed("nome inválido no módulo");
            return {};
        }
        std::string s(reinterpret_cast<const char*>(p), n);
        p += n;
        if (!validUtf8(s)) failed("nome inválido no módulo");
        return s;
    }
    uint32_t count() {
        const uint32_t n = u32();
        if (ok() && n > kMaxCount) failed("módulo grande demais");
        return n;
    }
};

inline bool numericType(uint8_t t) { return t == 0x7F || t == 0x7E || t == 0x7D || t == 0x7C; }

inline void valType(Reader& r) {
    const uint8_t t = r.byte();
    if (r.ok() && !numericType(t)) r.failed(t == 0x7B ? "o módulo usa SIMD, que não é permitido" : "o módulo usa tipos de referência, que não são permitidos");
}

inline void limits(Reader& r, uint32_t& min, uint32_t& max, bool& hasMax, bool memory) {
    const uint8_t flags = r.byte();
    if (!r.ok()) return;
    if (flags > 1) {
        r.failed(memory ? (flags & 2 ? "o módulo usa memória compartilhada (threads), que não é permitida"
                                     : "o módulo usa memória de 64 bits, que não é permitida")
                        : "tabela inválida no módulo");
        return;
    }
    min = r.u32();
    hasMax = flags == 1;
    max = hasMax ? r.u32() : 0;
    if (hasMax && max < min) r.failed("limites inválidos no módulo");
    if (memory && (min > 65536 || max > 65536)) r.failed("memória inválida no módulo");
}

inline void blockType(Reader& r) {
    const uint8_t b = r.byte();
    if (!r.ok() || b == 0x40 || numericType(b)) return;
    if (b == 0x7B) r.failed("o módulo usa SIMD, que não é permitido");
    else r.failed("o módulo usa blocos com vários valores (multi-value), que não são permitidos");
}

inline void memarg(Reader& r) {
    const uint32_t align = r.u32();
    if (r.ok() && align >= 32) r.failed("instrução de memória inválida no módulo");
    r.u32();  // offset
}

inline void zeroByte(Reader& r) {
    if (r.byte() != 0 && r.ok()) r.failed("o módulo usa várias memórias ou tabelas, o que não é permitido");
}

// One instruction (the opcode already read). Returns the block depth change.
inline int instruction(Reader& r, uint8_t op, uint32_t& loops) {
    switch (op) {
    case 0x00: case 0x01: case 0x05: case 0x0F: case 0x1A: case 0x1B: return 0;  // unreachable nop else return drop select
    case 0x02: case 0x04: blockType(r); return 1;                                 // block if
    case 0x03: blockType(r); ++loops; return 1;                                  // loop
    case 0x0B: return -1;                                                         // end
    case 0x0C: case 0x0D: r.u32(); return 0;                                     // br br_if
    case 0x0E: {                                                                  // br_table
        const uint32_t n = r.count();
        for (uint32_t i = 0; i <= n && r.ok(); ++i) r.u32();
        return 0;
    }
    case 0x10: r.u32(); return 0;  // call
    case 0x11: r.u32(); zeroByte(r); return 0;  // call_indirect type 0x00
    case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: r.u32(); return 0;  // local.* global.*
    case 0x3F: case 0x40: zeroByte(r); return 0;  // memory.size memory.grow
    case 0x41: r.sleb(32); return 0;               // i32.const
    case 0x42: r.sleb(64); return 0;               // i64.const
    case 0x43: r.skip(4); return 0;                // f32.const
    case 0x44: r.skip(8); return 0;                // f64.const
    case 0xFC: {
        const uint32_t sub = r.u32();
        if (!r.ok()) return 0;
        if (sub <= 7) return 0;  // non-trapping float-to-int
        switch (sub) {
        case 8: r.u32(); zeroByte(r); return 0;     // memory.init data 0x00
        case 9: r.u32(); return 0;                  // data.drop
        case 10: zeroByte(r); zeroByte(r); return 0;  // memory.copy
        case 11: zeroByte(r); return 0;             // memory.fill
        case 12: r.u32(); zeroByte(r); return 0;    // table.init elem 0x00
        case 13: r.u32(); return 0;                 // elem.drop
        case 14: zeroByte(r); zeroByte(r); return 0;  // table.copy
        default: r.failed("o módulo usa tipos de referência (tabelas), que não são permitidos"); return 0;
        }
    }
    case 0xFD: r.failed("o módulo usa SIMD, que não é permitido"); return 0;
    case 0xFE: r.failed("o módulo usa threads (atômicos), que não são permitidos"); return 0;
    case 0x06: case 0x07: case 0x08: case 0x09: case 0x18: case 0x19: case 0x1F:
        r.failed("o módulo usa exceções do WebAssembly, que não são permitidas");
        return 0;
    case 0x12: case 0x13: case 0x14: case 0x15: r.failed("o módulo usa chamadas de cauda ou por referência, que não são permitidas"); return 0;
    case 0x1C: case 0x25: case 0x26: case 0xD0: case 0xD1: case 0xD2:
        r.failed("o módulo usa tipos de referência, que não são permitidos");
        return 0;
    default:
        if (op >= 0x28 && op <= 0x3E) {  // loads and stores
            memarg(r);
            return 0;
        }
        if (op >= 0x45 && op <= 0xC4) return 0;  // numeric (0xC0-0xC4: sign extension)
        r.failed("o módulo usa uma instrução desconhecida");
        return 0;
    }
}

inline void constExpr(Reader& r) {
    const uint8_t op = r.byte();
    switch (op) {
    case 0x41: r.sleb(32); break;
    case 0x42: r.sleb(64); break;
    case 0x43: r.skip(4); break;
    case 0x44: r.skip(8); break;
    case 0x23: r.u32(); break;
    default: r.failed("expressão constante inválida no módulo"); return;
    }
    if (r.byte() != 0x0B && r.ok()) r.failed("expressão constante inválida no módulo");
}


}  // namespace mobilog::ext::wasmbin
