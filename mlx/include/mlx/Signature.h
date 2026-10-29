#pragma once
// Package signatures: Ed25519 (Monocypher) over a canonical list of every file in the package.
//
// signature.json = {"version": 1, "key": "<public key, base64>", "signature": "<base64>",
//                   "files": [{"path": "...", "size": N, "sha512": "<hex>"}, ...]}
// The signed text is "mobilog-ext-signature-v1\n" followed by one "path\tsize\tsha512\n" line per
// file, sorted by path (bytes). Every member except signature.json must be listed, with the same
// size and hash, and nothing else: a file added, removed or changed breaks the signature.
//
// Key files (text): "mobilog-ext-key-v1 <base64 public key> [name]" and
// "mobilog-ext-secret-v1 <base64 64-byte secret key>".
#include "mlx/Zip.h"

#include <array>
#include <string>
#include <vector>

namespace mobilog::ext {

using PublicKey = std::array<uint8_t, 32>;

struct SigningKey {
    std::array<uint8_t, 64> secret{};  // Monocypher's Ed25519 secret key (seed + public key)
    PublicKey pub{};
};
SigningKey signingKeyFromSeed(const std::array<uint8_t, 32>& seed);
std::string publicKeyText(const PublicKey& pub, const std::string& name = {});
std::string secretKeyText(const SigningKey& key);
// Accepts a key line or a bare base64 key; `name` gets the optional comment.
bool parsePublicKey(std::string_view text, PublicKey& out, std::string* name = nullptr);
bool parseSecretKey(std::string_view text, SigningKey& out);
// What the user compares: 16 hex digits in groups of four ("ab12 cd34 ef56 7890").
std::string keyFingerprint(const PublicKey& pub);

struct FileDigest {
    std::string path;
    uint64_t size = 0;
    std::string sha512;  // hex
};
constexpr const char* kSignatureFile = "signature.json";
// Every member except signature.json, sorted by path.
bool packageDigests(const ZipReader& zip, std::vector<FileDigest>& out, std::string* error);
std::string signedText(const std::vector<FileDigest>& files);
std::string signatureJson(const std::vector<FileDigest>& files, const SigningKey& key);

enum class SigStatus { Unsigned, Valid, Invalid };
struct SigCheck {
    SigStatus status = SigStatus::Unsigned;
    PublicKey key{};
    std::string error;  // Invalid: why, in Portuguese
};
SigCheck verifyPackage(const ZipReader& zip);

// A detached signature of a text (the store's catalog.json, in catalog.json.sig):
// "mobilog-catalog-signature-v1 <public key, base64> <signature, base64>\n", over
// "mobilog-catalog-v1\n" followed by the exact bytes of the text.
std::string signDetached(std::string_view data, const SigningKey& key);
// Valid: true, and `key` is the key that signed (compare it with the keys you trust).
bool verifyDetached(std::string_view data, std::string_view signature, PublicKey& key, std::string* error = nullptr);

}  // namespace mobilog::ext
