#include "mlx/Signature.h"

#include <monocypher-ed25519.h>
#include <monocypher.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstring>

namespace mobilog::ext {

using nlohmann::json;

namespace {
constexpr const char* kKeyTag = "mobilog-ext-key-v1";
constexpr const char* kSecretTag = "mobilog-ext-secret-v1";
constexpr const char* kSignedHeader = "mobilog-ext-signature-v1\n";

// Splits "tag value [rest]" on spaces.
std::vector<std::string> words(std::string_view s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (!cur.empty()) out.push_back(std::move(cur)), cur.clear();
        } else cur += c;
    }
    if (!cur.empty()) out.push_back(std::move(cur));
    return out;
}
}  // namespace

SigningKey signingKeyFromSeed(const std::array<uint8_t, 32>& seed) {
    SigningKey k;
    std::array<uint8_t, 32> s = seed;  // Monocypher wipes the seed
    crypto_ed25519_key_pair(k.secret.data(), k.pub.data(), s.data());
    return k;
}

std::string publicKeyText(const PublicKey& pub, const std::string& name) {
    std::string s = std::string(kKeyTag) + " " + toBase64(pub.data(), pub.size());
    if (!name.empty()) s += " " + sanitizeText(name, 60);
    return s + "\n";
}

std::string secretKeyText(const SigningKey& key) {
    return std::string(kSecretTag) + " " + toBase64(key.secret.data(), key.secret.size()) + "\n";
}

bool parsePublicKey(std::string_view text, PublicKey& out, std::string* name) {
    const auto w = words(text);
    if (w.empty()) return false;
    size_t i = 0;
    if (w[0] == kKeyTag) i = 1;
    else if (w.size() != 1) return false;
    Bytes b;
    if (i >= w.size() || !fromBase64(w[i], b) || b.size() != out.size()) return false;
    std::copy(b.begin(), b.end(), out.begin());
    if (name) {
        name->clear();
        for (size_t k = i + 1; k < w.size(); ++k) *name += (name->empty() ? "" : " ") + w[k];
        *name = sanitizeText(*name, 60);
    }
    return true;
}

bool parseSecretKey(std::string_view text, SigningKey& out) {
    const auto w = words(text);
    Bytes b;
    if (w.size() != 2 || w[0] != kSecretTag || !fromBase64(w[1], b) || b.size() != out.secret.size()) return false;
    std::copy(b.begin(), b.end(), out.secret.begin());
    std::copy(b.begin() + 32, b.end(), out.pub.begin());  // seed || public key
    return true;
}

std::string keyFingerprint(const PublicKey& pub) {
    const Sha512 h = sha512(pub.data(), pub.size());
    const std::string hex = toHex(h.data(), 8);
    return hex.substr(0, 4) + " " + hex.substr(4, 4) + " " + hex.substr(8, 4) + " " + hex.substr(12, 4);
}

bool packageDigests(const ZipReader& zip, std::vector<FileDigest>& out, std::string* error) {
    out.clear();
    Bytes data;
    for (const auto& e : zip.entries()) {
        if (e.name == kSignatureFile) continue;
        if (!zip.read(e, data, error)) return false;
        const Sha512 h = sha512(data);
        out.push_back({e.name, e.size, toHex(h.data(), h.size())});
    }
    std::sort(out.begin(), out.end(), [](const FileDigest& a, const FileDigest& b) { return a.path < b.path; });
    return true;
}

std::string signedText(const std::vector<FileDigest>& files) {
    std::string s = kSignedHeader;
    for (const auto& f : files) s += f.path + "\t" + std::to_string(f.size) + "\t" + f.sha512 + "\n";
    return s;
}

std::string signatureJson(const std::vector<FileDigest>& files, const SigningKey& key) {
    const std::string text = signedText(files);
    std::array<uint8_t, 64> sig;
    crypto_ed25519_sign(sig.data(), key.secret.data(), reinterpret_cast<const uint8_t*>(text.data()), text.size());
    json list = json::array();
    for (const auto& f : files) list.push_back({{"path", f.path}, {"size", f.size}, {"sha512", f.sha512}});
    json j = {{"version", 1},
              {"key", toBase64(key.pub.data(), key.pub.size())},
              {"signature", toBase64(sig.data(), sig.size())},
              {"files", list}};
    return j.dump(2);
}

