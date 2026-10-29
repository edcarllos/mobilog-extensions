#pragma once
// Small byte helpers for extension packages: hex, base64, SHA-512 (Monocypher) and UTF-8 checks.
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace mobilog::ext {

using Bytes = std::vector<uint8_t>;
using Sha512 = std::array<uint8_t, 64>;

std::string toHex(const uint8_t* data, size_t size);
inline std::string toHex(const Bytes& b) { return toHex(b.data(), b.size()); }
bool fromHex(std::string_view hex, Bytes& out);

std::string toBase64(const uint8_t* data, size_t size);
inline std::string toBase64(const Bytes& b) { return toBase64(b.data(), b.size()); }
// Standard alphabet with padding; whitespace is ignored, anything else fails.
bool fromBase64(std::string_view text, Bytes& out);

Sha512 sha512(const uint8_t* data, size_t size);
inline Sha512 sha512(const Bytes& b) { return sha512(b.data(), b.size()); }

// Well-formed UTF-8 (no overlongs, surrogates or code points above U+10FFFF).
bool validUtf8(std::string_view s);
// Text shown to the user from a package (names, descriptions): control and bidirectional
// characters removed (no right-to-left tricks), whitespace runs collapsed, trimmed, at most
// `maxChars` code points (an ellipsis marks the cut). Invalid UTF-8 gives "".
std::string sanitizeText(std::string_view s, size_t maxChars, bool keepNewlines = false);

}  // namespace mobilog::ext
