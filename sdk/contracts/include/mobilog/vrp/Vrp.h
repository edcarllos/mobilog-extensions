#pragma once
// Rich vehicle routing model shared by every solver:
//  - capacitated (CVRP): deliveries loaded at the depot;
//  - time windows (VRPTW): ready/due times and service durations, hard in final solutions;
//  - pickup and delivery (PDP): paired requests, pickup before delivery on the same vehicle;
//  - vehicles already on the road (dynamic VRP): own start location/time/load and requests
//    bound to a vehicle (goods on board).
//  - heterogeneous fleet: vehicle types with own capacity, speed and costs (objective in money).
// Travel times come from an asymmetric matrix (road network with live traffic).
#include <cstdint>
#include <string>
#include <vector>

namespace mobilog::opt {

struct VrpVehicle {
    uint32_t startLoc = 0;   // location index where the vehicle becomes available
    double startTime = 0;    // seconds
    int startLoad = 0;       // PDP: units already on board
};

enum class VehicleKind : uint8_t { Moto, Car, Van, Truck };

// A vehicle category of a mixed fleet. Travel time on an arc = driving / speedFactor + delays
// (signals, devices, congestion...: the same for every vehicle).
struct VehicleType {
    std::string name;
    VehicleKind kind = VehicleKind::Car;
    int capacity = 1;
    float speedFactor = 1.0f;
    double fixedCost = 0;    // per vehicle used [R$]
    double costPerKm = 0;    // [R$/km]
    double costPerHour = 0;  // route duration (driving + waiting + service) [R$/h]
    float tollFactor = 1.0f; // tariff relative to a car (moto 0.5, van 1.5, truck 3: axles)
    // Size for the road restrictions (R-14 peso, R-15 altura, R-16 largura); 0 = the kind's default.
    float weightT = 0, heightM = 0, widthM = 0;
    uint8_t layer = 0;       // index into VrpInstance::layers (paths for this toll factor)
    // Autonomous couriers (VrpInstance::payPerDelivery): what they are paid [R$].
    double deliveryFee = 0;  // per delivery
    double kmFee = 0;        // per km driven with goods on board
};

// Matrices of one routing layer (TravelMatrix::Layer): m x m, row = from.
struct ArcLayer {
    std::vector<float> time;       // seconds along the path (inf: unreachable)
    std::vector<float> drive;      // free-flow driving part of time (the vehicle's speed factor scales it)
    std::vector<float> dist;       // meters
    std::vector<float> toll;       // car tariffs [R$] (empty: none)
    std::vector<uint8_t> blocked;  // path needs a blocked road (empty: none)
};

struct VrpInstance {
    uint32_t n = 0;                  // request locations are 1..n; 0 is the depot
    std::vector<uint32_t> nodes;     // graph node per location (size m >= n + 1; extra = vehicle starts)
    std::vector<int> demand;         // per location; PDP: +q at pickups, -q at deliveries
    int vehicles = 1;                // fleet size (max routes)
    int capacity = 1;
    std::vector<float> time;         // m x m travel times [s], row = from

    // Time windows (all empty when the instance has none).
    std::vector<float> ready, due, service;
    // Pickup and delivery: partner location per location, -1 if none (empty when not PDP).
    std::vector<int32_t> pair;
    // Dynamic: vehicle that must serve a location (goods on board), -1 if free (may be empty).
    std::vector<int32_t> fixedVehicle;
    // Per-vehicle start (empty: everyone leaves the depot at t = 0 empty).
    std::vector<VrpVehicle> fleet;
    // Mixed fleet (empty: identical vehicles with `capacity`, objective = travel time).
    std::vector<VehicleType> types;
    std::vector<uint8_t> typeOf;     // per vehicle k: index into types
    std::vector<float> dist;         // m x m meters along the fastest paths (optional; needed for R$/km)
    std::vector<float> toll;         // m x m car tolls [R$] along those paths (optional)
    // Per toll factor (optional; layer 0 = time/dist/toll above): exact evaluation per vehicle type.
    std::vector<ArcLayer> layers;
    float valueOfTime = 25.0f;       // R$/h: tolls as time in the time objective (identical fleets)
    // Autonomous couriers: routes end at the last stop (no return to location 0, no closing time)
    // and, with a mixed fleet, the money is what they are paid: fee per delivery + per km with goods
    // on board + tolls (no fixed cost, idle time unpaid).
    bool openRoutes = false;
    bool payPerDelivery = false;

