#pragma once
// Zip archives in memory, for extension packages (.mlx). Nothing is ever extracted to disk: the
// package stays one blob and members are inflated on demand (libdeflate). The reader accepts only
// a strict subset and checks it before anything reads a member:
// - stored (0) or deflate (8) members, no encryption, no zip64, no multi-disk archives;
// - file names: UTF-8, relative, '/'-separated, no "." / ".." / empty parts, no '\\' or ':' (drive
//   letters), no control characters, unique even ignoring ASCII case; Unix symlinks refused;
// - every member's local header agrees with the central directory and lies inside the archive;
// - limits on member count, sizes and compression ratio (zip bombs); CRC-32 checked on read.
#include "mlx/Bytes.h"

#include <string>
#include <string_view>
#include <vector>

namespace mobilog::ext {

struct ZipLimits {
    size_t maxEntries = 256;
    size_t maxNameBytes = 160;
    uint64_t maxEntrySize = 64ull << 20;   // uncompressed, per member
    uint64_t maxTotalSize = 128ull << 20;  // uncompressed, all members
    uint32_t maxRatio = 200;               // uncompressed / compressed, members over 64 KB
};

struct ZipEntry {
    std::string name;
    uint16_t method = 0;
    uint32_t crc = 0;
    uint64_t compressedSize = 0, size = 0;
    uint64_t dataOffset = 0;  // first byte of the member's data in the archive
};

class ZipReader {
public:
    // Checks the whole directory; false with a message in Portuguese.
    bool open(Bytes archive, const ZipLimits& limits, std::string* error);
    bool open(Bytes archive, std::string* error) { return open(std::move(archive), ZipLimits{}, error); }

    const std::vector<ZipEntry>& entries() const { return entries_; }  // files only, in archive order
    const ZipEntry* find(std::string_view name) const;
    // Inflates (or copies) a member and checks its size and CRC.
    bool read(const ZipEntry& e, Bytes& out, std::string* error) const;
    bool read(std::string_view name, Bytes& out, std::string* error) const;
    const Bytes& archive() const { return data_; }

private:
    Bytes data_;
    std::vector<ZipEntry> entries_;
};

struct ZipFile {
    std::string name;
    Bytes data;
};
// Deterministic archive: members in the given order, fixed timestamp (1980-01-01), deflate when
// it saves space (level 9), stored otherwise.
Bytes writeZip(const std::vector<ZipFile>& files);

// The name rules above (also used by the packer before writing).
bool validMemberName(std::string_view name, std::string* error);

}  // namespace mobilog::ext
