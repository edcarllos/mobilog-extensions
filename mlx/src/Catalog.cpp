#include "mlx/Catalog.h"

#include "mlx/Official.h"
#include "mlx/Package.h"
#include "mlx/Signature.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace mobilog::ext {

namespace fs = std::filesystem;
using json = nlohmann::json;

std::string catalogFileName(const Manifest& m) { return m.id + "-" + m.version + ".mlx"; }

namespace {
// A file name of our own making only: no folders, no traversal, no scheme.
bool plainFileName(std::string_view f) {
    return !f.empty() && f.size() < 160 && f.find_first_of("/\\:?#") == std::string_view::npos && f.find("..") == std::string_view::npos;
}
}  // namespace

std::string catalogPackageUrl(std::string_view catalogUrl, std::string_view file) {
    if (!plainFileName(file)) return {};
    const size_t end = catalogUrl.find_first_of("?#");
    std::string_view base = catalogUrl.substr(0, end);
    const size_t slash = base.rfind('/');
    base = slash == std::string_view::npos ? std::string_view() : base.substr(0, slash + 1);
    return std::string(base) + std::string(file);
}

namespace {

bool entryOf(const Bytes& mlx, bool example, CatalogEntry& e, std::string& problem) {
    example = example || officialExample(mlx);  // one of the Mobilog's, put in the folder
    ZipReader zip;
    const PackageCheck c = checkPackage(mlx, {"", example}, zip);
    if (!c.ok) {
        problem = c.problems.empty() ? "pacote inválido" : c.problems.front();
        return false;
    }
    const Manifest& m = c.manifest;
    e.id = m.id;
    e.name = m.name;
    e.version = m.version;
    e.description = m.description;
    e.author = m.author;
    e.permissions = m.permissions;
    e.file = catalogFileName(m);
    e.size = c.size;
    e.sha512 = c.sha512;
    e.signerKey = c.signature.status == SigStatus::Valid ? toBase64(c.signature.key.data(), c.signature.key.size()) : std::string();
    e.example = example;
    e.summary = m.summary;
    e.category = m.category;
    e.license = m.license;
    e.homepage = m.homepage;
    e.repository = m.repository;
    e.minAppVersion = m.minAppVersion;
    e.apiVersion = m.apiVersion;
    e.keywords = m.keywords;
    e.names = m.names;
    e.summaries = m.summaries;
    e.descriptions = m.descriptions;
    e.screenshots = int(m.screenshots.size());
    Bytes icon;
    std::string err;
    if (m.icon.size() > 4 && m.icon.compare(m.icon.size() - 4, 4, ".svg") == 0 && zip.read(m.icon, icon, &err) &&
        icon.size() <= kMaxCatalogIcon)
        e.icon.assign(icon.begin(), icon.end());
    return true;
}

json entryJson(const CatalogEntry& e) {
    json perms = json::array();
    for (Permission perm : permissionList(e.permissions)) perms.push_back(permissionInfo(perm)->key);
    json o = {{"id", e.id}, {"name", e.name}, {"version", e.version}, {"description", e.description},
              {"author", e.author}, {"permissions", perms}, {"file", e.file}, {"size", e.size},
              {"sha512", e.sha512}, {"signerKey", e.signerKey}, {"example", e.example}};
    const auto opt = [&](const char* k, const std::string& v) {
        if (!v.empty()) o[k] = v;
    };
    opt("summary", e.summary);
    opt("category", e.category);
    opt("license", e.license);
    opt("homepage", e.homepage);
    opt("repository", e.repository);
    opt("minAppVersion", e.minAppVersion);
    opt("updated", e.updated);
    opt("icon", e.icon);
    if (e.apiVersion != 1) o["apiVersion"] = e.apiVersion;
    if (e.screenshots) o["screenshots"] = e.screenshots;
    if (!e.keywords.empty()) o["keywords"] = e.keywords;
    for (const auto& [lang, v] : e.names) o["name@" + lang] = v;
    for (const auto& [lang, v] : e.summaries) o["summary@" + lang] = v;
    for (const auto& [lang, v] : e.descriptions) o["description@" + lang] = v;
    return o;
}

}  // namespace

std::string catalogJson(const Catalog& c) {
    json j = {{"version", 1}, {"server", c.server}, {"serverKey", c.serverKey}, {"extensions", json::array()}};
    for (const CatalogEntry& e : c.entries) j["extensions"].push_back(entryJson(e));
    return j.dump(1);
}

