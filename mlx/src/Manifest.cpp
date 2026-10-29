#include "mlx/Manifest.h"

#include "mlx/Bytes.h"
#include "mlx/Zip.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <set>

namespace mobilog::ext {

using nlohmann::json;

const std::vector<PermissionInfo>& permissionTable() {
    static const std::vector<PermissionInfo> t = {
        {kPermMapRead, "mapa.ler", "Ler o mapa", "Ver a malha viária, os nomes das ruas, os locais, as zonas e os bairros da área.", false},
        {kPermSimRead, "simulacao.ler", "Ler a simulação",
         "Ver o estado da simulação: veículos, velocidades e fluxos por via, cruzamentos e indicadores.", false},
        {kPermMapDraw, "mapa.desenhar", "Desenhar no mapa", "Mostrar camadas próprias: linhas, marcadores, áreas e cores nas vias.",
         false},
        {kPermMapTools, "mapa.ferramentas", "Ferramentas no mapa", "Receber toques e arrastes no mapa enquanto a ferramenta dela estiver ativa.",
         false},
        {kPermScenarioPropose, "cenario.propor", "Propor mudanças no cenário",
         "Sugerir mudanças (semáforos, vias, ocorrências...) que só valem depois que você confirmar.", false},
        {kPermScenarioWrite, "cenario.alterar", "Alterar o cenário",
         "Aplicar mudanças no cenário sem pedir confirmação a cada vez (sempre com desfazer).", true},
        {kPermVehicles, "veiculos.controlar", "Controlar veículos", "Criar veículos próprios na simulação, enviá-los por rotas e removê-los.",
         true},
        {kPermSimExperiment, "simulacao.ensaiar", "Ensaiar na simulação",
         "Rodar cópias da simulação em segundo plano para comparar alternativas (usa processador e memória).", true},
        {kPermFilesExport, "arquivos.exportar", "Salvar arquivos", "Oferecer arquivos para você salvar, sempre pelo diálogo do sistema.", false},
        {kPermFilesImport, "arquivos.importar", "Abrir arquivos", "Ler arquivos que você escolher no diálogo do sistema.", true},
    };
    return t;
}

const PermissionInfo* permissionInfo(Permission p) {
    for (const auto& i : permissionTable())
        if (i.bit == p) return &i;
    return nullptr;
}

const PermissionInfo* permissionByKey(std::string_view key) {
    for (const auto& i : permissionTable())
        if (key == i.key) return &i;
    return nullptr;
}

std::vector<Permission> permissionList(uint32_t mask) {
    std::vector<Permission> out;
    for (const auto& i : permissionTable())
        if (mask & i.bit) out.push_back(i.bit);
    return out;
}

namespace {
struct KindKey {
    ContributionKind kind;
    const char* key;
};
const KindKey kKinds[] = {
    {ContributionKind::Panel, "painel"},       {ContributionKind::Tool, "ferramenta"}, {ContributionKind::Layer, "camada"},
    {ContributionKind::Chart, "grafico"},      {ContributionKind::Kpi, "kpi"},         {ContributionKind::Card, "cartao"},
    {ContributionKind::Solver, "resolvedor"}, {ContributionKind::Problem, "problema"}, {ContributionKind::Analysis, "analise"},
};

bool validLocalId(std::string_view s) {
    if (s.empty() || s.size() > 32) return false;
    for (char c : s)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
    return s.front() != '-' && s.back() != '-';
}

bool containsMobilog(std::string_view s) {
    std::string low(s);
    for (char& c : low)
        if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    return low.find("mobilog") != std::string::npos;
}

bool reservedId(std::string_view id) {
    return id.rfind("mobilog.", 0) == 0 || id.rfind("io.github.edcarllos.", 0) == 0 || id == "io.github.edcarllos";
}
}  // namespace

const char* contributionKindKey(ContributionKind k) {
    for (const auto& kk : kKinds)
        if (kk.kind == k) return kk.key;
    return "?";
}

const Contribution* Manifest::contribution(ContributionKind k, std::string_view cid) const {
    for (const auto& c : contributes)
        if (c.kind == k && c.id == cid) return &c;
    return nullptr;
}

bool parseSemVer(std::string_view s, SemVer& out) {
    out = {};
    int* parts[3] = {&out.major, &out.minor, &out.patch};
    size_t i = 0;
    for (int k = 0; k < 3; ++k) {
        if (i >= s.size() || s[i] < '0' || s[i] > '9') return false;
        if (s[i] == '0' && i + 1 < s.size() && s[i + 1] >= '0' && s[i + 1] <= '9') return false;  // no leading zeros
        long v = 0;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
            v = v * 10 + (s[i++] - '0');
            if (v > 99999) return false;
        }
        *parts[k] = int(v);
        if (k < 2) {
            if (i >= s.size() || s[i] != '.') return false;
            ++i;
        }
    }
    if (i == s.size()) return true;
    if (s[i] != '-' || i + 1 == s.size() || s.size() - i > 33) return false;
    out.pre = std::string(s.substr(i + 1));
    for (char c : out.pre)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-')) return false;
    return true;
}

