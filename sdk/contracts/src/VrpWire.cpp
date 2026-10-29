#include "mobilog/vrp/VrpWire.h"

#include <algorithm>
#include <cmath>
#include <type_traits>

namespace mobilog::opt {
namespace {

constexpr uint32_t kMaxLocations = 5000;

template <typename T>
void putVec(ext::WireWriter& w, const std::vector<T>& v) {
    w.u32(uint32_t(v.size()));
    w.raw(v.data(), v.size() * sizeof(T));
}

template <typename T>
bool getVec(ext::WireReader& r, std::vector<T>& v, size_t expected) {
    const uint32_t n = r.count(sizeof(T), uint32_t(expected));
    if (!r.ok() || (n != 0 && n != expected)) return false;
    v.resize(n);
    for (T& x : v) {
        if constexpr (sizeof(T) == 1) x = T(r.u8());
        else if constexpr (std::is_same_v<T, float>) x = r.f32();
        else x = T(r.i32());
    }
    return r.ok();
}

}  // namespace

void encodeVrpInstance(const VrpInstance& inst, ext::WireWriter& w) {
    const uint32_t m = inst.m();
    w.u32(inst.n).u32(m).i32(inst.vehicles).i32(inst.capacity).f32(inst.valueOfTime);
    w.u8(uint8_t((inst.openRoutes ? 1 : 0) | (inst.payPerDelivery ? 2 : 0)));
    w.raw(inst.demand.data(), inst.demand.size() * 4);  // m x i32
    putVec(w, inst.time);                                 // m*m
    putVec(w, inst.ready);                                // m or 0
    putVec(w, inst.due);
    putVec(w, inst.service);
    putVec(w, inst.pair);
    putVec(w, inst.fixedVehicle);
    w.u32(uint32_t(inst.fleet.size()));
    for (const VrpVehicle& v : inst.fleet) w.u32(v.startLoc).f64(v.startTime).i32(v.startLoad);
    w.u32(uint32_t(inst.types.size()));
    for (const VehicleType& t : inst.types)
        w.i32(t.capacity).f32(t.speedFactor).f64(t.fixedCost).f64(t.costPerKm).f64(t.costPerHour).f64(t.deliveryFee).f64(t.kmFee);
    putVec(w, inst.typeOf);
    putVec(w, inst.dist);
    putVec(w, inst.toll);
    // Second part: what an exact search per vehicle type needs.
    w.u8(2);
    for (const VehicleType& t : inst.types) w.f32(t.tollFactor).u8(t.layer);
    w.u32(uint32_t(inst.layers.size()));
    for (const ArcLayer& l : inst.layers) {
        putVec(w, l.time);
        putVec(w, l.drive);
        putVec(w, l.dist);
        putVec(w, l.toll);
        putVec(w, l.blocked);
    }
}

void encodeWarmStart(const std::vector<std::vector<uint32_t>>* routes, ext::WireWriter& w) {
    w.u8(routes ? 1 : 0);
    if (routes) encodeRoutes(*routes, w);
}

bool decodeWarmStart(ext::WireReader& r, const VrpInstance& inst, std::vector<std::vector<uint32_t>>& routes, bool& has) {
    has = false;
    if (r.left() == 0) return true;
    has = r.u8() == 1;
    if (!has) return r.done();
    return decodeRoutes(r, inst, routes);
}

bool decodeVrpInstance(ext::WireReader& r, VrpInstance& inst, bool whole) {
    inst = {};
    inst.n = r.u32();
    const uint32_t m = r.u32();
    inst.vehicles = r.i32();
    inst.capacity = r.i32();
    inst.valueOfTime = r.f32();
    const uint8_t flags = r.u8();
    inst.openRoutes = flags & 1;
    inst.payPerDelivery = flags & 2;
    if (!r.ok() || m < inst.n + 1 || m > kMaxLocations || inst.vehicles < 1 || inst.vehicles > 10000) return false;
    inst.nodes.assign(m, 0);
    inst.demand.resize(m);
    for (int& d : inst.demand) d = r.i32();
    const size_t mm = size_t(m) * m;
    if (!getVec(r, inst.time, mm) || inst.time.size() != mm) return false;
    if (!getVec(r, inst.ready, m) || !getVec(r, inst.due, m) || !getVec(r, inst.service, m)) return false;
    if (!getVec(r, inst.pair, m) || !getVec(r, inst.fixedVehicle, m)) return false;
    const uint32_t fleet = r.count(16, uint32_t(inst.vehicles));
    inst.fleet.resize(fleet);
    for (VrpVehicle& v : inst.fleet) {
        v.startLoc = r.u32();
        v.startTime = r.f64();
        v.startLoad = r.i32();
        if (v.startLoc >= m) return false;
    }
    const uint32_t types = r.count(48, 64);
    inst.types.resize(types);
    for (VehicleType& t : inst.types) {
        t.capacity = r.i32();
        t.speedFactor = r.f32();
        t.fixedCost = r.f64();
        t.costPerKm = r.f64();
        t.costPerHour = r.f64();
        t.deliveryFee = r.f64();
        t.kmFee = r.f64();
    }
    if (!getVec(r, inst.typeOf, size_t(inst.vehicles))) return false;
    for (uint8_t k : inst.typeOf)
        if (k >= inst.types.size()) return false;
    if (!getVec(r, inst.dist, mm) || !getVec(r, inst.toll, mm)) return false;
    if (r.left() > 0) {  // the second part
        if (r.u8() != 2) return false;
        for (VehicleType& t : inst.types) {
            t.tollFactor = r.f32();
            t.layer = r.u8();
        }
        const uint32_t layers = r.count(20, 16);
        inst.layers.resize(layers);
        for (ArcLayer& l : inst.layers)
            if (!getVec(r, l.time, mm) || !getVec(r, l.drive, mm) || !getVec(r, l.dist, mm) || !getVec(r, l.toll, mm) ||
                !getVec(r, l.blocked, mm))
                return false;
        for (const VehicleType& t : inst.types)
            if (t.layer > 0 && t.layer >= inst.layers.size()) return false;
    }
    return whole ? r.done() : r.ok();
}

void encodeRoutes(const std::vector<std::vector<uint32_t>>& routes, ext::WireWriter& w) {
    w.u32(uint32_t(routes.size()));
    for (const auto& route : routes) putVec(w, route);
}

bool decodeRoutes(ext::WireReader& r, const VrpInstance& inst, std::vector<std::vector<uint32_t>>& routes) {
    const uint32_t n = r.count(4, uint32_t(std::max(inst.vehicles, 1)));
    routes.assign(n, {});
    for (auto& route : routes) {
        const uint32_t len = r.count(4, inst.m());
        route.resize(len);
        for (uint32_t& loc : route) {
            loc = r.u32();
            if (loc == 0 || loc >= inst.m()) r.fail();
        }
    }
    return r.done();
}

}  // namespace mobilog::opt
