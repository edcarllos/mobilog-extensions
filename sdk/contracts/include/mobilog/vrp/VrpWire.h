#pragma once
// A VRP instance and solutions as the blocks a .wasm solver reads and writes (sdk/include/mobilog_ext.h:
// "Resolvedores VRP"). The instance carries what the solver needs to search (matrices, demands,
// windows, pairs, the fleet); the app evaluates every offered solution itself (opt::evaluate).
#include "mobilog/vrp/Vrp.h"
#include "mlx/Wire.h"

namespace mobilog::opt {

// After the fields every solver reads, a second part (u8 2): per vehicle type its toll factor and
// routing layer, and the layers (per toll factor and vehicle size: times, free-flow driving part,
// meters, tolls, blocked legs). Solvers written before it stop reading earlier and still work.
void encodeVrpInstance(const VrpInstance& inst, ext::WireWriter& out);
// `whole`: nothing may follow (a warm start may, read with decodeWarmStart).
bool decodeVrpInstance(ext::WireReader& in, VrpInstance& out, bool whole = true);  // false: malformed or inconsistent
// A plan to start from (dynamic re-optimization): u8 0 (none) or 1 and the routes.
void encodeWarmStart(const std::vector<std::vector<uint32_t>>* routes, ext::WireWriter& out);
bool decodeWarmStart(ext::WireReader& in, const VrpInstance& inst, std::vector<std::vector<uint32_t>>& routes, bool& has);
// Routes: u32 count, then per route u32 length and the location indices (1..m-1).
void encodeRoutes(const std::vector<std::vector<uint32_t>>& routes, ext::WireWriter& out);
bool decodeRoutes(ext::WireReader& in, const VrpInstance& inst, std::vector<std::vector<uint32_t>>& routes);

}  // namespace mobilog::opt
