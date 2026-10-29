#include "mlx/Metering.h"

#include "mlx/WasmInspect.h"
#include "mlx/WasmReader.h"

#include <algorithm>

namespace mobilog::ext {
namespace {

using namespace wasmbin;

void leb(Bytes& b, uint64_t v) {
    do {
        uint8_t c = v & 0x7F;
        v >>= 7;
        if (v) c |= 0x80;
        b.push_back(c);
    } while (v);
}
void sleb(Bytes& b, int64_t v) {
    bool more = true;
    while (more) {
        uint8_t c = v & 0x7F;
        v >>= 7;
        more = !((v == 0 && !(c & 0x40)) || (v == -1 && (c & 0x40)));
        if (more) c |= 0x80;
        b.push_back(c);
    }
}
void name(Bytes& b, const std::string& s) {
    leb(b, s.size());
    b.insert(b.end(), s.begin(), s.end());
}
void append(Bytes& b, const uint8_t* from, const uint8_t* to) { b.insert(b.end(), from, to); }

int sectionRank(uint8_t id) {
    static const int rank[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 10};
    return rank[id];
}

struct Rewrite {
    uint32_t fuel = 0;        // index of the fuel global
    uint32_t check = 0;       // index of mobilog.check
    uint32_t oldImports = 0;  // function imports before the pass
    uint32_t shift = 0;       // 1 when the check import was added
    uint32_t fn(uint32_t idx) const { return idx >= oldImports ? idx + shift : idx; }

