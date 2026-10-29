// Hybrid Genetic Search (after Vidal et al. 2012 / HGS-CVRP 2022) for CVRP and VRPTW.
//
//  - individual: giant tour (permutation) + Split into at most K routes (Bellman DP)
//  - crossover: OX on giant tours; education: granular local search (relocate, swap,
//    2-opt*, intra-route 2-opt) on a penalized objective
//  - feasible / infeasible subpopulations, adaptive penalties targeting ~20% feasible
//  - survivor selection by biased fitness = cost rank + diversity (broken-pairs) rank
//
// One independent population per copy (the app runs the copies in parallel), advanced in short
// slices (see ALNS).
#include "Search.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <numeric>
#include <random>

namespace mobilog::opt {

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

struct Individual {
    std::vector<uint32_t> tour;                 // giant tour
    std::vector<std::vector<uint32_t>> routes;  // split
    double penalized = kInf;                    // with current penalty weights
    double travel = 0;
    bool feasible = false;
    std::vector<uint32_t> succ, pred;           // for broken-pairs distance (0 = depot)
    double diversity = 0, biased = 0;
};

class Hgs {
public:
    // Otimização → Algoritmos (the defaults are those of HGS-CVRP).
    struct Params {
        int mu = 25, lambda = 40, elite = 4, close = 5, gamma = 20, initFactor = 4, adaptEvery = 100;
        double targetLow = 0.15, targetHigh = 0.25, penUp = 1.2, penDown = 0.85, repairProb = 0.5, repairMul = 10;
    };
    Hgs(const VrpInstance& inst, uint64_t seed, const Params& params)
        : inst_(inst), rng_(seed), n_(inst.n), K_(inst.vehicles), p_(params) {
        // Initial penalties: capacity scaled by the average edge per demand unit, TW at 1 per second.
        double avgT = 0;
        for (uint32_t i = 0; i <= n_; ++i)
            for (uint32_t j = 0; j <= n_; ++j) avgT += inst.t(i, j);
        avgT /= double(n_ + 1) * double(n_ + 1);
        double avgD = 0;
        for (uint32_t c = 1; c <= n_; ++c) avgD += inst.demand[c];
        avgD = std::max(1.0, avgD / std::max(1u, n_));
        pen_.overload = std::max(1.0, avgT / avgD);
        pen_.lateness = 1.0;
        // Granular neighbourhoods: Γ closest customers (symmetrized travel time).
        const uint32_t gamma = std::min<uint32_t>(uint32_t(std::max(1, p_.gamma)), n_ - 1);
        neighbours_.assign(n_ + 1, {});
        for (uint32_t i = 1; i <= n_; ++i) {
            std::vector<std::pair<float, uint32_t>> d;
            for (uint32_t j = 1; j <= n_; ++j)
                if (j != i) d.push_back({inst.t(i, j) + inst.t(j, i), j});
            std::partial_sort(d.begin(), d.begin() + gamma, d.end());
            for (uint32_t k = 0; k < gamma; ++k) neighbours_[i].push_back(d[k].second);
        }
    }

    // One random individual per call (keeps time slices short).
    void addRandomIndividual() {
        Individual ind;
        ind.tour.resize(n_);
        std::iota(ind.tour.begin(), ind.tour.end(), 1u);
        std::shuffle(ind.tour.begin(), ind.tour.end(), rng_);
        educateAndInsert(std::move(ind));
        ++initialized_;
    }

    void run(std::chrono::steady_clock::time_point until, const SolverControl& ctl) {
        while (std::chrono::steady_clock::now() < until && !ctl.shouldStop()) {
            generation();
            ++iterations;
        }
    }

    bool hasBest() const { return bestTravel_ < kInf; }
    VrpSolution bestSolution() const {
        VrpSolution s;
        s.routes = best_;
        evaluate(inst_, s);
        return s;
    }
    uint64_t iterations = 0;
    bool improved = false;

private:

    // ---- evaluation ----------------------------------------------------------------------
    double cost(const std::vector<uint32_t>& r) const { return r.empty() ? 0.0 : pen_.cost(evaluateRoute(inst_, 0, r)); }