int compareSemVer(const SemVer& a, const SemVer& b) {
    if (a.major != b.major) return a.major < b.major ? -1 : 1;
    if (a.minor != b.minor) return a.minor < b.minor ? -1 : 1;
    if (a.patch != b.patch) return a.patch < b.patch ? -1 : 1;
    if (a.pre == b.pre) return 0;
    if (a.pre.empty()) return 1;  // the release after its pre-releases
    if (b.pre.empty()) return -1;
    return a.pre < b.pre ? -1 : 1;
}

int compareVersions(std::string_view a, std::string_view b) {
    SemVer va, vb;
    const bool oa = parseSemVer(a, va), ob = parseSemVer(b, vb);
    if (!oa || !ob) return oa == ob ? 0 : (oa ? 1 : -1);
    return compareSemVer(va, vb);
}

bool validExtensionId(std::string_view id) {
    if (id.size() < 3 || id.size() > 64) return false;
    int parts = 0;
    size_t start = 0;
    while (start <= id.size()) {
        const size_t end = std::min(id.find('.', start), id.size());
        const std::string_view part = id.substr(start, end - start);
        if (part.empty() || part.front() == '-' || part.back() == '-') return false;
        for (char c : part)
            if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
        ++parts;
        start = end + 1;
    }
    return parts >= 2;
}

