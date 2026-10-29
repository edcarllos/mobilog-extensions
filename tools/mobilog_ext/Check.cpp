// check: everything the store checks before a package goes in, on a source folder (built first)
// or on a package. Beyond the app's own checks (the manifest, the module, the signature): what a
// store page needs (an icon that draws, a summary, a category, help, a license, news for this
// version, small screenshots) and, against the store, a version that was not published yet.
#include "Tool.h"

#include "mlx/Catalog.h"

#include <nlohmann/json.hpp>

#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#include <nanosvg.h>
#include <nanosvgrast.h>

#include <cstdio>
#include <functional>

namespace mlxtool {

using namespace mobilog::ext;

namespace {

constexpr size_t kMaxScreenshotBytes = 300 * 1024;

struct Findings {
    std::vector<std::string> errors, warnings;
    void error(std::string s) { errors.push_back(std::move(s)); }
    void warn(std::string s) { warnings.push_back(std::move(s)); }
};

// The icon must draw: parsed by nanosvg (the app's renderer), with something visible at 48 px.
void checkIcon(const std::string& name, const Bytes& data, Findings& f) {
    if (name.size() < 4 || name.compare(name.size() - 4, 4, ".svg") != 0) {
        f.error(tr("icon: a loja pede um ícone .svg (vetorial: nítido em qualquer tamanho)", "icon: the store needs an .svg icon (vector: sharp at any size)"));
        return;
    }
    if (data.size() > kMaxCatalogIcon) {
        f.error(std::string(tr("icon: grande demais (máximo ", "icon: too big (at most ")) + std::to_string(kMaxCatalogIcon / 1024) + " KB)");
        return;
    }
    std::string text(data.begin(), data.end());
    if (text.find("<svg") == std::string::npos) {
        f.error(tr("icon: não é um SVG", "icon: not an SVG"));
        return;
    }
    if (text.find("<image") != std::string::npos || text.find("<text") != std::string::npos || text.find("<script") != std::string::npos)
        f.warn(tr("icon: <image>, <text> e <script> não são desenhados pelo app; use só formas", "icon: <image>, <text> and <script> are not drawn by the app; use shapes only"));
    NSVGimage* img = nsvgParse(text.data(), "px", 96.0f);
    if (!img || !img->shapes || img->width <= 0 || img->height <= 0) {
        if (img) nsvgDelete(img);
        f.error(tr("icon: o SVG não tem formas que o app consiga desenhar (confira width/height ou viewBox)",
                   "icon: the SVG has no shapes the app can draw (check width/height or viewBox)"));
        return;
    }
    const float ratio = img->width / img->height;
    if (ratio < 0.8f || ratio > 1.25f) f.warn(tr("icon: o ícone deveria ser quadrado", "icon: the icon should be square"));
    NSVGrasterizer* r = nsvgCreateRasterizer();
    std::vector<unsigned char> px(48 * 48 * 4, 0);
    nsvgRasterize(r, img, 0, 0, 48.0f / std::max(img->width, img->height), px.data(), 48, 48, 48 * 4);
    nsvgDeleteRasterizer(r);
    nsvgDelete(img);
    size_t visible = 0;
    for (size_t i = 3; i < px.size(); i += 4) visible += px[i] > 32;
    if (visible < 48 * 48 / 20) f.error(tr("icon: quase nada aparece em 48 px", "icon: almost nothing shows at 48 px"));
}

bool imageMagic(const Bytes& b) {
    static const uint8_t png[] = {0x89, 'P', 'N', 'G'};
    if (b.size() >= 4 && std::equal(png, png + 4, b.begin())) return true;
    return b.size() >= 3 && b[0] == 0xFF && b[1] == 0xD8 && b[2] == 0xFF;  // JPEG
}

// What a store page needs, read through `read` (a folder's files or a package's members).
void storeRules(const Manifest& m, const std::function<bool(const std::string&, Bytes&)>& read, Findings& f) {
    if (m.summary.empty()) f.error(tr("summary: obrigatório na loja (uma linha, até 80 letras)", "summary: required by the store (one line, up to 80 characters)"));
    if (m.description.empty()) f.warn(tr("description: recomendada (o que faz, para quem)", "description: recommended (what it does, for whom)"));
    if (m.category.empty()) f.error(tr("category: obrigatória na loja", "category: required by the store"));
    if (m.author.empty()) f.error(tr("author: obrigatório na loja", "author: required by the store"));
    if (m.license.empty()) f.error(tr("license: obrigatória na loja (ex.: MIT)", "license: required by the store (e.g. MIT)"));
    if (m.help.empty()) f.error(tr("help: obrigatória na loja (um help.md)", "help: required by the store (a help.md)"));
    if (m.names.find("en") == m.names.end()) f.warn(tr("name@en: recomendado (o app também é usado em inglês)", "name@en: recommended"));
    if (m.summaries.find("en") == m.summaries.end()) f.warn(tr("summary@en: recomendado", "summary@en: recommended"));
    if (m.helps.find("en") == m.helps.end()) f.warn(tr("help@en: recomendada", "help@en: recommended"));
    Bytes data;
    if (m.icon.empty()) f.error(tr("icon: obrigatório na loja (icon.svg)", "icon: required by the store (icon.svg)"));
    else if (read(m.icon, data)) checkIcon(m.icon, data, f);
    if (m.changelog.empty()) {
        f.error(tr("changelog: obrigatório na loja (CHANGELOG.md, com uma seção \"## <versão>\")",
                   "changelog: required by the store (CHANGELOG.md, with a \"## <version>\" section)"));
    } else if (read(m.changelog, data)) {
        const std::string text(data.begin(), data.end());
        if (text.find("## " + m.version) == std::string::npos && text.find("## [" + m.version + "]") == std::string::npos)
            f.error(m.changelog + tr(": falta a seção \"## ", ": missing the section \"## ") + m.version + "\"");
    }
    for (const std::string& s : m.screenshots)
        if (read(s, data)) {
            if (data.size() > kMaxScreenshotBytes)
                f.error(s + tr(": grande demais (máximo 300 KB; reduza ou use JPG)", ": too big (at most 300 KB; shrink it or use JPG)"));
            if (!imageMagic(data)) f.error(s + tr(": não é PNG nem JPG", ": neither PNG nor JPG"));
        }
}

// Against the store's index: a version that was not published yet.
void storeVersion(const Args& a, const Manifest& m, Findings& f) {
    if (a.store.empty()) return;
    std::string text;
    if (!readText(fs::path(a.store) / "catalog.json", text)) {
        f.warn(tr("--store: catalog.json não encontrado", "--store: catalog.json not found"));
        return;
    }
    Catalog c;
    std::string err;
    if (!parseCatalog(text, c, &err)) {
        f.error("--store: " + err);
        return;
    }
    for (const CatalogEntry& e : c.entries) {
        if (e.id != m.id) continue;
        const int cmp = compareVersions(m.version, e.version);
        if (cmp < 0) f.error(tr("version: menor que a publicada (", "version: lower than the published one (") + e.version + ")");
        else if (cmp == 0) {
            const std::string msg = tr("version: ", "version: ") + m.version + tr(" já está na loja; suba a versão (e o CHANGELOG) para publicar a mudança",
                                                                                   " is already in the store; raise the version (and the CHANGELOG) to publish the change");
            if (a.requireBump) f.error(msg);
            else f.warn(msg);
        }
    }
}

int finish(const Findings& f, const std::string& what) {
    for (const auto& w : f.warnings) std::fprintf(stderr, "%s: %s\n", tr("aviso", "warning"), w.c_str());
    for (const auto& e : f.errors) std::fprintf(stderr, "%s: %s\n", tr("erro", "error"), e.c_str());
    if (!f.errors.empty()) {
        std::fprintf(stderr, "%s: %zu %s\n", what.c_str(), f.errors.size(), tr("problema(s): não entra na loja", "problem(s): not accepted by the store"));
        return 1;
    }
    std::printf("%s: %s%s\n", what.c_str(), tr("pronto para a loja", "ready for the store"),
                f.warnings.empty() ? "" : tr(" (veja os avisos)", " (see the warnings)"));
    return 0;
}

}  // namespace

int cmdCheck(const Args& a) {
    if (a.positional.size() > 1) return usage();
    const fs::path target = a.positional.empty() ? fs::current_path() : fs::path(a.positional[0]);
    std::error_code ec;
    Findings f;
    if (fs::is_regular_file(target, ec)) {  // a package
        Bytes mlx;
        if (!readFile(target, mlx)) return 1;
        ZipReader zip;
        const PackageCheck c = checkPackage(mlx, {"", a.mobilog || a.official}, zip);
        for (const auto& p : c.problems) f.error(p);
        if (c.ok) {
            storeRules(c.manifest, [&](const std::string& n, Bytes& out) { return zip.read(n, out, nullptr); }, f);
            storeVersion(a, c.manifest, f);
        }
        return finish(f, target.filename().string());
    }
    // A source folder: built (as the store will), then the same checks on the package, plus the folder's own.
    Args quiet = a;
    quiet.quiet = true;
    Built b;
    if (!buildFolder(quiet, target, b)) return 1;
    const fs::path dir = fs::absolute(target, ec);
    if (dir.parent_path().filename() == "extensions" && dir.filename().string() != b.manifest.id)
        f.error(tr("a pasta deve ter o nome do id: extensions/", "the folder must be named after the id: extensions/") + b.manifest.id);
    if (!fs::is_regular_file(dir / "README.md", ec)) f.warn(tr("README.md: recomendado (o que é, como usar, como compilar)", "README.md: recommended"));
    Bytes mlx;
    readFile(b.mlx, mlx);
    ZipReader zip;
    const PackageCheck c = checkPackage(mlx, {"", true}, zip);
    for (const auto& p : c.problems) f.error(p);
    storeRules(b.manifest, [&](const std::string& n, Bytes& out) { return zip.read(n, out, nullptr); }, f);
    storeVersion(a, b.manifest, f);
    std::printf("%s %s · %.1f KB\n", b.manifest.id.c_str(), b.manifest.version.c_str(), double(mlx.size()) / 1024.0);
    return finish(f, b.manifest.id);
}

}  // namespace mlxtool