    void finalize(Individual& ind) const {
        ind.penalized = 0;
        ind.travel = 0;
        ind.feasible = static_cast<int>(std::count_if(ind.routes.begin(), ind.routes.end(),
                                                      [](auto& r) { return !r.empty(); })) <= K_;
        for (const auto& r : ind.routes) {
            if (r.empty()) continue;
            const RouteEval e = evaluateRoute(inst_, 0, r);
            ind.penalized += pen_.cost(e);
            ind.travel += e.travel;
            ind.feasible = ind.feasible && e.feasible();
        }
        ind.tour.clear();
        ind.succ.assign(n_ + 1, 0);
        ind.pred.assign(n_ + 1, 0);
        for (const auto& r : ind.routes)
            for (size_t i = 0; i < r.size(); ++i) {
                ind.tour.push_back(r[i]);
                ind.succ[r[i]] = i + 1 < r.size() ? r[i + 1] : 0;
                ind.pred[r[i]] = i > 0 ? r[i - 1] : 0;
            }
    }

    // ---- split: giant tour -> at most K routes (Bellman DP over route count) -------------
    void split(Individual& ind) const {
        const auto& tour = ind.tour;
        const size_t N = tour.size();
        const int K = std::max(1, std::min<int>(K_, static_cast<int>(N)));
        std::vector<std::vector<double>> pot(K + 1, std::vector<double>(N + 1, kInf));
        std::vector<std::vector<uint32_t>> from(K + 1, std::vector<uint32_t>(N + 1, 0));
        pot[0][0] = 0;
        const bool tw = inst_.hasTimeWindows();
        for (int k = 0; k < K; ++k) {
            for (size_t i = 0; i < N; ++i) {
                if (pot[k][i] == kInf) continue;
                // Extend a route tour[i..j] incrementally.
                double travel = 0, time = 0, late = 0;
                int load = 0;
                uint32_t prev = 0;
                for (size_t j = i; j < N; ++j) {
                    const uint32_t c = tour[j];
                    load += inst_.demand[c];
                    travel += inst_.t(prev, c);
                    if (tw) {
                        time = std::max(time + inst_.t(prev, c), double(inst_.ready[c]));
                        late += std::max(0.0, time - inst_.due[c]);
                        time += inst_.service[c];
                    }
                    prev = c;
                    const double back = inst_.t(c, 0);
                    const double lateEnd = tw ? std::max(0.0, time + back - inst_.horizon()) : 0.0;
                    const double cst = travel + back + pen_.overload * std::max(0, load - inst_.capacity) +
                                       pen_.lateness * (late + lateEnd);
                    if (pot[k][i] + cst < pot[k + 1][j + 1]) {
                        pot[k + 1][j + 1] = pot[k][i] + cst;
                        from[k + 1][j + 1] = static_cast<uint32_t>(i);
                    }
                    if (load > 2 * inst_.capacity) break;  // hopeless route
                }
            }
        }
        int bestK = 1;
        for (int k = 1; k <= K; ++k)
            if (pot[k][N] < pot[bestK][N]) bestK = k;
        ind.routes.assign(K_, {});
        size_t end = N;
        for (int k = bestK; k > 0; --k) {
            const size_t begin = from[k][end];
            ind.routes[k - 1].assign(tour.begin() + begin, tour.begin() + end);
            end = begin;
        }
    }

