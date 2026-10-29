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

bool entryOf(const Bytes& mlx, bool example, CatalogEntry& e, std::string& problem) {
    example = example || officialExample(mlx);  // one of the Mobilog's, put in the folder
    const PackageCheck c = checkPackage(mlx, {"", example});
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
    return true;
}

}  // namespace

PublishedCatalog publishCatalog(const std::string& dir, const std::vector<std::pair<std::string, Bytes>>& examples,
                                const std::string& serverName, const std::string& serverKeyText) {
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
    json j = {{"version", 1}, {"server", serverName}, {"serverKey", serverKeyText}, {"extensions", json::array()}};
    for (const PublishedPackage& p : out.packages) {
        const CatalogEntry& e = p.entry;
        json perms = json::array();
        for (Permission perm : permissionList(e.permissions)) perms.push_back(permissionInfo(perm)->key);
        j["extensions"].push_back({{"id", e.id}, {"name", e.name}, {"version", e.version}, {"description", e.description},
                                   {"author", e.author}, {"permissions", perms}, {"file", e.file}, {"size", e.size},
                                   {"sha512", e.sha512}, {"signerKey", e.signerKey}, {"example", e.example}});
    }
    out.json = j.dump();
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
        e.description = sanitizeText(x.value("description", ""), 300);
        e.author = sanitizeText(x.value("author", ""), 60);
        e.file = x.value("file", "");
        e.size = x.value("size", uint64_t(0));
        e.sha512 = x.value("sha512", "");
        e.signerKey = x.value("signerKey", "");
        e.example = x.value("example", false);
        if (x.contains("permissions") && x["permissions"].is_array())
            for (const json& k : x["permissions"])
                if (k.is_string())
                    if (const PermissionInfo* p = permissionByKey(k.get<std::string>())) e.permissions |= p->bit;
        SemVer v;
        // A file name of our own making only: no folders, no traversal.
        const bool plainFile = !e.file.empty() && e.file.find('/') == std::string::npos && e.file.find('\\') == std::string::npos &&
                               e.file.find("..") == std::string::npos && e.file.size() < 160;
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
