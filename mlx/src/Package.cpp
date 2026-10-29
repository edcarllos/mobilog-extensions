#include "mlx/Package.h"

#include <algorithm>

namespace mobilog::ext {

PackageCheck checkPackage(Bytes mlx, const PackageOptions& opt, ZipReader& zip) {
    PackageCheck r;
    r.size = mlx.size();
    if (mlx.size() > kMaxPackageBytes) {
        r.problems.push_back("o pacote é grande demais (máximo " + std::to_string(kMaxPackageBytes >> 20) + " MB)");
        return r;
    }
    const Sha512 h = sha512(mlx);
    r.sha512 = toHex(h.data(), h.size());
    std::string err;
    if (!zip.open(std::move(mlx), &err)) {
        r.problems.push_back(err);
        return r;
    }
    Bytes raw;
    if (!zip.read(kManifestFile, raw, &err)) {
        r.problems.push_back("o pacote não tem manifest.json");
        return r;
    }
    std::vector<std::string> problems;
    parseManifest(std::string(raw.begin(), raw.end()), {opt.appVersion, opt.builtin}, r.manifest, problems);
    r.problems.insert(r.problems.end(), problems.begin(), problems.end());

    for (const std::string& f : r.manifest.namedFiles())
        if (!zip.find(f)) r.problems.push_back("o pacote não contém " + f);
    if (!r.manifest.native() && !r.manifest.entry.empty()) {
        Bytes wasm;
        if (!zip.read(r.manifest.entry, wasm, &err)) r.problems.push_back(err);
        else if (!inspectWasm(wasm, r.manifest.memoryMB, r.module, problems))
            for (auto& p : problems) r.problems.push_back(r.manifest.entry + ": " + p);
    }

    r.signature = verifyPackage(zip);
    if (r.signature.status == SigStatus::Invalid) r.problems.push_back("assinatura: " + r.signature.error);
    r.ok = r.problems.empty();
    return r;
}

Bytes buildPackage(const std::vector<ZipFile>& files, const SigningKey* key) {
    std::vector<ZipFile> ordered;
    for (const auto& f : files)
        if (f.name == kManifestFile) ordered.push_back(f);
    for (const auto& f : files)
        if (f.name != kManifestFile && f.name != kSignatureFile) ordered.push_back(f);
    if (key) {
        // Digests of exactly what goes in the archive.
        std::vector<FileDigest> digests;
        for (const auto& f : ordered) {
            const Sha512 h = sha512(f.data);
            digests.push_back({f.name, f.data.size(), toHex(h.data(), h.size())});
        }
        std::sort(digests.begin(), digests.end(), [](const FileDigest& a, const FileDigest& b) { return a.path < b.path; });
        const std::string sig = signatureJson(digests, *key);
        ordered.push_back({kSignatureFile, Bytes(sig.begin(), sig.end())});
    }
    return writeZip(ordered);
}

}  // namespace mobilog::ext
