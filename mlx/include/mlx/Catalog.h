#pragma once
// An extension catalog: the Mobilog store (mobilog_store/catalog.json, made by `mobilog_ext catalog`,
// signed in catalog.json.sig) or a mobilog_server's (/ext/catalog.json). The packages sit next to
// it (`file`, a plain name). The app lists them and installs through the usual dialog, after
// checking that the downloaded package is the one listed (its SHA-512).
//
// catalog.json = {"version": 1, "server": "<name>", "serverKey": "<public key line or "">",
//   "extensions": [{"id", "name", "version", "description", "author", "permissions": [keys],
//                   "file", "size", "sha512", "signerKey": "<base64 or "">", "example": bool,
//                   optional, for the store: "summary", "category", "keywords", "license",
//                   "homepage", "repository", "minAppVersion", "apiVersion", "updated",
//                   "icon" (the SVG itself), "screenshots" (how many), "name@<lang>",
//                   "summary@<lang>", "description@<lang>"}]}
// Apps before 0.17 read the same "version": 1 and leave out what they do not know.
#include "mlx/Bytes.h"
#include "mlx/Manifest.h"

#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace mobilog::ext {

struct CatalogEntry {
    std::string id, name, version, description, author;
    uint32_t permissions = 0;
    std::string file;       // next to catalog.json
    uint64_t size = 0;
    std::string sha512;     // of the package (hex)
    std::string signerKey;  // base64 of the signing key ("" unsigned)
    bool example = false;   // one of the Mobilog's own (mlx/Official.h)
    // For the store.
    std::string summary, category, license, homepage, repository, minAppVersion, updated;
    int apiVersion = 1;
    std::string icon;  // SVG text ("" none)
    int screenshots = 0;
    std::vector<std::string> keywords;
    std::map<std::string, std::string> names, summaries, descriptions;  // by language
};
constexpr size_t kMaxCatalogIcon = 32 * 1024;  // bytes of SVG

struct Catalog {
    std::string server;     // the server's name
    std::string serverKey;  // its public key ("mobilog-ext-key-v1 ..."; "" none)
    std::vector<CatalogEntry> entries;
};

// One package a server serves: from its folder (path) or from memory (the examples).
struct PublishedPackage {
    CatalogEntry entry;
    std::string path;
    Bytes data;
};
struct PublishedCatalog {
    std::string json;
    std::vector<PublishedPackage> packages;
    std::vector<std::string> problems;  // files left out, and why
};

// The .mlx files in `dir` (not the reserved "mobilog.*" ids, but the Mobilog's own examples, known by
// their bytes: mlx/Official.h) and `examples` (name, bytes).
// `updatedOf`: the date each package was last changed ("2026-10-29"), when the publisher knows it.
PublishedCatalog publishCatalog(const std::string& dir, const std::vector<std::pair<std::string, Bytes>>& examples,
                                const std::string& serverName, const std::string& serverKeyText,
                                const std::function<std::string(const CatalogEntry&)>& updatedOf = {});
// Several folders (a server's own and its copy of the store): one entry per id, the newest version; at
// the same version, the earlier folder's.
PublishedCatalog publishCatalog(const std::vector<std::string>& dirs, const std::string& serverName, const std::string& serverKeyText);
std::string catalogJson(const Catalog& c);  // what publishCatalog writes, from entries
bool parseCatalog(std::string_view json, Catalog& out, std::string* error = nullptr);
std::string catalogFileName(const Manifest& m);  // "<id>-<version>.mlx"
// Where a listed package is: next to the index ("https://x/loja/catalog.json?t=1" + "a-1.0.0.mlx"
// = "https://x/loja/a-1.0.0.mlx"). "" when `file` is not a plain name (parseCatalog leaves those out).
std::string catalogPackageUrl(std::string_view catalogUrl, std::string_view file);

struct SigningKey;
// Puts a package in a catalog folder (checked as the app checks it; older versions of the same
// extension leave). `signWith`: signs a package that came unsigned (an author's signature stays).
// False with the reasons in `error` (Portuguese) when the package is not valid.
// `replaceOlder` false (a folder of the user's): nothing else in it is touched, the package is copied in.
bool addToCatalog(const std::string& dir, Bytes mlx, const SigningKey* signWith, Manifest* added, std::string* error,
                  bool* signedNow = nullptr, bool replaceOlder = true);
// Takes an extension out of a catalog folder: how many files left.
int removeFromCatalog(const std::string& dir, const std::string& id);

}  // namespace mobilog::ext
