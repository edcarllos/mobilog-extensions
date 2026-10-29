#pragma once
// The binary blocks of mobilog.call (requests and answers) and of the events: little-endian numbers
// one after the other, no padding; text as u32 length + UTF-8 bytes; a point as f64 lon, f64 lat.
// The layouts of each function are in sdk/include/mobilog_ext.h. A reader never reads past its block: a
// short or malformed request makes ok() false (the call answers kErrInvalid).
#include "mlx/Bytes.h"

#include <cstring>
#include <string>
#include <string_view>

namespace mobilog::ext {

class WireWriter {
public:
    WireWriter& u8(uint8_t v) {
        b_.push_back(v);
        return *this;
    }
    WireWriter& u32(uint32_t v) { return raw(&v, 4); }
    WireWriter& i32(int32_t v) { return raw(&v, 4); }
    WireWriter& u64(uint64_t v) { return raw(&v, 8); }
    WireWriter& f32(float v) { return raw(&v, 4); }
    WireWriter& f64(double v) { return raw(&v, 8); }
    WireWriter& str(std::string_view s) {
        u32(uint32_t(s.size()));
        return raw(s.data(), s.size());
    }
    WireWriter& blob(const Bytes& b) {
        u32(uint32_t(b.size()));
        return raw(b.data(), b.size());
    }
    WireWriter& lonLat(double lon, double lat) { return f64(lon).f64(lat); }
    WireWriter& raw(const void* p, size_t n) {
        const auto* c = static_cast<const uint8_t*>(p);
        b_.insert(b_.end(), c, c + n);
        return *this;
    }
    size_t size() const { return b_.size(); }
    const Bytes& bytes() const { return b_; }
    Bytes take() { return std::move(b_); }

private:
    Bytes b_;  // little-endian hosts only (every platform the app runs on)
};

class WireReader {
public:
    WireReader(const uint8_t* data, size_t size) : p_(data), n_(size) {}
    explicit WireReader(const Bytes& b) : p_(b.data()), n_(b.size()) {}

    uint8_t u8() {
        uint8_t v = 0;
        get(&v, 1);
        return v;
    }
    uint32_t u32() {
        uint32_t v = 0;
        get(&v, 4);
        return v;
    }
    int32_t i32() {
        int32_t v = 0;
        get(&v, 4);
        return v;
    }
    uint64_t u64() {
        uint64_t v = 0;
        get(&v, 8);
        return v;
    }
    float f32() {
        float v = 0;
        get(&v, 4);
        return v;
    }
    double f64() {
        double v = 0;
        get(&v, 8);
        return v;
    }
    // Text of at most `max` bytes, valid UTF-8.
    std::string str(size_t max = 1u << 16) {
        const uint32_t len = u32();
        if (!ok_ || len > max || len > n_ - at_) return fail(), std::string();
        std::string s(reinterpret_cast<const char*>(p_ + at_), len);
        at_ += len;
        if (!validUtf8(s)) return fail(), std::string();
        return s;
    }
    Bytes blob(size_t max) {
        const uint32_t len = u32();
        if (!ok_ || len > max || len > n_ - at_) return fail(), Bytes();
        Bytes b(p_ + at_, p_ + at_ + len);
        at_ += len;
        return b;
    }
    // A count of items of at least `itemBytes` each: never more than the block can hold.
    uint32_t count(size_t itemBytes, uint32_t max) {
        const uint32_t c = u32();
        if (!ok_ || c > max || (itemBytes && size_t(c) > (n_ - at_) / itemBytes)) return fail(), 0;
        return c;
    }
    bool ok() const { return ok_; }
    bool done() const { return ok_ && at_ == n_; }  // read whole, nothing left over
    size_t left() const { return n_ - at_; }
    void fail() { ok_ = false; }

private:
    void get(void* v, size_t k) {
        if (!ok_ || k > n_ - at_) return fail();
        std::memcpy(v, p_ + at_, k);
        at_ += k;
    }
    const uint8_t* p_;
    size_t n_, at_ = 0;
    bool ok_ = true;
};

}  // namespace mobilog::ext
