#pragma once
// An extension package (.mlx) opened and checked as a whole: the zip, the manifest, the module, the
// signature and the files the manifest names. This is what the manager shows before installing and
// what the server lists in its catalog; nothing here runs the extension.
#include "mlx/Manifest.h"
#include "mlx/Signature.h"
#include "mlx/WasmInspect.h"

#include <string>
#include <vector>

namespace mobilog::ext {

constexpr const char* kManifestFile = "manifest.json";
constexpr size_t kMaxPackageBytes = 48u << 20;

struct PackageCheck {
    bool ok = false;
    std::vector<std::string> problems;  // Portuguese; empty when ok
    Manifest manifest;
    SigCheck signature;
    WasmModule module;      // when the entry is a .wasm
    std::string sha512;     // of the whole .mlx (hex): identifies this exact package
    uint64_t size = 0;
    bool storeSigned = false;  // signed by one of the store's keys (mlx/Official.h)
};

struct PackageOptions {
    std::string appVersion;
    bool builtin = false;
};

// `zip` is left open on the package (members can be read afterwards: icon, help, module).
PackageCheck checkPackage(Bytes mlx, const PackageOptions& opt, ZipReader& zip);
inline PackageCheck checkPackage(Bytes mlx, const PackageOptions& opt) {
    ZipReader zip;
    return checkPackage(std::move(mlx), opt, zip);
}

// Builds a package from files (the packer, tests): manifest.json first, then the rest in the given
// order; when `key` is set, signature.json is added.
Bytes buildPackage(const std::vector<ZipFile>& files, const SigningKey* key);

}  // namespace mobilog::ext