    void snippet(Bytes& o) const {
        o.push_back(0x23), leb(o, fuel);                                            // global.get fuel
        o.push_back(0x41), o.push_back(0x01), o.push_back(0x6B);                    // i32.const 1, i32.sub
        o.push_back(0x24), leb(o, fuel);                                            // global.set fuel
        o.push_back(0x23), leb(o, fuel);                                            // global.get fuel
        o.push_back(0x45);                                                          // i32.eqz (it reaches 0 exactly)
        o.push_back(0x04), o.push_back(0x40);                                       // if
        o.push_back(0x10), leb(o, check);                                           //   call check
        o.push_back(0x24), leb(o, fuel);                                            //   global.set fuel
        o.push_back(0x23), leb(o, fuel);                                            //   global.get fuel
        o.push_back(0x45);                                                          //   i32.eqz
        o.push_back(0x04), o.push_back(0x40), o.push_back(0x00), o.push_back(0x0B);  //   if unreachable end
        o.push_back(0x0B);                                                          // end
    }
};

// Copies a vector section, adding one item at the end.
Bytes withExtraItem(const uint8_t* p, size_t size, const Bytes& item, std::string& err) {
    Reader r{p, p + size, {}};
    const uint32_t n = r.u32();
    Bytes o;
    if (!r.ok()) {
        err = r.err;
        return o;
    }
    leb(o, uint64_t(n) + 1);
    append(o, r.p, r.end);
    o.insert(o.end(), item.begin(), item.end());
    return o;
}

Bytes rewriteExports(const uint8_t* p, size_t size, const Rewrite& rw, std::string& err) {
    Reader r{p, p + size, {}};
    const uint32_t n = r.count();
    Bytes o;
    leb(o, n);
    for (uint32_t i = 0; i < n && r.ok(); ++i) {
        const std::string nm = r.name();
        const uint8_t kind = r.byte();
        const uint32_t idx = r.u32();
        name(o, nm);
        o.push_back(kind);
        leb(o, kind == 0 ? rw.fn(idx) : idx);
    }
    err = r.err;
    return o;
}

Bytes rewriteElements(const uint8_t* p, size_t size, const Rewrite& rw, std::string& err) {
    Reader r{p, p + size, {}};
    const uint32_t n = r.count();
    Bytes o;
    leb(o, n);
    for (uint32_t i = 0; i < n && r.ok(); ++i) {
        const uint32_t flags = r.u32();
        leb(o, flags);
        if (flags == 2) leb(o, r.u32());
        if (flags == 0 || flags == 2) {
            const uint8_t* s = r.p;
            constExpr(r);
            append(o, s, r.p);
        }
        if (flags != 0) o.push_back(r.byte());
        const uint32_t k = r.count();
        leb(o, k);
        for (uint32_t j = 0; j < k && r.ok(); ++j) leb(o, rw.fn(r.u32()));
    }
    err = r.err;
    return o;
}

Bytes rewriteCode(const uint8_t* p, size_t size, const Rewrite& rw, std::string& err) {
    Reader r{p, p + size, {}};
    const uint32_t n = r.count();
    Bytes o;
    leb(o, n);
    uint32_t loops = 0;
    for (uint32_t i = 0; i < n && r.ok(); ++i) {
        const uint32_t bodySize = r.u32();
        if (!r.ok() || bodySize > r.left()) {
            r.failed("função truncada no módulo");
            break;
        }
        Reader b{r.p, r.p + bodySize, {}};
        r.p += bodySize;
        Bytes body;
        const uint32_t groups = b.count();
        for (uint32_t g = 0; g < groups && b.ok(); ++g) {
            b.u32();
            valType(b);
        }
        append(body, r.p - bodySize, b.p);  // the locals, as they were
        // Function entry: only where the function calls others (recursion). A function without
        // calls can only run long in a loop, and loops are metered.
        bool calls = false;
        {
            Reader scan = b;
            uint32_t unused = 0;
            int d = 1;
            while (scan.ok() && d > 0) {
                const uint8_t op = scan.byte();
                calls = calls || op == 0x10 || op == 0x11;
                d += instruction(scan, op, unused);
            }
        }
        if (calls) rw.snippet(body);
        int depth = 1;
        while (b.ok() && depth > 0) {
            const uint8_t* start = b.p;
            const uint8_t op = b.byte();
            if (op == 0x10) {  // call: the index may move
                body.push_back(0x10);
                leb(body, rw.fn(b.u32()));
                continue;
            }
            depth += instruction(b, op, loops);
            append(body, start, b.p);
            if (op == 0x03) rw.snippet(body);  // each loop iteration
        }
        if (!b.ok()) {
            r.failed(b.err.c_str());
            break;
        }
        leb(o, body.size());
        o.insert(o.end(), body.begin(), body.end());
    }
    err = r.err;
    return o;
}

}  // namespace

bool meterModule(const Bytes& in, Bytes& out, std::string* error) {
    WasmModule m;
    std::string err;
    if (!parseWasm(in, m, &err)) {
        if (error) *error = err;
        return false;
    }
    Rewrite rw;
    rw.oldImports = m.importedFunctions;
    rw.fuel = m.globals;  // no imported globals: defined ones start at 0
    bool addImport = true;
    for (uint32_t i = 0; i < m.imports.size(); ++i)
        if (m.imports[i].module == "mobilog" && m.imports[i].name == "check" && m.types[m.imports[i].type].params.empty() &&
            m.types[m.imports[i].type].results == std::vector<uint8_t>{0x7F}) {
            rw.check = i;
            addImport = false;
        }
    uint32_t checkType = uint32_t(m.types.size());
    bool addType = addImport;
    for (uint32_t t = 0; addImport && t < m.types.size(); ++t)
        if (m.types[t].params.empty() && m.types[t].results == std::vector<uint8_t>{0x7F}) {
            checkType = t;
            addType = false;
        }
    if (addImport) {
        rw.check = m.importedFunctions;
        rw.shift = 1;
    }

    const Bytes typeItem{0x60, 0x00, 0x01, 0x7F};
    Bytes importItem;
    name(importItem, "mobilog");
    name(importItem, "check");
    importItem.push_back(0x00);
    leb(importItem, checkType);
    Bytes globalItem{0x7F, 0x01, 0x41};
    sleb(globalItem, kMeterSlice);
    globalItem.push_back(0x0B);

    struct Out {
        uint8_t id;
        Bytes payload;
    };
    std::vector<Out> sections;
    bool sawType = false, sawImport = false, sawGlobal = false;
    for (const auto& s : m.sections) {
        if (s.id == 0) continue;  // custom sections: names, producers... dropped
        const uint8_t* p = in.data() + s.start;
        Out o{s.id, {}};
        switch (s.id) {
        case 1:
            sawType = true;
            o.payload = addType ? withExtraItem(p, s.size, typeItem, err) : Bytes(p, p + s.size);
            break;
        case 2:
            sawImport = true;
            o.payload = addImport ? withExtraItem(p, s.size, importItem, err) : Bytes(p, p + s.size);
            break;
        case 6:
            sawGlobal = true;
            o.payload = withExtraItem(p, s.size, globalItem, err);
            break;
        case 7: o.payload = rw.shift ? rewriteExports(p, s.size, rw, err) : Bytes(p, p + s.size); break;
        case 9: o.payload = rw.shift ? rewriteElements(p, s.size, rw, err) : Bytes(p, p + s.size); break;
        case 10: o.payload = rewriteCode(p, s.size, rw, err); break;
        default: o.payload.assign(p, p + s.size); break;
        }
        if (!err.empty()) {
            if (error) *error = err;
            return false;
        }
        sections.push_back(std::move(o));
    }
    const auto single = [](const Bytes& item) {
        Bytes b{0x01};
        b.insert(b.end(), item.begin(), item.end());
        return b;
    };
    if (addType && !sawType) sections.push_back({1, single(typeItem)});
    if (addImport && !sawImport) sections.push_back({2, single(importItem)});
    if (!sawGlobal) sections.push_back({6, single(globalItem)});
    std::stable_sort(sections.begin(), sections.end(), [](const Out& a, const Out& b) { return sectionRank(a.id) < sectionRank(b.id); });

    out.assign(in.begin(), in.begin() + 8);
    for (const auto& s : sections) {
        out.push_back(s.id);
        leb(out, s.payload.size());
        out.insert(out.end(), s.payload.begin(), s.payload.end());
    }
    return true;
}

}  // namespace mobilog::ext