    uint32_t m() const { return static_cast<uint32_t>(nodes.size()); }
    float t(uint32_t from, uint32_t to) const { return time[size_t(from) * m() + to]; }
    bool hasTimeWindows() const { return !ready.empty(); }
    bool isPickupDelivery() const { return !pair.empty(); }
    bool isDynamic() const { return !fleet.empty(); }
    bool isPickup(uint32_t loc) const { return isPickupDelivery() && pair[loc] >= 0 && demand[loc] > 0; }
    bool isDelivery(uint32_t loc) const { return isPickupDelivery() && demand[loc] < 0; }
    int fixedTo(uint32_t loc) const { return fixedVehicle.empty() ? -1 : fixedVehicle[loc]; }
    VrpVehicle vehicle(int k) const { return fleet.empty() ? VrpVehicle{} : fleet[size_t(k)]; }
    double horizon() const { return due.empty() ? 1e18 : due[0]; }  // depot closing time
    bool isHeterogeneous() const { return !types.empty(); }
    const VehicleType* typeOfVehicle(int k) const { return types.empty() ? nullptr : &types[typeOf[size_t(k)]]; }
    int capacityOf(int k) const { return types.empty() ? capacity : typeOfVehicle(k)->capacity; }
    float d(uint32_t from, uint32_t to) const { return dist.empty() ? 0.0f : dist[size_t(from) * m() + to]; }
    float tollAt(uint32_t from, uint32_t to) const { return toll.empty() ? 0.0f : toll[size_t(from) * m() + to]; }
};

// Builds `vehicles`, `typeOf` and `capacity` (largest type) from per-type counts.
void setFleetMix(VrpInstance& inst, std::vector<VehicleType> types, const std::vector<int>& counts);

struct VrpSolution {
    // Customer location indices per route. For dynamic and mixed-fleet instances route k is driven
    // by vehicle k (routes.size() == vehicles); otherwise routes are interchangeable and empty ones dropped.
    std::vector<std::vector<uint32_t>> routes;
    double cost = 0;       // objective: travel time [s], or money [R$] for mixed fleets
    double travel = 0;     // total driving time [s]
    double distance = 0;   // total distance [m] (0 without a distance matrix)
    double money = 0;      // total cost [R$] (mixed fleets)
    double tolls = 0;      // total tolls [R$]
    bool feasible = false;  // every hard constraint satisfied and every request served
};

// Detailed evaluation of one route driven by vehicle k.
struct RouteEval {
    double travel = 0;     // seconds driving (already divided by the vehicle's speed factor)
    double distance = 0;   // meters
    double money = 0;      // fixed + per km + per hour of route duration + tolls [R$] (mixed fleets)
    double toll = 0;       // tolls paid by this vehicle [R$]
    double base = 0;       // objective term: money for mixed fleets, travel otherwise
    double lateness = 0;   // seconds past due times (incl. depot)
    int overload = 0;      // max units above capacity
    int precedence = 0;    // PDP pairs violated (missing partner, wrong order)
    int blocked = 0;       // legs that need a road blocked by an incident
    double end = 0;        // return time at the depot (open routes: end of the last stop)
    double loadedDistance = 0;  // meters driven with goods on board
    int deliveries = 0;    // delivery stops (customers)
    double completion = 0; // sum of service start times at deliveries/customers (customer waiting)
    bool feasible() const { return lateness < 1e-6 && overload == 0 && precedence == 0 && blocked == 0; }
};

RouteEval evaluateRoute(const VrpInstance& inst, int vehicle, const std::vector<uint32_t>& route);

// Search objective: travel time (or money) + penalties (solvers tune the weights).
struct Penalties {
    double lateness = 10.0;   // per second late
    double overload = 1000.0; // per unit above capacity
    double precedence = 1e5;  // per violated pair
    double completion = 0.0;  // per second of customer waiting (dynamic delivery: serve early, spread the fleet)
    double blocked = 1e5;     // per leg through a blocked road
    double cost(const RouteEval& e) const {
        return e.base + lateness * e.lateness + overload * e.overload + precedence * e.precedence +
               completion * e.completion + blocked * e.blocked;
    }
};

double routeCost(const VrpInstance& inst, const std::vector<uint32_t>& route);  // travel only, vehicle 0
int routeLoad(const VrpInstance& inst, const std::vector<uint32_t>& route);
// Recomputes cost/feasibility from scratch (independent check of any solver's output).
void evaluate(const VrpInstance& inst, VrpSolution& sol);

// Problem features a solver can declare support for.
enum VrpFeature : uint32_t {
    kFeatCapacity = 1,
    kFeatTimeWindows = 2,
    kFeatPickupDelivery = 4,
    kFeatDynamic = 8,
    kFeatHeterogeneous = 16,
};
uint32_t requiredFeatures(const VrpInstance& inst);

}  // namespace mobilog::opt
