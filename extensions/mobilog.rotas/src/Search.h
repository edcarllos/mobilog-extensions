#pragma once
// The algorithms of the "Rotas e entregas" package (mobilog.rotas): nearest neighbour, Clarke-Wright,
// CW + local search, ALNS and HGS, as searches that work in slices. The same sources build the
// package (WebAssembly: Module.cpp, one search per instance, the app runs several in parallel) and,
// for the tests and the benchmark, a native solver (Native.cpp: the searches on the job system).
#include "mobilog/vrp/Solver.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace mobilog::opt {

// One search. slice() works about `ms` milliseconds, offers what it improved (ctl.offer), counts its
// iterations in ctl.iterations, and returns whether it can go on (false: a construction that is done).
class SlicedSearch {
public:
    virtual ~SlicedSearch() = default;
    virtual bool slice(int ms, SolverControl& ctl) = 0;
};

// A solver of the package: its id in the manifest ("alns"), how the app lists it (name, features,
// parameters) and whether its copies run in parallel (one search each, different seeds).
struct SolverDecl {
    const char* id;
    SolverInfo info;  // `make` and `owner` unset
    bool parallel;
};
const std::vector<SolverDecl>& routingSolvers();

// A search of solver `id` for `inst`, copy `copy` of `copies` (the seed and the warm start depend on
// it); nullptr when the id is unknown.
std::unique_ptr<SlicedSearch> makeSearch(const std::string& id, const VrpInstance& inst, const SolverControl& ctl, uint32_t copy,
                                         uint32_t copies);
std::unique_ptr<SlicedSearch> makeAlnsSearch(const VrpInstance& inst, const SolverControl& ctl, uint32_t copy, uint32_t copies);
std::unique_ptr<SlicedSearch> makeHgsSearch(const VrpInstance& inst, const SolverControl& ctl, uint32_t copy, uint32_t copies);
std::unique_ptr<SlicedSearch> makeConstructionSearch(const std::string& id, const VrpInstance& inst, const SolverControl& ctl,
                                                     uint32_t copy, uint32_t copies);

// Native (tests, benchmark): the package's solvers in the registry, run on the job system.
void registerRoutingSolvers(const std::string& owner);

}  // namespace mobilog::opt
