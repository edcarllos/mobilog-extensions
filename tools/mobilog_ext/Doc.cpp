// doc: the API reference (docs/pt/api.md, docs/en/api.md), written from the SDK's header
// (sdk/include/mobilog_ext.h: each function's number and its request → answer layout) and the app's
// table (mlx/Api.h: its name, permission and where it may be called). The store's CI writes it again
// and fails when the committed one differs, so the reference never drifts from the code.
#include "Tool.h"

#include "mlx/Api.h"

#include <cstdio>
#include <map>
#include <regex>
#include <sstream>

namespace mlxtool {

using namespace mobilog::ext;

namespace {

struct FnDoc {
    std::string macro, layout, group;
    uint32_t number = 0;
};

std::string trim(std::string s) {
    const size_t a = s.find_first_not_of(" \t*");
    const size_t b = s.find_last_not_of(" \t*/");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

// The functions of the header, in order, with the comment over each group ("/* Mapa (mapa.ler)... */").
std::vector<FnDoc> readHeader(const std::string& text) {
    std::vector<FnDoc> out;
    std::istringstream in(text);
    std::string line, group;
    static const std::regex def(R"(^#define\s+(MLX_FN_[A-Z0-9_]+)\s+(\d+)\s*(/\*(.*))?$)");
    static const std::regex groupLine(R"(^/\*\s*([^*].*?)(\*/)?\s*$)");
    bool inLayout = false;
    while (std::getline(in, line)) {
        std::smatch m;
        if (inLayout) {
            const bool end = line.find("*/") != std::string::npos;
            out.back().layout += " " + trim(line.substr(0, end ? line.find("*/") : std::string::npos));
            inLayout = !end;
            continue;
        }
        if (std::regex_match(line, m, def)) {
            FnDoc f;
            f.macro = m[1];
            f.number = uint32_t(std::stoul(m[2]));
            f.group = group;
            std::string rest = m[4];
            const bool end = rest.find("*/") != std::string::npos;
            f.layout = trim(end ? rest.substr(0, rest.find("*/")) : rest);
            out.push_back(f);
            inLayout = !end && m[3].matched;
            continue;
        }
        if (line.rfind("/* ----", 0) == 0) continue;
        if (std::regex_match(line, m, groupLine) && line.find("#define") == std::string::npos) {
            std::string g = trim(m[1]);
            // The group's name: up to the first sentence or colon (the rest is in the SDK header).
            int depth = 0;
            for (size_t i = 0; i + 1 < g.size(); ++i) {
                depth += g[i] == '(' ? 1 : g[i] == ')' ? -1 : 0;
                if (depth == 0 && (g[i] == '.' || g[i] == ':') && g[i + 1] == ' ') {
                    g.resize(i);
                    break;
                }
            }
            while (!g.empty() && (g.back() == '.' || g.back() == ':' || g.back() == ';')) g.pop_back();
            if (!g.empty() && g.size() < 120) group = g;
        }
    }
    return out;
}

// The group names in English (the header is in Portuguese).
std::string groupEn(std::string g) {
    static const std::pair<const char*, const char*> kWords[] = {
        {"A própria extensão", "The extension itself"}, {"Desenho no mapa", "Drawing on the map"},
        {"Veículos gerenciados", "Managed vehicles"}, {"Tarefas em segundo plano", "Background tasks"},
        {"Resolvedores VRP", "VRP solvers"}, {"Simulação", "Simulation"}, {"Cenário", "Scenario"}, {"Ensaios", "Experiments"},
        {"Rotas", "Routes"}, {"Mapa", "Map"}, {"só ganchos", "hooks only"}, {"até 2 ao mesmo tempo", "up to 2 at a time"},
        {"até 500", "up to 500"}, {"manifesto", "manifest"}};
    for (const auto& [pt, en] : kWords)
        if (const size_t at = g.find(pt); at != std::string::npos) g.replace(at, std::string(pt).size(), en);
    return g;
}

std::string escapeCell(std::string s) {
    for (size_t p = 0; (p = s.find('|', p)) != std::string::npos; p += 2) s.replace(p, 1, "\\|");
    return s;
}

std::string whereText(uint8_t w, bool pt) {
    std::string s;
    const auto add = [&](const char* x) { s += (s.empty() ? "" : ", ") + std::string(x); };
    if (w & kInHooks) add(pt ? "ganchos" : "hooks");
    if (w & kInTasks) add(pt ? "tarefas" : "tasks");
    if (w & kInSolver) add(pt ? "resolvedores" : "solvers");
    return s;
}

std::string permissionText(uint32_t p, bool pt) {
    if (!p) return pt ? "nenhuma" : "none";
    const PermissionInfo* info = permissionInfo(Permission(p));
    return info ? "`" + std::string(info->key) + "`" : "?";
}

}  // namespace

int cmdDoc(const Args& a) {
    if (a.positional.size() != 1 || a.positional[0] != "api") return usage();
    const bool pt = a.lang != "en";
    const fs::path root = repoRoot();
    std::string header;
    if (root.empty() || !readText(root / "sdk" / "include" / "mobilog_ext.h", header)) {
        std::fprintf(stderr, "%s\n", tr("erro: sdk/include/mobilog_ext.h não encontrado", "error: sdk/include/mobilog_ext.h not found"));
        return 1;
    }
    const std::vector<FnDoc> fns = readHeader(header);
    std::ostringstream md;
    if (pt) {
        md << "# Referência da API\n\n"
              "> Gerada por `mobilog_ext doc api` a partir de `sdk/include/mobilog_ext.h` e da tabela do app (`mlx/Api.h`). "
              "Não edite à mão: o CI confere.\n\n"
              "Cada função é chamada com `mobilog_call(número, pedido, tamanho, resposta, capacidade)` (em C++, `mlx::call`). "
              "Os blocos usam os tipos `u8`, `u32`, `i32`, `u64`, `f32`, `f64` (little-endian), `texto` (u32 tamanho + UTF-8), "
              "`ponto` (f64 lon, f64 lat) e `lista` (u32 quantidade + itens). **Onde:** ganchos = `mlx_init`/`mlx_event` (thread "
              "principal), tarefas = `mlx_task`, resolvedores = execução de um resolvedor de rotas.\n\n"
              "| Função | Nº | Permissão | Onde | Pedido → resposta |\n|---|---|---|---|---|\n";
    } else {
        md << "# API reference\n\n"
              "> Written by `mobilog_ext doc api` from `sdk/include/mobilog_ext.h` and the app's table (`mlx/Api.h`). "
              "Do not edit by hand: the CI checks it.\n\n"
              "Each function is called with `mobilog_call(number, request, size, answer, capacity)` (C++: `mlx::call`). "
              "Blocks use `u8`, `u32`, `i32`, `u64`, `f32`, `f64` (little-endian), `texto` = text (u32 size + UTF-8), "
              "`ponto` = point (f64 lon, f64 lat) and `lista` = list (u32 count + items). **Where:** hooks = `mlx_init`/`mlx_event` "
              "(main thread), tasks = `mlx_task`, solvers = a route solver's run. The layout notes come from the SDK header, in "
              "Portuguese (the types and order are what matter).\n\n"
              "| Function | No. | Permission | Where | Request → answer |\n|---|---|---|---|---|\n";
    }
    std::string lastGroup;
    int missing = 0;
    for (const FnDoc& f : fns) {
        const ApiFunctionInfo* info = apiFunction(f.number);
        if (!info) {
            std::fprintf(stderr, "%s %s (%u)\n", tr("aviso: a tabela do app não tem", "warning: the app's table has no"), f.macro.c_str(), f.number);
            ++missing;
            continue;
        }
        if (f.group != lastGroup && !f.group.empty()) {
            md << "| **" << escapeCell(pt ? f.group : groupEn(f.group)) << "** | | | | |\n";
            lastGroup = f.group;
        }
        std::string layout = f.layout;
        // "[G T] — a → b" and "[mapa.ferramentas] a → b": where and permission are columns already.
        if (layout.rfind("[", 0) == 0 && layout.find(']') != std::string::npos) layout = layout.substr(layout.find(']') + 1);
        layout = trim(layout);
        if (layout.rfind("— ", 0) == 0) layout = layout.substr(std::string("— ").size());
        md << "| `" << info->name << "` `" << f.macro << "` | " << f.number << " | " << permissionText(info->permission, pt) << " | "
           << whereText(info->where, pt) << " | " << escapeCell(layout) << " |\n";
    }
    for (const ApiFunctionInfo& info : kApiFunctions)
        if (std::none_of(fns.begin(), fns.end(), [&](const FnDoc& f) { return f.number == info.fn; })) {
            std::fprintf(stderr, "%s %s (%u)\n", tr("aviso: o cabeçalho do SDK não tem", "warning: the SDK header has no"), info.name, info.fn);
            ++missing;
        }
    const std::string text = md.str();
    if (a.out.empty()) std::fputs(text.c_str(), stdout);
    else if (!writeFile(a.out, text.data(), text.size())) return 1;
    return missing ? 1 : 0;
}

}  // namespace mlxtool