    // ---- local search (education) ----------------------------------------------------------
    void localSearch(Individual& ind) {
        auto& R = ind.routes;
        if (R.size() < size_t(K_)) R.resize(K_);
        std::vector<int> routeOf(n_ + 1, -1);
        std::vector<double> rc(R.size());
        auto index = [&](size_t r) {
            for (uint32_t c : R[r]) routeOf[c] = static_cast<int>(r);
            rc[r] = cost(R[r]);
        };
        for (size_t r = 0; r < R.size(); ++r) index(r);
        auto posIn = [&](const std::vector<uint32_t>& r, uint32_t c) {
            return static_cast<size_t>(std::find(r.begin(), r.end(), c) - r.begin());
        };

        std::vector<uint32_t> order(n_);
        std::iota(order.begin(), order.end(), 1u);
        std::vector<uint32_t> a, b;
        bool improved = true;
        auto commit = [&](int r1, std::vector<uint32_t>& n1, int r2, std::vector<uint32_t>& n2, double c1, double c2) {
            R[r1].swap(n1);
            rc[r1] = c1;
            if (r2 >= 0 && r2 != r1) {
                R[r2].swap(n2);
                rc[r2] = c2;
                index(r2);
            }
            index(r1);
            improved = true;
        };
        int loops = 0;
        while (improved && loops++ < 50) {
            improved = false;
            std::shuffle(order.begin(), order.end(), rng_);
            for (uint32_t u : order) {
                for (uint32_t v : neighbours_[u]) {
                    const int ru = routeOf[u], rv = routeOf[v];
                    const size_t pu = posIn(R[ru], u), pv = posIn(R[rv], v);
                    if (ru != rv) {
                        // Relocate u after v.
                        a = R[ru];
                        a.erase(a.begin() + pu);
                        b = R[rv];
                        b.insert(b.begin() + pv + 1, u);
                        double ca = cost(a), cb = cost(b);
                        if (ca + cb < rc[ru] + rc[rv] - 1e-6) {
                            commit(ru, a, rv, b, ca, cb);
                            break;
                        }
                        // Swap u and v.
                        a = R[ru];
                        b = R[rv];
                        std::swap(a[pu], b[pv]);
                        ca = cost(a);
                        cb = cost(b);
                        if (ca + cb < rc[ru] + rc[rv] - 1e-6) {
                            commit(ru, a, rv, b, ca, cb);
                            break;
                        }
                        // 2-opt*: exchange the tails after u and after v.
                        a.assign(R[ru].begin(), R[ru].begin() + pu + 1);
                        a.insert(a.end(), R[rv].begin() + pv + 1, R[rv].end());
                        b.assign(R[rv].begin(), R[rv].begin() + pv + 1);
                        b.insert(b.end(), R[ru].begin() + pu + 1, R[ru].end());
                        ca = cost(a);
                        cb = cost(b);
                        if (ca + cb < rc[ru] + rc[rv] - 1e-6) {
                            commit(ru, a, rv, b, ca, cb);
                            break;
                        }
                    } else {
                        // Intra-route relocate u after v.
                        a = R[ru];
                        a.erase(a.begin() + pu);
                        a.insert(a.begin() + posIn(a, v) + 1, u);
                        double ca = cost(a);
                        if (ca < rc[ru] - 1e-6) {
                            commit(ru, a, -1, b, ca, 0);
                            break;
                        }
                        // 2-opt: reverse the segment between them.
                        a = R[ru];
                        const size_t lo = std::min(pu, pv) + 1, hi = std::max(pu, pv);
                        if (lo < hi) {
                            std::reverse(a.begin() + lo, a.begin() + hi + 1);
                            ca = cost(a);
                            if (ca < rc[ru] - 1e-6) {
                                commit(ru, a, -1, b, ca, 0);
                                break;
                            }
                        }
                    }
                }
                // Move u into an empty route (opens a vehicle).
                const int ru = routeOf[u];
                for (size_t r = 0; r < R.size(); ++r) {
                    if (!R[r].empty() || R[ru].size() <= 1) continue;
                    a = R[ru];
                    a.erase(a.begin() + posIn(a, u));
                    b = {u};
                    const double ca = cost(a), cb = cost(b);
                    if (ca + cb < rc[ru] - 1e-6) commit(ru, a, static_cast<int>(r), b, ca, cb);
                    break;
                }
            }
        }
    }

    // ---- population ----------------------------------------------------------------------
    double brokenPairs(const Individual& x, const Individual& y) const {
        int diff = 0;
        for (uint32_t c = 1; c <= n_; ++c) {
            if (x.succ[c] != y.succ[c] && x.succ[c] != y.pred[c]) ++diff;
            if (x.pred[c] == 0 && y.pred[c] != 0 && y.succ[c] != 0) ++diff;
        }
        return double(diff) / n_;
    }