bool parseManifest(std::string_view text, const ManifestOptions& opt, Manifest& m, std::vector<std::string>& problems) {
    m = {};
    problems.clear();
    json j;
    try {
        j = json::parse(text);
    } catch (const std::exception&) {
        problems.push_back("manifest.json não é um JSON válido");
        return false;
    }
    if (!j.is_object()) {
        problems.push_back("manifest.json deve ser um objeto");
        return false;
    }
    const auto str = [&](const char* key, std::string& out, bool required, size_t maxChars, bool multiline = false) {
        const auto it = j.find(key);
        if (it == j.end() || it->is_null()) {
            if (required) problems.push_back(std::string(key) + ": obrigatório");
            return false;
        }
        if (!it->is_string()) {
            problems.push_back(std::string(key) + ": deve ser texto");
            return false;
        }
        const std::string raw = it->get<std::string>();
        if (!validUtf8(raw)) {
            problems.push_back(std::string(key) + ": texto inválido");
            return false;
        }
        out = maxChars ? sanitizeText(raw, maxChars, multiline) : raw;
        if (required && out.empty()) {
            problems.push_back(std::string(key) + ": vazio");
            return false;
        }
        return true;
    };

    if (str("id", m.id, true, 0) && !validExtensionId(m.id))
        problems.push_back("id: use letras minúsculas, números e '-', em partes separadas por ponto (ex.: br.ufsb.minha-extensao)");
    if (!opt.builtin && reservedId(m.id)) problems.push_back("id: reservado para as extensões do Mobilog");
    if (str("name", m.name, true, 40) && !opt.builtin && containsMobilog(m.name))
        problems.push_back("name: o nome \"Mobilog\" é reservado");
    for (auto it = j.begin(); it != j.end(); ++it) {
        const std::string& k = it.key();
        if (k.rfind("name@", 0) != 0) continue;
        const std::string lang = k.substr(5);
        bool okLang = lang.size() >= 2 && lang.size() <= 8;
        for (char c : lang) okLang = okLang && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-');
        std::string v;
        if (!okLang) problems.push_back(k + ": idioma inválido");
        else if (str(k.c_str(), v, false, 40) && !v.empty()) {
            if (!opt.builtin && containsMobilog(v)) problems.push_back(k + ": o nome \"Mobilog\" é reservado");
            m.names[lang] = v;
        }
    }
    SemVer sv;
    if (str("version", m.version, true, 0) && !parseSemVer(m.version, sv)) problems.push_back("version: use o formato 1.2.3");
    str("description", m.description, false, 400, true);
    str("author", m.author, false, 60);
    str("license", m.license, false, 40);
    if (str("homepage", m.homepage, false, 0) &&
        (m.homepage.rfind("https://", 0) != 0 || m.homepage.size() > 200 || m.homepage.find_first_of(" \"<>") != std::string::npos)) {
        problems.push_back("homepage: deve ser um endereço https://");
        m.homepage.clear();
    }

    if (const auto it = j.find("apiVersion"); it == j.end()) problems.push_back("apiVersion: obrigatório");
    else if (!it->is_number_integer() || it->get<long long>() < 1) problems.push_back("apiVersion: deve ser um inteiro ≥ 1");
    else if (it->get<long long>() > kApiVersion) problems.push_back("requer uma versão mais nova do Mobilog (API " + it->dump() + ")");
    else m.apiVersion = it->get<int>();
    if (str("minAppVersion", m.minAppVersion, false, 0)) {
        if (!parseSemVer(m.minAppVersion, sv)) problems.push_back("minAppVersion: use o formato 1.2.3");
        else if (!opt.appVersion.empty() && compareVersions(opt.appVersion, m.minAppVersion) < 0)
            problems.push_back("requer o Mobilog " + m.minAppVersion + " ou mais novo");
    }

    if (str("entry", m.entry, true, 0)) {
        if (m.entry == "nativa") {
            if (!opt.builtin) problems.push_back("entry: \"nativa\" só vale para as extensões embutidas");
        } else {
            std::string err;
            if (!validMemberName(m.entry, &err) || m.entry.size() < 6 || m.entry.compare(m.entry.size() - 5, 5, ".wasm") != 0)
                problems.push_back("entry: deve ser um arquivo .wasm do pacote");
        }
    }
    const auto member = [&](const char* key, std::string& out, std::initializer_list<const char*> exts) {
        if (!str(key, out, false, 0)) return;
        std::string err;
        bool okExt = false;
        for (const char* e : exts) {
            const size_t n = std::char_traits<char>::length(e);
            okExt = okExt || (out.size() > n && out.compare(out.size() - n, n, e) == 0);
        }
        if (!validMemberName(out, &err) || !okExt) {
            problems.push_back(std::string(key) + ": arquivo inválido");
            out.clear();
        }
    };
    member("icon", m.icon, {".svg", ".png"});
    member("help", m.help, {".md"});

    if (const auto it = j.find("permissions"); it != j.end()) {
        if (!it->is_array()) problems.push_back("permissions: deve ser uma lista");
        else
            for (const auto& p : *it) {
                const PermissionInfo* info = p.is_string() ? permissionByKey(p.get<std::string>()) : nullptr;
                if (!info) problems.push_back("permissions: '" + (p.is_string() ? sanitizeText(p.get<std::string>(), 40) : p.dump()) + "' não existe");
                else m.permissions |= info->bit;
            }
    }

    if (const auto it = j.find("contributes"); it != j.end()) {
        if (!it->is_array()) problems.push_back("contributes: deve ser uma lista");
        else if (it->size() > 64) problems.push_back("contributes: itens demais (máximo 64)");
        else {
            std::set<std::pair<int, std::string>> seen;
            for (const auto& c : *it) {
                if (!c.is_object() || !c.contains("kind") || !c["kind"].is_string() || !c.contains("id") || !c["id"].is_string()) {
                    problems.push_back("contributes: cada item precisa de \"kind\" e \"id\"");
                    continue;
                }
                Contribution con;
                const std::string kind = c["kind"].get<std::string>();
                bool known = false;
                for (const auto& kk : kKinds)
                    if (kind == kk.key) con.kind = kk.kind, known = true;
                con.id = c["id"].get<std::string>();
                if (!known) {
                    problems.push_back("contributes: tipo '" + sanitizeText(kind, 24) + "' não existe");
                    continue;
                }
                if (!validLocalId(con.id)) {
                    problems.push_back("contributes: id '" + sanitizeText(con.id, 32) + "' inválido (use a-z, 0-9 e '-')");
                    continue;
                }
                if (!seen.insert({int(con.kind), con.id}).second) {
                    problems.push_back("contributes: '" + kind + ":" + con.id + "' repetido");
                    continue;
                }
                if (c.contains("title") && c["title"].is_string()) con.title = sanitizeText(c["title"].get<std::string>(), 40);
                if (con.title.empty()) con.title = m.name;
                if (c.contains("icon") && c["icon"].is_string()) {
                    con.icon = c["icon"].get<std::string>();
                    if (!validLocalId(con.icon)) con.icon.clear();
                }
                m.contributes.push_back(std::move(con));
            }
        }
    }

    if (const auto it = j.find("limits"); it != j.end() && it->is_object() && it->contains("memoryMB")) {
        const auto& v = (*it)["memoryMB"];
        if (!v.is_number_integer() || v.get<long long>() < 1 || v.get<long long>() > kMaxMemoryMB)
            problems.push_back("limits.memoryMB: de 1 a " + std::to_string(kMaxMemoryMB));
        else m.memoryMB = v.get<uint32_t>();
    }
    return problems.empty();
}

