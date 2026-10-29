// keygen, pack, sign, verify, inspect, meter: making and checking packages.
#include "Tool.h"

#include "mlx/Metering.h"

#include <array>
#include <cstdio>
#include <random>

namespace mlxtool {

using namespace mobilog::ext;

int cmdKeygen(const Args& a) {
    if (a.positional.size() != 1) return usage();
    std::random_device rd;
    std::array<uint8_t, 32> seed;
    for (size_t i = 0; i < seed.size(); i += 4) {
        const uint32_t v = rd();
        for (int k = 0; k < 4; ++k) seed[i + k] = uint8_t(v >> (8 * k));
    }
    const SigningKey key = signingKeyFromSeed(seed);
    const std::string base = a.positional[0];
    const std::string secret = secretKeyText(key), pub = publicKeyText(key.pub, a.name);
    if (fs::exists(base + ".secret")) {
        std::fprintf(stderr, "%s: %s.secret\n", tr("erro: já existe", "error: already exists"), base.c_str());
        return 1;
    }
    if (!writeFile(base + ".secret", secret.data(), secret.size(), true) || !writeFile(base + ".pub", pub.data(), pub.size())) {
        std::fprintf(stderr, "%s\n", tr("erro: não foi possível gravar as chaves", "error: could not write the keys"));
        return 1;
    }
    std::printf(tr("Chave criada: %s.secret (guarde em segredo) e %s.pub (distribua)\nImpressão digital: %s\n",
                   "Key created: %s.secret (keep it secret) and %s.pub (share it)\nFingerprint: %s\n"),
                base.c_str(), base.c_str(), keyFingerprint(key.pub).c_str());
    return 0;
}

int cmdPack(const Args& a) {
    if (a.positional.size() != 1) return usage();
    const fs::path dir = a.positional[0];
    if (!fs::is_regular_file(dir / kManifestFile)) {
        std::fprintf(stderr, "%s: %s\n", tr("erro: a pasta não tem manifest.json", "error: the folder has no manifest.json"),
                     dir.string().c_str());
        return 1;
    }
    std::vector<ZipFile> files;
    if (!collectFolder(dir, files)) return 1;
    SigningKey key;
    if (!a.key.empty() && !loadSecret(a.key, key)) return 1;
    const Bytes mlx = buildPackage(files, a.key.empty() ? nullptr : &key);
    const PackageCheck c = checkPackage(mlx, {"", a.mobilog});
    const std::string out = !a.out.empty() ? a.out : (c.manifest.id.empty() ? "extensao" : c.manifest.id + "-" + c.manifest.version) + ".mlx";
    if (report(c, out)) return 1;
    if (!writeFile(out, mlx.data(), mlx.size())) {
        std::fprintf(stderr, "%s %s\n", tr("erro: não foi possível gravar", "error: could not write"), out.c_str());
        return 1;
    }
    if (!a.quiet)
        std::printf(tr("%s: %s %s, %zu arquivos, %.1f KB%s\n", "%s: %s %s, %zu files, %.1f KB%s\n"), out.c_str(), c.manifest.name.c_str(),
                    c.manifest.version.c_str(), files.size(), double(mlx.size()) / 1024.0,
                    a.key.empty() ? tr(" (sem assinatura)", " (unsigned)") : tr(", assinado", ", signed"));
    return 0;
}

int cmdSign(const Args& a) {
    if (a.positional.size() != 1 || a.key.empty()) return usage();
    Bytes mlx;
    if (!readFile(a.positional[0], mlx)) {
        std::fprintf(stderr, "%s %s\n", tr("erro: não foi possível ler", "error: could not read"), a.positional[0].c_str());
        return 1;
    }
    ZipReader zip;
    std::string err;
    if (!zip.open(mlx, &err)) {
        std::fprintf(stderr, "%s: %s\n", tr("erro", "error"), err.c_str());
        return 1;
    }
    std::vector<ZipFile> files;
    for (const auto& e : zip.entries()) {
        if (e.name == kSignatureFile) continue;
        ZipFile f{e.name, {}};
        if (!zip.read(e, f.data, &err)) {
            std::fprintf(stderr, "%s: %s\n", tr("erro", "error"), err.c_str());
            return 1;
        }
        files.push_back(std::move(f));
    }
    SigningKey key;
    if (!loadSecret(a.key, key)) return 1;
    const Bytes out = buildPackage(files, &key);
    const std::string path = a.out.empty() ? a.positional[0] : a.out;
    if (report(checkPackage(out, {"", a.mobilog}), path)) return 1;
    if (!writeFile(path, out.data(), out.size())) {
        std::fprintf(stderr, "%s %s\n", tr("erro: não foi possível gravar", "error: could not write"), path.c_str());
        return 1;
    }
    if (!a.quiet) std::printf("%s: %s (%s)\n", path.c_str(), tr("assinado", "signed"), keyFingerprint(key.pub).c_str());
    return 0;
}

int cmdVerify(const Args& a, bool details) {
    if (a.positional.size() != 1) return usage();
    Bytes mlx;
    if (!readFile(a.positional[0], mlx)) {
        std::fprintf(stderr, "%s %s\n", tr("erro: não foi possível ler", "error: could not read"), a.positional[0].c_str());
        return 1;
    }
    const PackageCheck c = checkPackage(std::move(mlx), {"", a.mobilog});
    const Manifest& m = c.manifest;
    if (!m.id.empty()) std::printf("%s %s (%s)\n", m.name.c_str(), m.version.c_str(), m.id.c_str());
    if (details) {
        if (!m.author.empty()) std::printf("%s: %s\n", tr("Autor", "Author"), m.author.c_str());
        if (!m.summary.empty()) std::printf("%s\n", m.summary.c_str());
        if (!m.description.empty()) std::printf("%s\n", m.description.c_str());
        if (!m.category.empty()) {
            const CategoryInfo* cat = categoryByKey(m.category);
            std::printf("%s: %s\n", tr("Categoria", "Category"), cat ? tr(cat->label, cat->labelEn) : m.category.c_str());
        }
        std::printf(tr("API %d%s · memória até %u MB · %.1f KB\n", "API %d%s · memory up to %u MB · %.1f KB\n"), m.apiVersion,
                    m.minAppVersion.empty() ? "" : (" · Mobilog " + m.minAppVersion + "+").c_str(), m.memoryMB, double(c.size) / 1024.0);
        std::printf("%s:\n", tr("Permissões", "Permissions"));
        for (Permission p : permissionList(m.permissions)) std::printf("  %s — %s\n", permissionInfo(p)->key, permissionInfo(p)->label);
        if (!m.contributes.empty()) std::printf("%s:\n", tr("Contribui com", "Contributes"));
        for (const auto& con : m.contributes) std::printf("  %s:%s — %s\n", contributionKindKey(con.kind), con.id.c_str(), con.title.c_str());
        if (!m.native() && c.module.hasMemory)
            std::printf(tr("Módulo: %zu funções, %u laços, memória %u–%u páginas, %zu importações\n",
                           "Module: %zu functions, %u loops, memory %u–%u pages, %zu imports\n"),
                        c.module.functions.size(), c.module.loops, c.module.memoryMin, c.module.memoryMax, c.module.imports.size());
        const std::vector<std::string> named = m.namedFiles();
        if (!named.empty()) {
            std::printf("%s:", tr("Arquivos", "Files"));
            for (const std::string& f : named) std::printf(" %s", f.c_str());
            std::printf("\n");
        }
        std::printf("SHA-512: %s\n", c.sha512.c_str());
    }
    bool trusted = false;
    if (c.signature.status == SigStatus::Valid) {
        for (const auto& t : a.trust) {
            std::string text;
            PublicKey k;
            if (readText(t, text) && parsePublicKey(text, k) && k == c.signature.key) trusted = true;
        }
        std::printf("%s (%s)%s\n", tr("Assinatura: válida", "Signature: valid"), keyFingerprint(c.signature.key).c_str(),
                    a.trust.empty() ? "" : (trusted ? tr(", chave confiável", ", trusted key") : tr(", chave NÃO confiável", ", key NOT trusted")));
    } else if (c.signature.status == SigStatus::Unsigned) {
        std::printf("%s\n", tr("Assinatura: nenhuma (não verificada)", "Signature: none (unverified)"));
    }
    if (report(c, a.positional[0])) return 1;
    if (!a.trust.empty() && !trusted) return 3;
    std::printf("OK\n");
    return 0;
}

int cmdMeter(const Args& a) {
    if (a.positional.size() != 1 || a.out.empty()) return usage();
    Bytes in, out;
    std::string err;
    if (!readFile(a.positional[0], in)) {
        std::fprintf(stderr, "%s %s\n", tr("erro: não foi possível ler", "error: could not read"), a.positional[0].c_str());
        return 1;
    }
    if (!meterModule(in, out, &err)) {
        std::fprintf(stderr, "%s: %s\n", tr("erro", "error"), err.c_str());
        return 1;
    }
    if (!writeFile(a.out, out.data(), out.size())) {
        std::fprintf(stderr, "%s %s\n", tr("erro: não foi possível gravar", "error: could not write"), a.out.c_str());
        return 1;
    }
    std::printf("%s: %zu -> %zu bytes\n", a.out.c_str(), in.size(), out.size());
    return 0;
}

}  // namespace mlxtool
