#include "mlx/Bytes.h"

#include <monocypher-ed25519.h>

namespace mobilog::ext {

std::string toHex(const uint8_t* data, size_t size) {
    static const char* d = "0123456789abcdef";
    std::string s(size * 2, '0');
    for (size_t i = 0; i < size; ++i) {
        s[2 * i] = d[data[i] >> 4];
        s[2 * i + 1] = d[data[i] & 15];
    }
    return s;
}

static int hexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool fromHex(std::string_view hex, Bytes& out) {
    if (hex.size() % 2) return false;
    out.resize(hex.size() / 2);
    for (size_t i = 0; i < out.size(); ++i) {
        const int a = hexDigit(hex[2 * i]), b = hexDigit(hex[2 * i + 1]);
        if (a < 0 || b < 0) return false;
        out[i] = uint8_t(a * 16 + b);
    }
    return true;
}

static const char* kB64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string toBase64(const uint8_t* data, size_t size) {
    std::string s;
    s.reserve((size + 2) / 3 * 4);
    for (size_t i = 0; i < size; i += 3) {
        const uint32_t n = uint32_t(data[i]) << 16 | (i + 1 < size ? uint32_t(data[i + 1]) << 8 : 0) |
                           (i + 2 < size ? data[i + 2] : 0);
        s += kB64[n >> 18 & 63];
        s += kB64[n >> 12 & 63];
        s += i + 1 < size ? kB64[n >> 6 & 63] : '=';
        s += i + 2 < size ? kB64[n & 63] : '=';
    }
    return s;
}

bool fromBase64(std::string_view text, Bytes& out) {
    out.clear();
    uint32_t acc = 0;
    int bits = 0, pad = 0;
    for (char c : text) {
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t') continue;
        if (c == '=') {
            ++pad;
            continue;
        }
        if (pad) return false;  // data after padding
        int v;
        if (c >= 'A' && c <= 'Z') v = c - 'A';
        else if (c >= 'a' && c <= 'z') v = c - 'a' + 26;
        else if (c >= '0' && c <= '9') v = c - '0' + 52;
        else if (c == '+') v = 62;
        else if (c == '/') v = 63;
        else return false;
        acc = acc << 6 | uint32_t(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(uint8_t(acc >> bits & 0xFF));
        }
    }
    return pad <= 2 && bits < 6;
}

Sha512 sha512(const uint8_t* data, size_t size) {
    Sha512 h;
    crypto_sha512(h.data(), data, size);
    return h;
}

// Decodes one code point at s[i]; returns its length (0 when malformed).
static size_t decodeUtf8(std::string_view s, size_t i, uint32_t& cp) {
    const auto b = [&](size_t k) { return uint8_t(s[i + k]); };
    const uint8_t c = b(0);
    if (c < 0x80) {
        cp = c;
        return 1;
    }
    size_t n;
    uint32_t min;
    if ((c & 0xE0) == 0xC0) n = 2, cp = c & 0x1F, min = 0x80;
    else if ((c & 0xF0) == 0xE0) n = 3, cp = c & 0x0F, min = 0x800;
    else if ((c & 0xF8) == 0xF0) n = 4, cp = c & 0x07, min = 0x10000;
    else return 0;
    if (i + n > s.size()) return 0;
    for (size_t k = 1; k < n; ++k) {
        if ((b(k) & 0xC0) != 0x80) return 0;
        cp = cp << 6 | (b(k) & 0x3F);
    }
    if (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return 0;
    return n;
}

bool validUtf8(std::string_view s) {
    for (size_t i = 0; i < s.size();) {
        uint32_t cp = 0;
        const size_t n = decodeUtf8(s, i, cp);
        if (!n) return false;
        i += n;
    }
    return true;
}

static bool hiddenCodePoint(uint32_t cp) {
    return cp < 0x20 || (cp >= 0x7F && cp <= 0x9F) || cp == 0x061C || (cp >= 0x200B && cp <= 0x200F) ||
           (cp >= 0x202A && cp <= 0x202E) || (cp >= 0x2060 && cp <= 0x2069) || cp == 0xFEFF;
}

std::string sanitizeText(std::string_view s, size_t maxChars, bool keepNewlines) {
    if (!validUtf8(s)) return {};
    std::string out;
    size_t chars = 0;
    bool space = false;
    for (size_t i = 0; i < s.size();) {
        uint32_t cp = 0;
        const size_t n = decodeUtf8(s, i, cp);
        const std::string_view piece = s.substr(i, n);
        i += n;
        if (keepNewlines && cp == '\n') {
            while (!out.empty() && out.back() == ' ') out.pop_back();
            out += '\n';
            space = false;
            ++chars;
            continue;
        }
        if (cp == ' ' || cp == '\t' || cp == '\r' || cp == '\n' || cp == 0xA0) {
            space = true;
            continue;
        }
        if (hiddenCodePoint(cp)) continue;
        if (space && !out.empty() && out.back() != '\n') {
            out += ' ';
            ++chars;
        }
        space = false;
        if (chars >= maxChars) {
            while (!out.empty() && out.back() == ' ') out.pop_back();
            out += "…";
            return out;
        }
        out += piece;
        ++chars;
    }
    while (!out.empty() && (out.back() == ' ' || out.back() == '\n')) out.pop_back();
    return out;
}

}  // namespace mobilog::ext