SigCheck verifyPackage(const ZipReader& zip) {
    SigCheck r;
    const ZipEntry* e = zip.find(kSignatureFile);
    if (!e) return r;
    r.status = SigStatus::Invalid;
    Bytes raw;
    if (!zip.read(*e, raw, &r.error)) return r;
    json j;
    try {
        j = json::parse(raw.begin(), raw.end());
    } catch (const std::exception&) {
        r.error = "assinatura ilegível";
        return r;
    }
    Bytes key, sig;
    if (!j.is_object() || j.value("version", 0) != 1 || !j.contains("key") || !j["key"].is_string() || !j.contains("signature") ||
        !j["signature"].is_string() || !j.contains("files") || !j["files"].is_array() ||
        !fromBase64(j["key"].get<std::string>(), key) || key.size() != 32 || !fromBase64(j["signature"].get<std::string>(), sig) ||
        sig.size() != 64) {
        r.error = "assinatura ilegível";
        return r;
    }
    std::copy(key.begin(), key.end(), r.key.begin());
    std::vector<FileDigest> listed;
    for (const auto& f : j["files"]) {
        if (!f.is_object() || !f.contains("path") || !f["path"].is_string() || !f.contains("size") || !f["size"].is_number_unsigned() ||
            !f.contains("sha512") || !f["sha512"].is_string()) {
            r.error = "assinatura ilegível";
            return r;
        }
        listed.push_back({f["path"].get<std::string>(), f["size"].get<uint64_t>(), f["sha512"].get<std::string>()});
    }
    std::sort(listed.begin(), listed.end(), [](const FileDigest& a, const FileDigest& b) { return a.path < b.path; });
    std::vector<FileDigest> actual;
    if (!packageDigests(zip, actual, &r.error)) return r;
    if (listed.size() != actual.size()) {
        r.error = "o pacote foi modificado depois de assinado (arquivos a mais ou a menos)";
        return r;
    }
    for (size_t i = 0; i < listed.size(); ++i)
        if (listed[i].path != actual[i].path || listed[i].size != actual[i].size || listed[i].sha512 != actual[i].sha512) {
            r.error = "o pacote foi modificado depois de assinado (" + actual[i].path + ")";
            return r;
        }
    const std::string text = signedText(actual);
    if (crypto_ed25519_check(sig.data(), r.key.data(), reinterpret_cast<const uint8_t*>(text.data()), text.size()) != 0) {
        r.error = "assinatura inválida";
        return r;
    }
    r.status = SigStatus::Valid;
    r.error.clear();
    return r;
}

namespace {
constexpr const char* kDetachedHeader = "mobilog-catalog-v1\n";
constexpr std::string_view kDetachedTag = "mobilog-catalog-signature-v1";
}  // namespace

std::string signDetached(std::string_view data, const SigningKey& key) {
    std::string text = kDetachedHeader;
    text.append(data);
    std::array<uint8_t, 64> sig;
    crypto_ed25519_sign(sig.data(), key.secret.data(), reinterpret_cast<const uint8_t*>(text.data()), text.size());
    return std::string(kDetachedTag) + " " + toBase64(key.pub.data(), key.pub.size()) + " " + toBase64(sig.data(), sig.size()) + "\n";
}

bool verifyDetached(std::string_view data, std::string_view signature, PublicKey& key, std::string* error) {
    const auto fail = [&](const char* why) {
        if (error) *error = why;
        return false;
    };
    while (!signature.empty() && (signature.back() == '\n' || signature.back() == '\r' || signature.back() == ' ')) signature.remove_suffix(1);
    if (signature.substr(0, kDetachedTag.size()) != kDetachedTag || signature.size() <= kDetachedTag.size() + 1 ||
        signature[kDetachedTag.size()] != ' ')
        return fail("assinatura do catálogo ilegível");
    signature.remove_prefix(kDetachedTag.size() + 1);
    const size_t space = signature.find(' ');
    Bytes k, s;
    if (space == std::string_view::npos || !fromBase64(signature.substr(0, space), k) || k.size() != 32 ||
        !fromBase64(signature.substr(space + 1), s) || s.size() != 64)
        return fail("assinatura do catálogo ilegível");
    std::copy(k.begin(), k.end(), key.begin());
    std::string text = kDetachedHeader;
    text.append(data);
    if (crypto_ed25519_check(s.data(), key.data(), reinterpret_cast<const uint8_t*>(text.data()), text.size()) != 0)
        return fail("assinatura do catálogo inválida");
    return true;
}

}  // namespace mobilog::ext