    void updateBiased(std::vector<Individual>& pop) const {
        const size_t P = pop.size();
        if (P == 0) return;
        for (size_t i = 0; i < P; ++i) {
            std::vector<double> d;
            for (size_t j = 0; j < P; ++j)
                if (j != i) d.push_back(brokenPairs(pop[i], pop[j]));
            std::sort(d.begin(), d.end());
            const size_t k = std::min<size_t>(size_t(p_.close), d.size());
            pop[i].diversity = k ? std::accumulate(d.begin(), d.begin() + k, 0.0) / k : 0.0;
        }
        std::vector<size_t> byCost(P), byDiv(P);
        std::iota(byCost.begin(), byCost.end(), 0);
        std::iota(byDiv.begin(), byDiv.end(), 0);
        std::sort(byCost.begin(), byCost.end(), [&](size_t a, size_t b) { return pop[a].penalized < pop[b].penalized; });
        std::sort(byDiv.begin(), byDiv.end(), [&](size_t a, size_t b) { return pop[a].diversity > pop[b].diversity; });
        std::vector<double> rc(P), rd(P);
        for (size_t r = 0; r < P; ++r) {
            rc[byCost[r]] = double(r) / std::max<size_t>(1, P - 1);
            rd[byDiv[r]] = double(r) / std::max<size_t>(1, P - 1);
        }
        const double w = 1.0 - double(std::min(p_.elite, int(P))) / P;
        for (size_t i = 0; i < P; ++i) pop[i].biased = rc[i] + w * rd[i];
    }

    void insert(Individual ind) {
        if (ind.feasible && ind.travel < bestTravel_ - 1e-6) {
            bestTravel_ = ind.travel;
            best_ = ind.routes;
            improved = true;
        }
        auto& pop = ind.feasible ? feasible_ : infeasible_;
        pop.push_back(std::move(ind));
        if (pop.size() > size_t(p_.mu + p_.lambda)) {
            // Survivor selection: drop clones first, then the worst biased fitness.
            while (pop.size() > size_t(p_.mu)) {
                updateBiased(pop);
                size_t worst = 0;
                bool clone = false;
                for (size_t i = 0; i < pop.size(); ++i) {
                    const bool isClone = pop[i].diversity < 1e-9;
                    if ((isClone && !clone) || (isClone == clone && pop[i].biased > pop[worst].biased)) {
                        worst = i;
                        clone = isClone;
                    }
                }
                pop.erase(pop.begin() + worst);
            }
        }
    }

    void educateAndInsert(Individual ind) {
        split(ind);
        localSearch(ind);
        finalize(ind);
        ++sinceAdapt_;
        loadFeasible_ += ind.feasible;
        if (!ind.feasible && std::uniform_real_distribution<double>(0, 1)(rng_) < p_.repairProb) {
            // Repair attempt with stronger penalties.
            Individual fixed = ind;
            const Penalties saved = pen_;
            pen_.overload *= p_.repairMul;
            pen_.lateness *= p_.repairMul;
            localSearch(fixed);
            pen_ = saved;
            finalize(fixed);
            if (fixed.feasible) insert(fixed);
        }
        insert(std::move(ind));
        if (sinceAdapt_ >= p_.adaptEvery) {  // adaptive penalties: target a share of feasible offspring
            const double frac = double(loadFeasible_) / sinceAdapt_;
            const double f = frac < p_.targetLow ? p_.penUp : (frac > p_.targetHigh ? p_.penDown : 1.0);
            pen_.overload = std::clamp(pen_.overload * f, 0.1, 1e5);
            pen_.lateness = std::clamp(pen_.lateness * f, 0.01, 1e4);
            sinceAdapt_ = loadFeasible_ = 0;
        }
    }

    const Individual& tournament() {
        auto pick = [&]() -> const Individual& {
            const size_t total = feasible_.size() + infeasible_.size();
            size_t i = std::uniform_int_distribution<size_t>(0, total - 1)(rng_);
            return i < feasible_.size() ? feasible_[i] : infeasible_[i - feasible_.size()];
        };
        const Individual& a = pick();
        const Individual& b = pick();
        return a.biased < b.biased ? a : b;
    }

