#include "mobilog/vrp/Vrp.h"


#include <algorithm>

namespace mobilog::opt {

namespace {

// The arcs one vehicle drives on: its type's layer (only the driving part scales with its speed) or
// the instance's own matrices.
struct Arcs {
    const float *time = nullptr, *drive = nullptr, *dist = nullptr, *toll = nullptr;
    const uint8_t* blocked = nullptr;
    uint32_t m = 0;
    float invSpeed = 1.0f;
};

// The route walked stop by stop. A template on the kind of arcs so the loop has no branch for it and
// the sums stay in registers (the hottest code of every search: each candidate move is evaluated here).
template <bool Layered>
RouteEval walk(const VrpInstance& inst, int k, const std::vector<uint32_t>& route, const Arcs& g, const VehicleType* type,
               int capacity) {
    RouteEval e;
    const VrpVehicle veh = inst.vehicle(k);
    const bool tw = inst.hasTimeWindows();
    const bool pdp = inst.isPickupDelivery();
    double travel = 0, distance = 0, toll = 0;
    int blocked = 0;
    auto leg = [&](uint32_t a, uint32_t b) {
        const size_t i = size_t(a) * g.m + b;
        if constexpr (Layered) {
            const float dt = g.drive[i] * g.invSpeed + (g.time[i] - g.drive[i]);
            travel += dt;
            distance += g.dist[i];
            if (g.toll) toll += g.toll[i];
            if (g.blocked) blocked += g.blocked[i];
            return dt;
        } else {
            const float dt = g.time[i] * g.invSpeed;
            travel += dt;
            distance += g.dist ? g.dist[i] : 0.0f;
            toll += g.toll ? g.toll[i] : 0.0f;
            return dt;
        }
    };
    // Load at departure: CVRP loads every delivery at the depot; PDP starts with goods on board.
    int load = veh.startLoad;
    if (!pdp)
        for (uint32_t c : route) load += inst.demand[c];
    e.overload = std::max(0, load - capacity);

    // PDP precedence needs each stop's position: stamped per-thread lookup, O(route) per call.
    thread_local std::vector<uint32_t> posOf, stampOf;
    thread_local uint32_t stamp = 0;
    if (pdp) {
        if (posOf.size() < inst.m()) {
            posOf.assign(inst.m(), 0);
            stampOf.assign(inst.m(), 0);
        }
        if (++stamp == 0) {
            std::fill(stampOf.begin(), stampOf.end(), 0);
            stamp = 1;
        }
        for (size_t i = 0; i < route.size(); ++i) {
            posOf[route[i]] = static_cast<uint32_t>(i);
            stampOf[route[i]] = stamp;
        }
    }

    const bool fixed = !inst.fixedVehicle.empty();
    double time = veh.startTime;
    uint32_t at = veh.startLoc;
    for (size_t i = 0; i < route.size(); ++i) {
        const uint32_t c = route[i];
        const double before = distance;
        time += leg(at, c);
        if (load > 0) e.loadedDistance += distance - before;  // carrying something on this leg
        if (inst.demand[c] <= 0) ++e.deliveries;
        if (tw) {
            time = std::max(time, double(inst.ready[c]));
            e.lateness += std::max(0.0, time - double(inst.due[c]));
            if (inst.demand[c] <= 0) e.completion += time - veh.startTime;  // deliveries (or plain customers)
            time += inst.service[c];
        }
        if (pdp) {
            load += inst.demand[c];
            e.overload = std::max(e.overload, load - capacity);
            const int32_t partner = inst.pair[c];
            if (partner >= 0) {
                // The partner must be on this route: before us if we deliver, after us if we pick up.
                const bool here = stampOf[partner] == stamp;
                const bool before = here && posOf[partner] < i;
                const bool after = here && posOf[partner] > i;
                if ((inst.demand[c] < 0 && !before) || (inst.demand[c] > 0 && !after)) ++e.precedence;
            }
        }
        if (fixed && inst.fixedTo(c) >= 0 && inst.fixedTo(c) != k) ++e.precedence;  // on another vehicle's board
        at = c;
    }
    if (!inst.openRoutes) {
        const double before = distance;
        time += leg(at, 0);
        if (load > 0) e.loadedDistance += distance - before;
    }
    e.travel = travel;
    e.distance = distance;
    e.toll = toll;
    e.blocked = blocked;
    e.toll *= type ? type->tollFactor : 1.0f;
    e.end = time;
    if (tw && !inst.openRoutes) e.lateness += std::max(0.0, time - inst.horizon());
    if (type && !route.empty()) {
        if (inst.payPerDelivery)
            e.money = type->deliveryFee * e.deliveries + type->kmFee * e.loadedDistance / 1000.0 + e.toll;
        else
            e.money = type->fixedCost + type->costPerKm * e.distance / 1000.0 + type->costPerHour * (e.end - veh.startTime) / 3600.0 +
                      e.toll;
    }
    // Identical fleets optimize time: tolls count as the time they are worth.
    e.base = type ? e.money : e.travel + e.toll * 3600.0 / std::max(1.0f, inst.valueOfTime);
    return e;
}

}  // namespace

RouteEval evaluateRoute(const VrpInstance& inst, int k, const std::vector<uint32_t>& route) {
    const VehicleType* type = inst.typeOfVehicle(k);
    const int capacity = inst.capacityOf(k);
    Arcs g;
    g.m = inst.m();
    g.invSpeed = type ? 1.0f / type->speedFactor : 1.0f;
    if (inst.layers.empty()) {
        g.time = inst.time.data();
        g.dist = inst.dist.empty() ? nullptr : inst.dist.data();
        g.toll = inst.toll.empty() ? nullptr : inst.toll.data();
        return walk<false>(inst, k, route, g, type, capacity);
    }
    const ArcLayer& L = inst.layers[std::min<size_t>(type ? type->layer : 0, inst.layers.size() - 1)];
    g.time = L.time.data();
    g.drive = L.drive.data();
    g.dist = L.dist.data();
    g.toll = L.toll.empty() ? nullptr : L.toll.data();
    g.blocked = L.blocked.empty() ? nullptr : L.blocked.data();
    return walk<true>(inst, k, route, g, type, capacity);
}

double routeCost(const VrpInstance& inst, const std::vector<uint32_t>& r) {
    if (r.empty()) return 0;
    double c = inst.t(0, r.front()) + inst.t(r.back(), 0);
    for (size_t i = 1; i < r.size(); ++i) c += inst.t(r[i - 1], r[i]);
    return c;
}

int routeLoad(const VrpInstance& inst, const std::vector<uint32_t>& r) {
    int load = 0;
    for (uint32_t c : r) load += inst.demand[c];
    return load;
}

void setFleetMix(VrpInstance& inst, std::vector<VehicleType> types, const std::vector<int>& counts) {
    inst.types = std::move(types);
    inst.typeOf.clear();
    inst.capacity = 1;
    for (size_t t = 0; t < inst.types.size(); ++t) {
        for (int i = 0; i < (t < counts.size() ? counts[t] : 0); ++i) inst.typeOf.push_back(static_cast<uint8_t>(t));
        if (t < counts.size() && counts[t] > 0) inst.capacity = std::max(inst.capacity, inst.types[t].capacity);
    }
    inst.vehicles = static_cast<int>(inst.typeOf.size());
}

void evaluate(const VrpInstance& inst, VrpSolution& sol) {
    // Vehicle-indexed routes (dynamic / mixed fleet) keep their empty slots.
    const bool indexed = inst.isDynamic() || inst.isHeterogeneous();
    if (!indexed) std::erase_if(sol.routes, [](const auto& r) { return r.empty(); });
    sol.cost = sol.travel = sol.distance = sol.money = sol.tolls = 0;
    sol.feasible = static_cast<int>(sol.routes.size()) <= inst.vehicles;
    std::vector<uint8_t> seen(inst.m(), 0);
    for (size_t k = 0; k < sol.routes.size(); ++k) {
        const auto& r = sol.routes[k];
        // Static identical fleets: any vehicle; dynamic / mixed: route k is vehicle k.
        if (r.empty() && indexed) continue;  // idle vehicle: no cost
        const RouteEval e = evaluateRoute(inst, indexed ? int(k) : 0, r);
        sol.cost += e.base;
        sol.travel += e.travel;
        sol.distance += e.distance;
        sol.money += e.money;
        sol.tolls += e.toll;
        if (!e.feasible()) sol.feasible = false;
        for (uint32_t c : r) {
            if (c == 0 || c > inst.n || seen[c]) sol.feasible = false;
            else seen[c] = 1;
        }
    }
    for (uint32_t c = 1; c <= inst.n; ++c)
        if (!seen[c]) sol.feasible = false;
}

uint32_t requiredFeatures(const VrpInstance& inst) {
    uint32_t f = kFeatCapacity;
    if (inst.hasTimeWindows()) f |= kFeatTimeWindows;
    if (inst.isPickupDelivery()) f |= kFeatPickupDelivery;
    if (inst.isDynamic()) f |= kFeatDynamic;
    if (inst.isHeterogeneous()) f |= kFeatHeterogeneous;
    return f;
}

}  // namespace mobilog::opt
