#include "mlx/WasmInspect.h"

#include "mlx/Abi.h"
#include "mlx/WasmReader.h"

#include <algorithm>
#include <set>

namespace mobilog::ext {
namespace {

using namespace wasmbin;

int sectionRank(uint8_t id) {
    static const int rank[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 10};  // data count (12) before code (10)
    return id <= 12 ? rank[id] : -1;
}

bool parseInto(const Bytes& wasm, WasmModule& m, std::string& err) {
    Reader r{wasm.data(), wasm.data() + wasm.size(), {}};
    static const uint8_t kHeader[8] = {0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00};
    if (wasm.size() < 8 || !std::equal(kHeader, kHeader + 8, wasm.begin())) {
        err = "o arquivo não é um módulo WebAssembly (versão 1)";
        return false;
    }
    r.p += 8;
    int lastRank = 0;
    bool sawCode = false;
    while (r.ok() && r.p < r.end) {
        const uint8_t id = r.byte();
        const uint32_t size = r.u32();
        if (!r.ok()) break;
        if (size > r.left()) {
            r.failed("módulo truncado");
            break;
        }
        const int rank = sectionRank(id);
        if (rank < 0) {
            r.failed("seção desconhecida no módulo");
            break;
        }
        if (id != 0) {
            if (rank <= lastRank) {
                r.failed("seções fora de ordem no módulo");
                break;
            }
            lastRank = rank;
        }
        m.sections.push_back({id, size_t(r.p - wasm.data()), size});
        Reader s{r.p, r.p + size, {}};
        r.p += size;
        switch (id) {
        case 0:
            s.name();
            s.p = s.end;
            break;
        case 1: {
            const uint32_t n = s.count();
            for (uint32_t i = 0; i < n && s.ok(); ++i) {
                if (s.byte() != 0x60 && s.ok()) s.failed("tipo inválido no módulo");
                WasmFuncType t;
                const uint32_t np = s.count();
                for (uint32_t k = 0; k < np && s.ok(); ++k) {
                    const uint8_t* at = s.p;
                    valType(s);
                    if (s.ok()) t.params.push_back(*at);
                }
                const uint32_t nr = s.count();
                if (s.ok() && nr > 1) s.failed("o módulo usa funções com vários resultados (multi-value), que não são permitidas");
                for (uint32_t k = 0; k < nr && s.ok(); ++k) {
                    const uint8_t* at = s.p;
                    valType(s);
                    if (s.ok()) t.results.push_back(*at);
                }
                m.types.push_back(std::move(t));
            }
            break;
        }
        case 2: {
            const uint32_t n = s.count();
            for (uint32_t i = 0; i < n && s.ok(); ++i) {
                WasmImport im;
                im.module = s.name();
                im.name = s.name();
                im.kind = s.byte();
                if (!s.ok()) break;
                if (im.kind != 0) {
                    s.failed(im.kind == 2 ? "o módulo importa a memória; ele deve ter a sua própria"
                                          : "o módulo importa tabelas ou variáveis globais, o que não é permitido");
                    break;
                }
                im.type = s.u32();
                if (s.ok() && im.type >= m.types.size()) s.failed("tipo inválido no módulo");
                ++m.importedFunctions;
                m.imports.push_back(std::move(im));
            }
            break;
        }
        case 3: {
            const uint32_t n = s.count();
            for (uint32_t i = 0; i < n && s.ok(); ++i) {
                const uint32_t t = s.u32();
                if (s.ok() && t >= m.types.size()) s.failed("tipo inválido no módulo");
                m.functions.push_back(t);
            }
            break;
        }
        case 4: {
            const uint32_t n = s.count();
            if (n > 1) s.failed("o módulo usa várias tabelas, o que não é permitido");
            for (uint32_t i = 0; i < n && s.ok(); ++i) {
                if (s.byte() != 0x70 && s.ok()) s.failed("o módulo usa tipos de referência, que não são permitidos");
                uint32_t mn, mx;
                bool hm;
                limits(s, mn, mx, hm, false);
            }
            break;
        }
        case 5: {
            const uint32_t n = s.count();
            if (n > 1) s.failed("o módulo usa várias memórias, o que não é permitido");
            for (uint32_t i = 0; i < n && s.ok(); ++i) {
                limits(s, m.memoryMin, m.memoryMax, m.memoryHasMax, true);
                m.hasMemory = true;
            }
            break;
        }
        case 6: {
            const uint32_t n = s.count();
            for (uint32_t i = 0; i < n && s.ok(); ++i) {
                valType(s);
                const uint8_t mut = s.byte();
                if (s.ok() && mut > 1) s.failed("variável global inválida no módulo");
                constExpr(s);
                ++m.globals;
            }
            break;
        }
        case 7: {
            const uint32_t n = s.count();
            std::set<std::string> names;
            for (uint32_t i = 0; i < n && s.ok(); ++i) {
                WasmExport e;
                e.name = s.name();
                e.kind = s.byte();
                e.index = s.u32();
                if (!s.ok()) break;
                if (e.kind > 3) s.failed("exportação inválida no módulo");
                if (!names.insert(e.name).second) s.failed("exportação repetida no módulo");
                m.exports.push_back(std::move(e));
            }
            break;
        }
        case 8: s.failed("o módulo tem função de início (start), o que não é permitido; use mlx_init"); break;
        case 9: {
            const uint32_t n = s.count();
            for (uint32_t i = 0; i < n && s.ok(); ++i) {
                const uint32_t flags = s.u32();
                if (!s.ok()) break;
                if (flags > 3) {
                    s.failed("o módulo usa tipos de referência (elementos), que não são permitidos");
                    break;
                }
                if (flags == 2) {
                    if (s.u32() != 0 && s.ok()) s.failed("o módulo usa várias tabelas, o que não é permitido");
                }
                if (flags == 0 || flags == 2) constExpr(s);
                if (flags != 0 && s.byte() != 0x00 && s.ok()) s.failed("elementos inválidos no módulo");
                const uint32_t k = s.count();
                for (uint32_t j = 0; j < k && s.ok(); ++j) s.u32();
            }
            break;
        }
        case 10: {
            const uint32_t n = s.count();
            if (s.ok() && n != m.functions.size()) s.failed("número de funções inconsistente no módulo");
            for (uint32_t i = 0; i < n && s.ok(); ++i) {
                const uint32_t bodySize = s.u32();
                if (!s.ok() || bodySize > s.left()) {
                    s.failed("função truncada no módulo");
                    break;
                }
                WasmBody body{size_t(s.p - wasm.data()), bodySize};
                Reader b{s.p, s.p + bodySize, {}};
                s.p += bodySize;
                const uint32_t groups = b.count();
                uint64_t locals = 0;
                for (uint32_t g = 0; g < groups && b.ok(); ++g) {
                    locals += b.u32();
                    valType(b);
                }
                if (b.ok() && locals > kMaxLocals) b.failed("função com variáveis locais demais no módulo");
                int depth = 1;
                while (b.ok() && depth > 0) depth += instruction(b, b.byte(), m.loops);
                if (b.ok() && b.p != b.end) b.failed("função inválida no módulo");
                if (!b.ok()) s.failed(b.err.c_str());
                m.bodies.push_back(body);
            }
            sawCode = true;
            break;
        }
        case 11: {
            const uint32_t n = s.count();
            for (uint32_t i = 0; i < n && s.ok(); ++i) {
                const uint32_t flags = s.u32();
                if (flags > 2) {
                    s.failed("dados inválidos no módulo");
                    break;
                }
                if (flags == 2 && s.u32() != 0 && s.ok()) s.failed("o módulo usa várias memórias, o que não é permitido");
                if (flags != 1) constExpr(s);
                const uint32_t len = s.u32();
                s.skip(len);
            }
            break;
        }
        case 12: s.u32(); break;
        }
        if (s.ok() && s.p != s.end) s.failed("seção inválida no módulo");
        if (!s.ok()) r.failed(s.err.c_str());
    }
    if (r.ok() && !sawCode && !m.functions.empty()) r.failed("número de funções inconsistente no módulo");
    err = r.err;
    return r.ok();
}

}  // namespace

const WasmFuncType* WasmModule::functionType(uint32_t funcIndex) const {
    uint32_t type;
    if (funcIndex < importedFunctions) type = imports[funcIndex].type;  // only function imports exist
    else if (funcIndex - importedFunctions < functions.size()) type = functions[funcIndex - importedFunctions];
    else return nullptr;
    return type < types.size() ? &types[type] : nullptr;
}

const WasmExport* WasmModule::findExport(const std::string& name) const {
    for (const auto& e : exports)
        if (e.name == name) return &e;
    return nullptr;
}

std::string signatureOf(const WasmFuncType& t) {
    const auto letter = [](uint8_t v) {
        switch (v) {
        case 0x7F: return 'i';
        case 0x7E: return 'I';
        case 0x7D: return 'f';
        default: return 'd';
        }
    };
    std::string s;
    for (uint8_t p : t.params) s += letter(p);
    s += ':';
    for (uint8_t r : t.results) s += letter(r);
    return s;
}

bool parseWasm(const Bytes& wasm, WasmModule& out, std::string* error) {
    out = {};
    std::string err;
    const bool ok = parseInto(wasm, out, err);
    if (error) *error = err;
    return ok;
}

bool inspectWasm(const Bytes& wasm, uint32_t memoryLimitMB, WasmModule& m, std::vector<std::string>& problems) {
    problems.clear();
    std::string err;
    if (!parseWasm(wasm, m, &err)) {
        problems.push_back(err);
        return false;
    }
    for (const auto& im : m.imports) {
        const AbiFunction* f = findHostImport(im.module, im.name);
        if (!f) problems.push_back("importa " + sanitizeText(im.module + "." + im.name, 60) + ", que o Mobilog não oferece");
        else if (signatureOf(m.types[im.type]) != f->signature)
            problems.push_back("importa " + im.module + "." + im.name + " com tipos diferentes dos do Mobilog");
    }
    if (!m.hasMemory) problems.push_back("o módulo não tem memória própria");
    else {
        if (!m.memoryHasMax) problems.push_back("a memória do módulo precisa de um máximo declarado (opção --max-memory)");
        else if (uint64_t(m.memoryMax) * 65536 > uint64_t(memoryLimitMB) << 20)
            problems.push_back("a memória máxima do módulo (" + std::to_string(uint64_t(m.memoryMax) * 65536 >> 20) +
                               " MB) passa do limite da extensão (" + std::to_string(memoryLimitMB) + " MB)");
        const WasmExport* mem = m.findExport("memory");
        if (!mem || mem->kind != 2) problems.push_back("o módulo deve exportar a memória como \"memory\"");
    }
    for (const auto& ex : kExtensionExports) {
        const WasmExport* e = m.findExport(ex.name);
        if (!e) {
            if (ex.required) problems.push_back(std::string("falta a função ") + ex.name);
            continue;
        }
        const WasmFuncType* t = e->kind == 0 ? m.functionType(e->index) : nullptr;
        if (!t || signatureOf(*t) != ex.signature) problems.push_back(std::string("a função ") + ex.name + " tem tipos diferentes dos esperados");
    }
    return problems.empty();
}

}  // namespace mobilog::ext
