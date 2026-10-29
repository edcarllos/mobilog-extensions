// catalog: the store's index (catalog.json) from the packages in a folder, signed (catalog.json.sig)
// with the store's key. The app reads both and installs a package only when it is the one listed.
#include "Tool.h"

#include "mlx/Catalog.h"

#include <cstdio>

namespace mlxtool {

using namespace mobilog::ext;

namespace {
// The date of the last commit that touched extensions/<id> ("2026-10-29"), "" when unknown.
std::string gitDate(const std::string& repo, const std::string& id) {
#if defined(_WIN32)
    (void)repo, (void)id;
    return {};
#else
    for (char c : id)
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-')) return {};  // validExtensionId already
    const std::string cmd = "git -C '" + repo + "' log -1 --format=%cs -- 'extensions/" + id + "' 2>/dev/null";
    FILE* p = popen(cmd.c_str(), "r");
    if (!p) return {};
    char buf[64] = {};
    const size_t n = std::fread(buf, 1, sizeof buf - 1, p);
    pclose(p);
    std::string s(buf, n);
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
    return s.size() == 10 ? s : std::string();
#endif
}
}  // namespace

int cmdCatalog(const Args& a) {
    if (a.positional.size() != 1) return usage();
    const fs::path dir = a.positional[0];
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) {
        std::fprintf(stderr, "%s: %s\n", tr("erro: não é uma pasta", "error: not a folder"), dir.string().c_str());
        return 1;
    }
    SigningKey key;
    const bool sign = !a.key.empty();
    if (sign && !loadSecret(a.key, key)) return 1;
    const std::string name = a.name.empty() ? "Loja Mobilog" : a.name;
    std::function<std::string(const CatalogEntry&)> updated;
    if (!a.updatedFromGit.empty()) updated = [&](const CatalogEntry& e) { return gitDate(a.updatedFromGit, e.id); };
    const PublishedCatalog pc = publishCatalog(dir.string(), {}, name, sign ? publicKeyText(key.pub, name) : std::string(), updated);
    for (const auto& p : pc.problems) std::fprintf(stderr, "%s: %s\n", tr("erro", "error"), p.c_str());
    if (!pc.problems.empty()) return 1;
    if (!writeFile(dir / "catalog.json", pc.json.data(), pc.json.size())) return 1;
    if (sign) {
        const std::string sig = signDetached(pc.json, key);
        if (!writeFile(dir / "catalog.json.sig", sig.data(), sig.size())) return 1;
    }
    if (!a.quiet)
        std::printf(tr("%s: %zu extensões%s\n", "%s: %zu extensions%s\n"), (dir / "catalog.json").string().c_str(), pc.packages.size(),
                    sign ? tr(", assinado", ", signed") : tr(" (sem assinatura)", " (unsigned)"));
    return 0;
}

}  // namespace mlxtool