PublishedCatalog publishCatalog(const std::string& dir, const std::vector<std::pair<std::string, Bytes>>& examples,
                                const std::string& serverName, const std::string& serverKeyText,
                                const std::function<std::string(const CatalogEntry&)>& updatedOf) {
    PublishedCatalog out;
    std::error_code ec;
    std::vector<fs::path> files;
    if (!dir.empty())
        for (const auto& it : fs::directory_iterator(dir, ec))
            if (it.is_regular_file(ec) && it.path().extension() == ".mlx") files.push_back(it.path());
    std::sort(files.begin(), files.end());
    for (const fs::path& p : files) {
        if (fs::file_size(p, ec) > kMaxPackageBytes) {
            out.problems.push_back(p.filename().string() + ": grande demais");
            continue;
        }
        std::ifstream f(p, std::ios::binary);
        const Bytes data((std::istreambuf_iterator<char>(f)), {});
        PublishedPackage pkg;
        std::string problem;
        if (!entryOf(data, false, pkg.entry, problem)) {
            out.problems.push_back(p.filename().string() + ": " + problem);
            continue;
        }
        pkg.path = p.string();
        out.packages.push_back(std::move(pkg));
    }
    for (const auto& [name, data] : examples) {
        PublishedPackage pkg;
        std::string problem;
        if (!entryOf(data, true, pkg.entry, problem)) {
            out.problems.push_back(name + ": " + problem);
            continue;
        }
        pkg.data = data;
        out.packages.push_back(std::move(pkg));
    }
    // One entry per id: the newest version (several versions of one package can sit in the folder).
    std::vector<PublishedPackage> unique;
    for (PublishedPackage& p : out.packages) {
        auto same = std::find_if(unique.begin(), unique.end(), [&](const PublishedPackage& u) { return u.entry.id == p.entry.id; });
        if (same == unique.end()) unique.push_back(std::move(p));
        else if (compareVersions(p.entry.version, same->entry.version) > 0) *same = std::move(p);
    }
    out.packages = std::move(unique);
    // In a fixed order (the store's index changes only where a package did).
    std::sort(out.packages.begin(), out.packages.end(),
              [](const PublishedPackage& a, const PublishedPackage& b) { return a.entry.id < b.entry.id; });
    Catalog c{serverName, serverKeyText, {}};
    for (PublishedPackage& p : out.packages) {
        if (updatedOf) p.entry.updated = sanitizeText(updatedOf(p.entry), 32);
        c.entries.push_back(p.entry);
    }
    out.json = catalogJson(c);
    return out;
}

