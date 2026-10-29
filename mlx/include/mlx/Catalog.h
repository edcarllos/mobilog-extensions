#pragma once
// The extension catalog a mobilog_server publishes (/ext/catalog.json and /ext/<file>.mlx): the
// packages in its folder, checked as the app checks them, and the examples that come with it. The
// app lists them in Sistema → Extensões → Catálogo and installs through the usual dialog, after
// checking that the downloaded package is the one listed (its SHA-512).
//
// catalog.json = {"version": 1, "server": "<name>", "serverKey": "<public key line or "">",
//   "extensions": [{"id", "name", "version", "description", "author", "permissions": [keys],
//                   "file", "size", "sha512", "signerKey": "<base64 or "">", "examples": bool}]}
#include "mlx/Bytes.h"
#include "mlx/Manifest.h"

#include <string>
#include <string_view>
#include <vector>

namespace mobilog::ext {

struct CatalogEntry {
    std::string id, name, version, description, author;
    uint32_t permissions = 0;
    std::string file;       // under /ext/
    uint64_t size = 0;
    std::string sha512;     // of the package (hex)
    std::string signerKey;  // base64 of the signing key ("" unsigned)
    bool example = false;   // one of the app's own examples (it installs its own copy)
};

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
PublishedCatalog publishCatalog(const std::string& dir, const std::vector<std::pair<std::string, Bytes>>& examples,
                                const std::string& serverName, const std::string& serverKeyText);
bool parseCatalog(std::string_view json, Catalog& out, std::string* error = nullptr);
std::string catalogFileName(const Manifest& m);  // "<id>-<version>.mlx"

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