std::string manifestJson(const Manifest& m) {
    json j;
    j["id"] = m.id;
    j["name"] = m.name;
    for (const auto& [lang, n] : m.names) j["name@" + lang] = n;
    j["version"] = m.version;
    j["apiVersion"] = m.apiVersion;
    if (!m.minAppVersion.empty()) j["minAppVersion"] = m.minAppVersion;
    if (!m.description.empty()) j["description"] = m.description;
    if (!m.author.empty()) j["author"] = m.author;
    if (!m.license.empty()) j["license"] = m.license;
    if (!m.homepage.empty()) j["homepage"] = m.homepage;
    j["entry"] = m.entry;
    if (!m.icon.empty()) j["icon"] = m.icon;
    if (!m.help.empty()) j["help"] = m.help;
    json perms = json::array();
    for (Permission p : permissionList(m.permissions)) perms.push_back(permissionInfo(p)->key);
    j["permissions"] = perms;
    json con = json::array();
    for (const auto& c : m.contributes) {
        json o = {{"kind", contributionKindKey(c.kind)}, {"id", c.id}, {"title", c.title}};
        if (!c.icon.empty()) o["icon"] = c.icon;
        con.push_back(o);
    }
    j["contributes"] = con;
    j["limits"] = {{"memoryMB", m.memoryMB}};
    return j.dump(2);
}

}  // namespace mobilog::ext