bool parseCatalog(std::string_view text, Catalog& out, std::string* error) {
    const json j = json::parse(text, nullptr, false);
    const auto fail = [&](const char* why) {
        if (error) *error = why;
        return false;
    };
    if (!j.is_object() || j.value("version", 0) != 1 || !j.contains("extensions") || !j["extensions"].is_array())
        return fail("o catálogo do servidor não está no formato esperado");
    out = {};
    out.server = sanitizeText(j.value("server", ""), 60);
    out.serverKey = j.value("serverKey", "");
    for (const json& x : j["extensions"]) {
        if (!x.is_object()) continue;
        CatalogEntry e;
        e.id = x.value("id", "");
        e.name = sanitizeText(x.value("name", ""), 40);
        e.version = x.value("version", "");
        e.description = sanitizeText(x.value("description", ""), 400, true);
        e.author = sanitizeText(x.value("author", ""), 60);
        e.file = x.value("file", "");
        e.size = x.value("size", uint64_t(0));
        e.sha512 = x.value("sha512", "");
        e.signerKey = x.value("signerKey", "");
        e.example = x.value("example", false);
        const auto text = [&](const char* k, size_t n) {
            return x.contains(k) && x[k].is_string() ? sanitizeText(x[k].get<std::string>(), n) : std::string();
        };
        e.summary = text("summary", 80);
        e.category = categoryByKey(text("category", 24)) ? text("category", 24) : std::string();
        e.license = text("license", 40);
        e.minAppVersion = text("minAppVersion", 24);
        e.updated = text("updated", 32);
        for (const auto& [k, field] : {std::pair{"homepage", &e.homepage}, std::pair{"repository", &e.repository}}) {
            const std::string u = text(k, 200);
            if (u.rfind("https://", 0) == 0 && u.find_first_of(" \"<>") == std::string::npos) *field = u;
        }
        if (x.contains("apiVersion") && x["apiVersion"].is_number_integer()) e.apiVersion = x["apiVersion"].get<int>();
        if (x.contains("screenshots") && x["screenshots"].is_number_integer())
            e.screenshots = std::clamp(x["screenshots"].get<int>(), 0, int(kMaxScreenshots));
        if (x.contains("icon") && x["icon"].is_string()) {
            std::string svg = x["icon"].get<std::string>();
            if (svg.size() <= kMaxCatalogIcon && validUtf8(svg) && svg.find("<svg") != std::string::npos) e.icon = std::move(svg);
        }
        if (x.contains("keywords") && x["keywords"].is_array())
            for (const json& w : x["keywords"])
                if (w.is_string() && e.keywords.size() < kMaxKeywords && validUtf8(w.get<std::string>()))
                    e.keywords.push_back(sanitizeText(w.get<std::string>(), 24));
        for (auto it = x.begin(); it != x.end(); ++it) {
            const std::string& k = it.key();
            const size_t at = k.find('@');
            if (at == std::string::npos || !it->is_string() || k.size() - at - 1 > 8 || !validUtf8(it->get<std::string>())) continue;
            const std::string lang = k.substr(at + 1), base = k.substr(0, at);
            if (base == "name") e.names[lang] = sanitizeText(it->get<std::string>(), 40);
            else if (base == "summary") e.summaries[lang] = sanitizeText(it->get<std::string>(), 80);
            else if (base == "description") e.descriptions[lang] = sanitizeText(it->get<std::string>(), 400, true);
        }
        if (x.contains("permissions") && x["permissions"].is_array())
            for (const json& k : x["permissions"])
                if (k.is_string())
                    if (const PermissionInfo* p = permissionByKey(k.get<std::string>())) e.permissions |= p->bit;
        SemVer v;
        const bool plainFile = plainFileName(e.file);
        if (!validExtensionId(e.id) || !parseSemVer(e.version, v) || !plainFile || e.sha512.size() != 128 || e.size == 0 ||
            e.size > kMaxPackageBytes)
            continue;  // left out: not installable anyway
        out.entries.push_back(std::move(e));
    }
    return true;
}

bool addToCatalog(const std::string& dir, Bytes mlx, const SigningKey* signWith, Manifest* added, std::string* error,
                  bool* signedNow, bool replaceOlder) {
    if (signedNow) *signedNow = false;
    ZipReader zip;
    // One of the Mobilog's examples goes in as it is (never signed again: the apps know it by its bytes).
    const bool official = officialExample(mlx);
    if (official) signWith = nullptr;
    PackageCheck c = checkPackage(mlx, {"", official}, zip);
    if (!c.ok) {
        if (error) {
            *error = "pacote inválido";
            for (const std::string& p : c.problems) *error += "\n" + p;
        }
        return false;
    }
    if (signWith && c.signature.status == SigStatus::Unsigned) {
        std::vector<ZipFile> files;
        for (const ZipEntry& e : zip.entries()) {
            ZipFile f{e.name, {}};
            std::string err;
            if (!zip.read(e, f.data, &err)) {
                if (error) *error = err;
                return false;
            }
            files.push_back(std::move(f));
        }
        mlx = buildPackage(files, signWith);
        if (signedNow) *signedNow = true;
    }
    std::error_code ec;
    fs::create_directories(dir, ec);
    // Older versions of the same extension leave the folder (the catalog offers the newest anyway).
    if (replaceOlder) removeFromCatalog(dir, c.manifest.id);
    const fs::path out = fs::path(dir) / catalogFileName(c.manifest);
    std::ofstream f(out, std::ios::binary);
    f.write(reinterpret_cast<const char*>(mlx.data()), std::streamsize(mlx.size()));
    if (!f) {
        if (error) *error = "não foi possível gravar " + out.string();
        return false;
    }
    if (added) *added = c.manifest;
    return true;
}

int removeFromCatalog(const std::string& dir, const std::string& id) {
    int removed = 0;
    std::error_code ec;
    for (const PublishedPackage& p : publishCatalog(dir, {}, "", "").packages)
        if (p.entry.id == id) removed += fs::remove(p.path, ec) ? 1 : 0;
    return removed;
}

}  // namespace mobilog::ext
