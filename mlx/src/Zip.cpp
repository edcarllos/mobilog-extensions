#include "mlx/Zip.h"

#include <libdeflate.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <set>

namespace mobilog::ext {
namespace {

uint16_t rd16(const uint8_t* p) { return uint16_t(p[0] | p[1] << 8); }
uint32_t rd32(const uint8_t* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }
void wr16(Bytes& b, uint16_t v) {
    b.push_back(uint8_t(v));
    b.push_back(uint8_t(v >> 8));
}
void wr32(Bytes& b, uint32_t v) {
    for (int i = 0; i < 4; ++i) b.push_back(uint8_t(v >> (8 * i)));
}

constexpr uint32_t kLocalSig = 0x04034b50, kCentralSig = 0x02014b50, kEndSig = 0x06054b50, kZip64LocatorSig = 0x07064b50;
constexpr size_t kLocalSize = 30, kCentralSize = 46, kEndSize = 22;

bool fail(std::string* error, std::string msg) {
    if (error) *error = std::move(msg);
    return false;
}

std::string lowerAscii(std::string s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    return s;
}

}  // namespace

bool validMemberName(std::string_view name, std::string* error) {
    if (name.empty()) return fail(error, "arquivo sem nome no pacote");
    if (!validUtf8(name)) return fail(error, "nome de arquivo com texto inválido no pacote");
    for (unsigned char c : name)
        if (c < 0x20 || c == 0x7F || c == '\\' || c == ':')
            return fail(error, "nome de arquivo não permitido no pacote: " + std::string(name));
    if (name.front() == '/') return fail(error, "caminho absoluto no pacote: " + std::string(name));
    size_t start = 0;
    while (start <= name.size()) {
        const size_t end = std::min(name.find('/', start), name.size());
        const std::string_view part = name.substr(start, end - start);
        if (part.empty() || part == "." || part == "..")
            return fail(error, "caminho não permitido no pacote: " + std::string(name));
        start = end + 1;
    }
    return true;
}

bool ZipReader::open(Bytes archive, const ZipLimits& lim, std::string* error) {
    data_ = std::move(archive);
    entries_.clear();
    const uint8_t* d = data_.data();
    const size_t size = data_.size();
    if (size < kEndSize) return fail(error, "o arquivo não é um pacote (zip) válido");

    // End of central directory: the comment must reach exactly the end of the file.
    size_t endPos = std::string::npos;
    const size_t lowest = size > kEndSize + 0xFFFF ? size - kEndSize - 0xFFFF : 0;
    for (size_t p = size - kEndSize + 1; p-- > lowest;) {
        if (rd32(d + p) == kEndSig && p + kEndSize + rd16(d + p + 20) == size) {
            endPos = p;
            break;
        }
    }
    if (endPos == std::string::npos) return fail(error, "o arquivo não é um pacote (zip) válido");
    if (endPos >= 20 && rd32(d + endPos - 20) == kZip64LocatorSig) return fail(error, "pacote zip64 não suportado");
    const uint8_t* e = d + endPos;
    const uint16_t disk = rd16(e + 4), cdDisk = rd16(e + 6), onDisk = rd16(e + 8), total = rd16(e + 10);
    const uint32_t cdSize = rd32(e + 12), cdOffset = rd32(e + 16);
    if (disk || cdDisk || onDisk != total) return fail(error, "pacote dividido em partes não suportado");
    if (total == 0xFFFF || cdSize == 0xFFFFFFFFu || cdOffset == 0xFFFFFFFFu) return fail(error, "pacote zip64 não suportado");
    if (total > lim.maxEntries) return fail(error, "o pacote tem arquivos demais");
    if (uint64_t(cdOffset) + cdSize > endPos) return fail(error, "diretório do pacote corrompido");

    struct Span {
        uint64_t begin, end;
    };
    std::vector<Span> spans;
    std::set<std::string> seen;
    uint64_t totalSize = 0;
    size_t p = cdOffset;
    for (uint16_t i = 0; i < total; ++i) {
        if (p + kCentralSize > uint64_t(cdOffset) + cdSize || rd32(d + p) != kCentralSig)
            return fail(error, "diretório do pacote corrompido");
        const uint8_t* c = d + p;
        const uint16_t madeBy = rd16(c + 4), flags = rd16(c + 8), method = rd16(c + 10);
        const uint32_t crc = rd32(c + 16), csize = rd32(c + 20), usize = rd32(c + 24);
        const uint16_t nameLen = rd16(c + 28), extraLen = rd16(c + 30), commentLen = rd16(c + 32), diskStart = rd16(c + 34);
        const uint32_t external = rd32(c + 38), localOffset = rd32(c + 42);
        const size_t next = p + kCentralSize + nameLen + extraLen + commentLen;
        if (next > uint64_t(cdOffset) + cdSize) return fail(error, "diretório do pacote corrompido");
        std::string name(reinterpret_cast<const char*>(c + kCentralSize), nameLen);
        p = next;

        if (flags & (1 | 64 | 8192)) return fail(error, "pacote cifrado não suportado");
        if (diskStart) return fail(error, "pacote dividido em partes não suportado");
        if (csize == 0xFFFFFFFFu || usize == 0xFFFFFFFFu || localOffset == 0xFFFFFFFFu) return fail(error, "pacote zip64 não suportado");
        if ((madeBy >> 8) == 3 && ((external >> 16) & 0xF000) == 0xA000)
            return fail(error, "o pacote contém um atalho (link), o que não é permitido: " + name);
        if (name.size() > lim.maxNameBytes) return fail(error, "nome de arquivo longo demais no pacote");
        const bool dir = !name.empty() && name.back() == '/';
        if (!validMemberName(dir ? std::string_view(name).substr(0, name.size() - 1) : std::string_view(name), error)) return false;
        if (!seen.insert(lowerAscii(name)).second) return fail(error, "arquivo repetido no pacote: " + name);
        if (dir) {
            if (usize) return fail(error, "pasta com conteúdo no pacote: " + name);
            continue;
        }
        if (method != 0 && method != 8) return fail(error, "compressão não suportada no pacote: " + name);
        if (method == 0 && csize != usize) return fail(error, "tamanho inconsistente no pacote: " + name);
        if (usize > lim.maxEntrySize) return fail(error, "arquivo grande demais no pacote: " + name);
        totalSize += usize;
        if (totalSize > lim.maxTotalSize) return fail(error, "o conteúdo do pacote é grande demais");
        if (usize > (64u << 10) && uint64_t(usize) > uint64_t(csize) * lim.maxRatio)
            return fail(error, "compressão suspeita no pacote (zip bomb): " + name);

        // The local header must agree and lie before the central directory.
        if (uint64_t(localOffset) + kLocalSize > cdOffset || rd32(d + localOffset) != kLocalSig)
            return fail(error, "arquivo corrompido no pacote: " + name);
        const uint8_t* l = d + localOffset;
        const uint16_t lflags = rd16(l + 6), lmethod = rd16(l + 8), lnameLen = rd16(l + 26), lextraLen = rd16(l + 28);
        const uint64_t dataOffset = uint64_t(localOffset) + kLocalSize + lnameLen + lextraLen;
        if (lmethod != method || (lflags & 1) || lnameLen != nameLen || dataOffset + csize > cdOffset ||
            std::memcmp(l + kLocalSize, name.data(), nameLen) != 0)
            return fail(error, "arquivo corrompido no pacote: " + name);
        if (!(lflags & 8) && (rd32(l + 14) != crc || rd32(l + 18) != csize || rd32(l + 22) != usize))
            return fail(error, "arquivo corrompido no pacote: " + name);
        spans.push_back({localOffset, dataOffset + csize});
        entries_.push_back({std::move(name), method, crc, csize, usize, dataOffset});
    }
    // Members may not overlap (another zip bomb trick).
    std::sort(spans.begin(), spans.end(), [](const Span& a, const Span& b) { return a.begin < b.begin; });
    for (size_t i = 1; i < spans.size(); ++i)
        if (spans[i].begin < spans[i - 1].end) return fail(error, "arquivos sobrepostos no pacote");
    return true;
}

const ZipEntry* ZipReader::find(std::string_view name) const {
    for (const auto& e : entries_)
        if (e.name == name) return &e;
    return nullptr;
}

bool ZipReader::read(const ZipEntry& e, Bytes& out, std::string* error) const {
    struct Deleter {
        void operator()(libdeflate_decompressor* d) const { libdeflate_free_decompressor(d); }
    };
    thread_local std::unique_ptr<libdeflate_decompressor, Deleter> dec(libdeflate_alloc_decompressor());
    const uint8_t* src = data_.data() + e.dataOffset;
    out.assign(size_t(e.size), 0);
    if (e.method == 0) {
        std::memcpy(out.data(), src, size_t(e.size));
    } else {
        size_t actual = 0;
        if (!dec || libdeflate_deflate_decompress(dec.get(), src, size_t(e.compressedSize), out.data(), out.size(), &actual) !=
                        LIBDEFLATE_SUCCESS ||
            actual != e.size)
            return fail(error, "arquivo corrompido no pacote: " + e.name);
    }
    if (libdeflate_crc32(0, out.data(), out.size()) != e.crc) return fail(error, "arquivo corrompido no pacote (CRC): " + e.name);
    return true;
}

bool ZipReader::read(std::string_view name, Bytes& out, std::string* error) const {
    const ZipEntry* e = find(name);
    if (!e) return fail(error, "o pacote não contém " + std::string(name));
    return read(*e, out, error);
}

Bytes writeZip(const std::vector<ZipFile>& files) {
    struct Deleter {
        void operator()(libdeflate_compressor* c) const { libdeflate_free_compressor(c); }
    };
    std::unique_ptr<libdeflate_compressor, Deleter> enc(libdeflate_alloc_compressor(9));
    constexpr uint16_t kVersion = 20, kUtf8 = 0x0800, kTime = 0, kDate = (1 << 5) | 1;  // 1980-01-01 00:00
    Bytes out, central;
    for (const auto& f : files) {
        const uint32_t crc = libdeflate_crc32(0, f.data.data(), f.data.size());
        Bytes packed(libdeflate_deflate_compress_bound(enc.get(), f.data.size()));
        size_t n = libdeflate_deflate_compress(enc.get(), f.data.data(), f.data.size(), packed.data(), packed.size());
        const bool deflate = n > 0 && n < f.data.size();
        const Bytes& body = deflate ? packed : f.data;
        if (!deflate) n = f.data.size();
        const uint32_t offset = uint32_t(out.size());
        const auto header = [&](Bytes& b, bool isCentral) {
            wr32(b, isCentral ? kCentralSig : kLocalSig);
            if (isCentral) wr16(b, kVersion);
            wr16(b, kVersion);
            wr16(b, kUtf8);
            wr16(b, deflate ? 8 : 0);
            wr16(b, kTime);
            wr16(b, kDate);
            wr32(b, crc);
            wr32(b, uint32_t(n));
            wr32(b, uint32_t(f.data.size()));
            wr16(b, uint16_t(f.name.size()));
            wr16(b, 0);
            if (isCentral) {
                wr16(b, 0);  // comment
                wr16(b, 0);  // disk
                wr16(b, 0);  // internal attributes
                wr32(b, 0);  // external attributes
                wr32(b, offset);
            }
            b.insert(b.end(), f.name.begin(), f.name.end());
        };
        header(out, false);
        out.insert(out.end(), body.begin(), body.begin() + long(n));
        header(central, true);
    }
    const uint32_t cdOffset = uint32_t(out.size());
    out.insert(out.end(), central.begin(), central.end());
    wr32(out, kEndSig);
    wr16(out, 0);
    wr16(out, 0);
    wr16(out, uint16_t(files.size()));
    wr16(out, uint16_t(files.size()));
    wr32(out, uint32_t(central.size()));
    wr32(out, cdOffset);
    wr16(out, 0);
    return out;
}

}  // namespace mobilog::ext