    void generation() {
        if (initialized_ < p_.initFactor * p_.mu || feasible_.size() + infeasible_.size() < 2) {
            addRandomIndividual();
            return;
        }
        updateBiased(feasible_);
        updateBiased(infeasible_);
        const Individual& p1 = tournament();
        const Individual& p2 = tournament();
        // OX crossover on giant tours.
        const size_t N = p1.tour.size();
        size_t i = std::uniform_int_distribution<size_t>(0, N - 1)(rng_);
        size_t j = std::uniform_int_distribution<size_t>(0, N - 1)(rng_);
        if (i > j) std::swap(i, j);
        Individual child;
        child.tour.assign(N, 0);
        std::vector<uint8_t> used(n_ + 1, 0);
        for (size_t k = i; k <= j; ++k) {
            child.tour[k] = p1.tour[k];
            used[p1.tour[k]] = 1;
        }
        size_t w = (j + 1) % N;
        for (size_t k = 0; k < N; ++k) {
            const uint32_t c = p2.tour[(j + 1 + k) % N];
            if (used[c]) continue;
            child.tour[w] = c;
            w = (w + 1) % N;
        }
        educateAndInsert(std::move(child));
    }

    const VrpInstance& inst_;
    std::mt19937_64 rng_;
    uint32_t n_;
    int K_;
    Params p_;
    Penalties pen_;
    std::vector<std::vector<uint32_t>> neighbours_;
    std::vector<Individual> feasible_, infeasible_;
    std::vector<std::vector<uint32_t>> best_;
    double bestTravel_ = kInf;
    int sinceAdapt_ = 0, loadFeasible_ = 0;
    int initialized_ = 0;
};

// One HGS island (a copy).
class HgsSearch final : public SlicedSearch {
public:
    HgsSearch(const VrpInstance& inst, const SolverControl& ctl, uint32_t copy) {
        if (inst.n < 2) return;  // nothing to search
        Hgs::Params p;
        p.mu = int(ctl.param("mu", float(p.mu)));
        p.lambda = int(ctl.param("lambda", float(p.lambda)));
        p.elite = int(ctl.param("elite", float(p.elite)));
        p.close = int(ctl.param("close", float(p.close)));
        p.gamma = int(ctl.param("gamma", float(p.gamma)));
        p.initFactor = int(ctl.param("initFactor", float(p.initFactor)));
        p.adaptEvery = int(ctl.param("adaptEvery", float(p.adaptEvery)));
        p.targetLow = ctl.param("targetLow", 15.0f) / 100.0;
        p.targetHigh = std::max(p.targetLow, double(ctl.param("targetHigh", 25.0f)) / 100.0);
        p.penUp = 1.0 + ctl.param("penUp", 20.0f) / 100.0;
        p.penDown = 1.0 - ctl.param("penDown", 15.0f) / 100.0;
        p.repairProb = ctl.param("repair", 50.0f) / 100.0;
        p.repairMul = ctl.param("repairMul", 10.0f);
        island_ = std::make_unique<Hgs>(inst, ctl.seed * 104729 + copy, p);
    }
    bool slice(int ms, SolverControl& ctl) override {
        if (!island_) return false;
        Hgs& h = *island_;
        const uint64_t before = h.iterations;
        h.run(std::chrono::steady_clock::now() + std::chrono::milliseconds(ms), ctl);
        ctl.iterations += h.iterations - before;
        if (h.improved) {
            h.improved = false;
            ctl.offer(h.bestSolution());
        }
        return true;
    }

private:
    std::unique_ptr<Hgs> island_;
};

}  // namespace

std::unique_ptr<SlicedSearch> makeHgsSearch(const VrpInstance& inst, const SolverControl& ctl, uint32_t copy, uint32_t) {
    return std::make_unique<HgsSearch>(inst, ctl, copy);
}

}  // namespace mobilog::opt
