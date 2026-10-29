#pragma once
#include "mobilog/vrp/Vrp.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#if !defined(__wasm__)
#include <mutex>
#endif

namespace mobilog::opt {

// A solver package compiled to WebAssembly runs single-threaded (its copies run in parallel, each in
// its own instance): no mutex there.
#if defined(__wasm__)
struct SolverMutex {
    void lock() {}
    void unlock() {}
};
#else
using SolverMutex = std::mutex;
#endif

// Shared between a running solver (worker threads) and the UI (main thread).
class SolverControl {
public:
    explicit SolverControl(double timeLimitSec, uint64_t seed = 1)
        : timeLimit(timeLimitSec), seed(seed), start_(std::chrono::steady_clock::now()) {}

    const double timeLimit;
    const uint64_t seed;
    // The algorithm's parameters for this run (Otimização → Algoritmos), copied when it starts.
    std::vector<std::pair<std::string, float>> params;
    float param(const char* key, float def) const {
        for (const auto& [k, v] : params)
            if (k == key) return v;
        return def;
    }
    // Optional warm start (dynamic re-optimization keeps the current plan as a baseline).
    std::shared_ptr<const VrpSolution> initial;
    std::atomic<bool> cancel{false};
    std::atomic<uint64_t> iterations{0};

    double elapsed() const {
        double end = finished_.load();
        if (end >= 0) return end;
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - start_).count();
    }
    void finish() { finished_ = elapsed(); }  // freezes elapsed()
    // The clock starts at construction; a control made ahead of time restarts it just before solving.
    void restart() { start_ = std::chrono::steady_clock::now(); }
    bool shouldStop() const { return cancel.load(std::memory_order_relaxed) || elapsed() >= timeLimit; }

    // Thread-safe: keeps the solution if it beats the incumbent. Returns true if improved.
    bool offer(const VrpSolution& s);
    bool best(VrpSolution& out) const;
    double bestCost() const { return bestCost_.load(); }
    std::vector<std::pair<double, double>> history() const;  // (seconds, cost)

private:
    std::chrono::steady_clock::time_point start_;
    [[maybe_unused]] mutable SolverMutex mutex_;  // the WebAssembly package locks nothing
    VrpSolution best_;
    bool hasBest_ = false;
    std::atomic<double> bestCost_{1e300};
    std::atomic<double> finished_{-1.0};
    std::vector<std::pair<double, double>> history_;
};

// A pluggable optimization algorithm. Implementations may use jobs() for parallelism and
// must poll control.shouldStop() regularly.
class ISolver {
public:
    virtual ~ISolver() = default;
    virtual const char* name() const = 0;
    virtual const char* description() const = 0;
    virtual uint32_t features() const { return kFeatCapacity; }  // VrpFeature bitmask
    virtual void solve(const VrpInstance& inst, SolverControl& control) = 0;
};

// A tunable number of an algorithm, shown in Otimização → Algoritmos (the main ones; the rest under
// Avançado). The solver reads it with SolverControl::param(key, its default).
struct SolverParam {
    std::string key;
    std::string label;
    std::string tip;
    float def, min, max, step;
    std::string format = "%.0f";
    bool advanced = false;
};

struct SolverInfo {
    std::string name;
    uint32_t features;  // VrpFeature bitmask: which instances it can solve
    std::function<std::unique_ptr<ISolver>()> make;
    std::vector<SolverParam> params = {};
    std::vector<float> values = {};  // the user's values (defaults until changed), one per param
    std::string owner = {};          // the extension that registered it ("mobilog.rotas", "br.ufsb.2opt")
    bool supports(const VrpInstance& inst) const {
        const uint32_t need = requiredFeatures(inst);
        return (features & need) == need;
    }
    float value(size_t i) const { return i < values.size() ? values[i] : params[i].def; }
    bool isDefault() const {
        for (size_t i = 0; i < params.size(); ++i)
            if (value(i) != params[i].def) return false;
        return true;
    }
    void restoreDefaults() {
        values.clear();
        for (const SolverParam& p : params) values.push_back(p.def);
    }
    // For SolverControl::params.
    std::vector<std::pair<std::string, float>> paramValues() const {
        std::vector<std::pair<std::string, float>> out;
        for (size_t i = 0; i < params.size(); ++i) out.push_back({params[i].key, value(i)});
        return out;
    }
};

// The solvers the extensions offer (an extension point of the core: "resolvedores VRP"). Extensions
// register theirs when they turn on and remove them when they turn off; whoever runs a VRP picks
// from here. Main thread.
std::vector<SolverInfo>& solverRegistry();
// The parameters of the solver called `name` (empty when unknown), for a run.
std::vector<std::pair<std::string, float>> solverParamsFor(const std::string& name);
// Adds one (its parameters at their defaults); false when the name is taken.
bool registerSolver(SolverInfo info);
void unregisterSolvers(const std::string& owner);

}  // namespace mobilog::opt
