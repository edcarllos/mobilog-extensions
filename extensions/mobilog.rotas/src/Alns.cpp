// Adaptive Large Neighborhood Search (Ropke & Pisinger 2006) for the rich VRP model:
// capacity, time windows, pickup-and-delivery pairs and vehicles already on the road.
//
//  destroy: random, worst (removal gain), Shaw relatedness, whole route
//  repair:  greedy (cheapest insertion), regret-2, regret-3, with per-route insertion caches
//  accept:  simulated annealing on a time-based cooling schedule
//  weights: adaptive roulette (scores 33 / 9 / 13, segments of 100 iterations)
//
// Several independent searches (one per copy: the app runs the copies in parallel) work in short
// slices, sharing the incumbent through SolverControl, so the traffic simulation keeps its threads.
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
constexpr double kUnassignedPenalty = 1e6;  // per request left out

struct Request {
    uint32_t a;        // customer, or pickup
    int32_t b = -1;    // delivery of a PDP pair
    int fixed = -1;    // vehicle that must serve it (goods on board)
};

struct Insertion {
    double delta = kInf;
    int route = -1;
    uint32_t posA = 0, posB = 0;  // insert a at posA, then b at posB (indices in the final route)
};

class Alns {
public:
    // Otimização → Algoritmos (the defaults: Ropke & Pisinger 2006, tuned here).
    struct Params {
        double destroyMin = 0.05, destroyMax = 0.30;  // share of the requests removed per iteration
        int destroyCap = 40;                          // at most this many
        double worseAtStart = 0.05;                   // a solution this much worse accepted with p = 0.5 at the start
        double endTemperature = 1e-3;                 // relative to the start, at the end of the time
        double scoreBest = 33, scoreBetter = 9, scoreAccepted = 13;
        int segment = 100;                            // iterations between weight updates
        double reaction = 0.1, minWeight = 0.05;
    };
    Alns(const VrpInstance& inst, uint64_t seed, bool softTimeWindows, const Params& params)
        : inst_(inst), rng_(seed), soft_(softTimeWindows), p_(params) {
        // Requests: PDP pairs (keyed by pickup), orphan deliveries (on board), or single customers.
        reqOf_.assign(inst.m(), -1);
        for (uint32_t c = 1; c <= inst.n; ++c) {
            if (inst.isPickupDelivery() && inst.pair[c] >= 0 && inst.demand[c] < 0) continue;  // delivery: with its pickup
            Request r{c};
            if (inst.isPickupDelivery() && inst.pair[c] >= 0) r.b = inst.pair[c];
            r.fixed = inst.fixedTo(c);
            reqOf_[c] = static_cast<int>(reqs_.size());
            if (r.b >= 0) reqOf_[r.b] = static_cast<int>(reqs_.size());
            reqs_.push_back(r);
        }
        K_ = inst.vehicles;
        routes_.assign(K_, {});
        cost_.assign(K_, 0.0);
        // Soft weights are per second of travel; with a money objective (mixed fleet) convert them
        // with the fleet's average cost of one second of route time.
        double unit = 1.0;
        if (inst.isHeterogeneous()) {
            double perHour = 0;
            for (int k = 0; k < inst.vehicles; ++k) perHour += inst.typeOfVehicle(k)->costPerHour;
            unit = std::max(perHour / std::max(1, inst.vehicles), 1.0) / 3600.0;
        }
        pen_.lateness = soft_ ? 5.0 * unit : 1e4;
        // Dynamic delivery: also minimize how long customers wait (balances the fleet).
        pen_.completion = soft_ ? 0.5 * unit : 0.0;
        dWeights_.assign(4, 1.0);
        rWeights_.assign(3, 1.0);
        dScores_.assign(4, 0.0);
        rScores_.assign(3, 0.0);
        dUses_.assign(4, 0);
        rUses_.assign(3, 0);
    }

    void start(const VrpSolution* initial) {
        std::vector<uint8_t> placed(reqs_.size(), 0);
        if (initial && initial->routes.size() == size_t(K_)) {
            for (int k = 0; k < K_; ++k)
                for (uint32_t c : initial->routes[k])
                    if (c >= 1 && c <= inst_.n && reqOf_[c] >= 0) {
                        routes_[k].push_back(c);
                        placed[reqOf_[c]] = 1;
                    }
        }
        for (int k = 0; k < K_; ++k) cost_[k] = routeCost(k, routes_[k]);
        unassigned_.clear();
        for (size_t r = 0; r < reqs_.size(); ++r)
            if (!placed[r]) unassigned_.push_back(static_cast<uint32_t>(r));
        repair(0);
        current_ = snapshot();
        best_ = current_;
        const double c = total();
        currentCost_ = bestCost_ = c;
        // Accept a solution worseAtStart (5%) worse with probability 0.5 at the start (penalties excluded).
        const double travel = std::accumulate(cost_.begin(), cost_.end(), 0.0);
        T0_ = std::max(1.0, p_.worseAtStart * travel / std::log(2.0));
    }

    // Runs until `until` (steady clock); temperature follows the overall time budget.
    void run(std::chrono::steady_clock::time_point until, const SolverControl& ctl, double budget) {
        while (std::chrono::steady_clock::now() < until && !ctl.shouldStop()) {
            iterate(ctl.elapsed() / std::max(budget, 1e-3));
            ++iterations;
        }
    }

    VrpSolution bestSolution() const {
        VrpSolution s;
        s.routes = best_.routes;
        evaluate(inst_, s);
        return s;
    }
    double bestCost() const { return bestCost_; }
    uint64_t iterations = 0;

private:
    struct Snapshot {
        std::vector<std::vector<uint32_t>> routes;
        std::vector<uint32_t> unassigned;
    };

    double routeCost(int k, const std::vector<uint32_t>& r) const {
        if (r.empty()) return 0.0;
        return pen_.cost(evaluateRoute(inst_, k, r));
    }
    double total() const {
        return std::accumulate(cost_.begin(), cost_.end(), 0.0) + kUnassignedPenalty * unassigned_.size();
    }
    Snapshot snapshot() const { return {routes_, unassigned_}; }
    void restore(const Snapshot& s) {
        routes_ = s.routes;
        unassigned_ = s.unassigned;
        for (int k = 0; k < K_; ++k) cost_[k] = routeCost(k, routes_[k]);
    }
    int roulette(const std::vector<double>& w) {
        double sum = std::accumulate(w.begin(), w.end(), 0.0);
        double x = std::uniform_real_distribution<double>(0, sum)(rng_);
        for (size_t i = 0; i < w.size(); ++i)
            if ((x -= w[i]) <= 0) return static_cast<int>(i);
        return static_cast<int>(w.size() - 1);
    }

    // ---- removal ---------------------------------------------------------------------
    std::vector<uint32_t> assignedRequests() const {
        std::vector<uint32_t> out;
        for (const auto& r : routes_)
            for (uint32_t c : r)
                if (reqs_[reqOf_[c]].a == c) out.push_back(static_cast<uint32_t>(reqOf_[c]));
        return out;
    }
    void removeRequest(uint32_t q) {
        const Request& r = reqs_[q];
        for (int k = 0; k < K_; ++k) {
            auto& route = routes_[k];
            const size_t before = route.size();
            std::erase_if(route, [&](uint32_t c) { return c == r.a || (r.b >= 0 && c == uint32_t(r.b)); });
            if (route.size() != before) cost_[k] = routeCost(k, route);
        }
        unassigned_.push_back(q);
    }
    void destroy(int op, size_t q) {
        std::vector<uint32_t> assigned = assignedRequests();
        if (assigned.empty()) return;
        q = std::min(q, assigned.size());
        switch (op) {
            case 0: {  // random
                std::shuffle(assigned.begin(), assigned.end(), rng_);
                for (size_t i = 0; i < q; ++i) removeRequest(assigned[i]);
                break;
            }
            case 1: {  // worst: largest cost saving when removed (randomized, p = 3)
                std::vector<std::pair<double, uint32_t>> gain;
                for (uint32_t r : assigned) {
                    const Request& rq = reqs_[r];
                    for (int k = 0; k < K_; ++k) {
                        auto& route = routes_[k];
                        if (std::find(route.begin(), route.end(), rq.a) == route.end()) continue;
                        std::vector<uint32_t> without;
                        for (uint32_t c : route)
                            if (c != rq.a && !(rq.b >= 0 && c == uint32_t(rq.b))) without.push_back(c);
                        gain.push_back({cost_[k] - routeCost(k, without), r});
                        break;
                    }
                }
                std::sort(gain.begin(), gain.end(), [](auto& x, auto& y) { return x.first > y.first; });
                std::vector<uint8_t> taken(gain.size(), 0);
                for (size_t i = 0; i < q; ++i) {
                    double y = std::uniform_real_distribution<double>(0, 1)(rng_);
                    size_t idx = static_cast<size_t>(std::pow(y, 3.0) * gain.size());
                    while (idx < gain.size() && taken[idx]) ++idx;
                    if (idx >= gain.size()) idx = std::find(taken.begin(), taken.end(), 0) - taken.begin();
                    if (idx >= gain.size()) break;
                    taken[idx] = 1;
                    removeRequest(gain[idx].second);
                }
                break;
            }
            case 2: {  // Shaw: travel-time and time-window relatedness to a random seed request
                const uint32_t seedReq = assigned[std::uniform_int_distribution<size_t>(0, assigned.size() - 1)(rng_)];
                auto rel = [&](uint32_t x, uint32_t y) {
                    const Request &a = reqs_[x], &b = reqs_[y];
                    double v = inst_.t(a.a, b.a) + inst_.t(b.a, a.a);
                    if (a.b >= 0 && b.b >= 0) v += inst_.t(a.b, b.b) + inst_.t(b.b, a.b);
                    if (inst_.hasTimeWindows()) v += 0.5 * std::abs(inst_.ready[a.a] - inst_.ready[b.a]);
                    return v;
                };
                std::sort(assigned.begin(), assigned.end(),
                          [&](uint32_t x, uint32_t y) { return rel(seedReq, x) < rel(seedReq, y); });
                for (size_t i = 0; i < q; ++i) {
                    double y = std::uniform_real_distribution<double>(0, 1)(rng_);
                    size_t idx = static_cast<size_t>(std::pow(y, 6.0) * assigned.size());
                    removeRequest(assigned[idx]);
                    assigned.erase(assigned.begin() + idx);
                    if (assigned.empty()) break;
                }
                break;
            }
            case 3: {  // route removal: empty one or two routes entirely
                for (int rep = 0; rep < 2 && q > 0; ++rep) {
                    std::vector<int> nonEmpty;
                    for (int k = 0; k < K_; ++k)
                        if (!routes_[k].empty()) nonEmpty.push_back(k);
                    if (nonEmpty.empty()) break;
                    const int k = nonEmpty[std::uniform_int_distribution<size_t>(0, nonEmpty.size() - 1)(rng_)];
                    std::vector<uint32_t> inRoute;
                    for (uint32_t c : routes_[k])
                        if (reqs_[reqOf_[c]].a == c) inRoute.push_back(static_cast<uint32_t>(reqOf_[c]));
                    for (uint32_t r : inRoute) removeRequest(r);
                    q = inRoute.size() >= q ? 0 : q - inRoute.size();
                }
                break;
            }
        }
    }

    // ---- insertion ---------------------------------------------------------------------
    Insertion bestInsertion(uint32_t q, int k) {
        Insertion best;
        const Request& r = reqs_[q];
        if (r.fixed >= 0 && r.fixed != k) return best;
        const auto& route = routes_[k];
        const size_t len = route.size();
        std::vector<uint32_t>& cand = scratch_;
        auto consider = [&](size_t pa, size_t pb) {
            const RouteEval e = evaluateRoute(inst_, k, cand);
            if (!soft_ && !e.feasible()) return;
            if (soft_ && (e.overload > 0 || e.precedence > 0)) return;
            const double d = pen_.cost(e) - cost_[k];
            if (d < best.delta) best = {d, k, uint32_t(pa), uint32_t(pb)};
        };
        if (r.b < 0) {
            // The stop placed first, then moved one position at a time (the same candidates, no copies).
            cand.assign(1, r.a);
            cand.insert(cand.end(), route.begin(), route.end());
            for (size_t i = 0; i <= len; ++i) {
                if (i) std::swap(cand[i - 1], cand[i]);
                consider(i, 0);
            }
        } else {
            for (size_t i = 0; i <= len; ++i)
                for (size_t j = i + 1; j <= len + 1; ++j) {
                    cand.assign(route.begin(), route.end());
                    cand.insert(cand.begin() + i, r.a);
                    cand.insert(cand.begin() + j, uint32_t(r.b));
                    consider(i, j);
                }
        }
        return best;
    }
    void apply(uint32_t q, const Insertion& ins) {
        const Request& r = reqs_[q];
        auto& route = routes_[ins.route];
        route.insert(route.begin() + ins.posA, r.a);
        if (r.b >= 0) route.insert(route.begin() + ins.posB, uint32_t(r.b));
        cost_[ins.route] = routeCost(ins.route, route);
        std::erase(unassigned_, q);
    }
    // Greedy (regret = 0) or regret-k insertion with a per-route cache of best insertions.
    void repair(int regret) {
        std::shuffle(unassigned_.begin(), unassigned_.end(), rng_);
        std::vector<uint32_t> pending = unassigned_;
        std::vector<std::vector<Insertion>> cache(pending.size(), std::vector<Insertion>(K_));
        for (size_t i = 0; i < pending.size(); ++i)
            for (int k = 0; k < K_; ++k) cache[i][k] = bestInsertion(pending[i], k);
        while (!pending.empty()) {
            size_t pick = SIZE_MAX;
            double pickScore = -kInf, pickDelta = kInf;
            for (size_t i = 0; i < pending.size(); ++i) {
                std::vector<double> d;
                for (const Insertion& ins : cache[i])
                    if (ins.route >= 0) d.push_back(ins.delta);
                if (d.empty()) continue;
                std::sort(d.begin(), d.end());
                double score;
                if (regret == 0) {
                    score = -d[0];
                } else {
                    score = 0;
                    for (int r = 1; r < regret; ++r) score += (size_t(r) < d.size() ? d[r] : 1e9) - d[0];
                }
                if (score > pickScore + 1e-9 || (std::abs(score - pickScore) <= 1e-9 && d[0] < pickDelta)) {
                    pick = i;
                    pickScore = score;
                    pickDelta = d[0];
                }
            }
            if (pick == SIZE_MAX) break;  // nothing insertable: stays unassigned
            const Insertion best =
                *std::min_element(cache[pick].begin(), cache[pick].end(),
                                  [](const Insertion& a, const Insertion& b) { return a.delta < b.delta; });
            apply(pending[pick], best);
            pending.erase(pending.begin() + pick);
            cache.erase(cache.begin() + pick);
            for (size_t i = 0; i < pending.size(); ++i) cache[i][best.route] = bestInsertion(pending[i], best.route);
        }
    }

    void iterate(double progress) {
        const int d = roulette(dWeights_), r = roulette(rWeights_);
        const size_t R = reqs_.size();
        const size_t lo = std::max<size_t>(1, size_t(double(R) * p_.destroyMin + 1e-9));
        const size_t hi =
            std::max<size_t>(lo + 1, std::min<size_t>(size_t(std::max(2, p_.destroyCap)), size_t(double(R) * p_.destroyMax + 1e-9)));
        const size_t q = std::uniform_int_distribution<size_t>(lo, hi)(rng_);

        destroy(d, q);
        repair(r == 0 ? 0 : r + 1);  // 0 = greedy, 2 = regret-2, 3 = regret-3
        const double c = total();

        const double T = T0_ * std::pow(std::clamp(p_.endTemperature, 1e-9, 1.0), std::clamp(progress, 0.0, 1.0));
        double score = 0;
        if (c < bestCost_ - 1e-6) {
            best_ = snapshot();
            bestCost_ = c;
            current_ = best_;
            currentCost_ = c;
            score = p_.scoreBest;
            improved = true;
        } else if (c < currentCost_ - 1e-6) {
            current_ = snapshot();
            currentCost_ = c;
            score = p_.scoreBetter;
        } else if (std::uniform_real_distribution<double>(0, 1)(rng_) < std::exp(-(c - currentCost_) / T)) {
            current_ = snapshot();
            currentCost_ = c;
            score = p_.scoreAccepted;
        } else {
            restore(current_);
        }
        dScores_[d] += score;
        rScores_[r] += score;
        ++dUses_[d];
        ++rUses_[r];
        if (++segment_ >= std::max(1, p_.segment)) {  // adapt operator weights
            const double r = std::clamp(p_.reaction, 0.0, 1.0);
            for (size_t i = 0; i < dWeights_.size(); ++i)
                if (dUses_[i]) dWeights_[i] = (1.0 - r) * dWeights_[i] + r * dScores_[i] / dUses_[i];
            for (size_t i = 0; i < rWeights_.size(); ++i)
                if (rUses_[i]) rWeights_[i] = (1.0 - r) * rWeights_[i] + r * rScores_[i] / rUses_[i];
            for (double& w : dWeights_) w = std::max(w, p_.minWeight);
            for (double& w : rWeights_) w = std::max(w, p_.minWeight);
            std::fill(dScores_.begin(), dScores_.end(), 0.0);
            std::fill(rScores_.begin(), rScores_.end(), 0.0);
            std::fill(dUses_.begin(), dUses_.end(), 0);
            std::fill(rUses_.begin(), rUses_.end(), 0);
            segment_ = 0;
        }
    }

public:
    bool improved = false;

private:
    const VrpInstance& inst_;
    std::mt19937_64 rng_;
    bool soft_;
    Params p_;
    Penalties pen_;
    int K_ = 0;
    std::vector<Request> reqs_;
    std::vector<int> reqOf_;
    std::vector<std::vector<uint32_t>> routes_;
    std::vector<double> cost_;
    std::vector<uint32_t> unassigned_;
    std::vector<uint32_t> scratch_;
    Snapshot current_, best_;
    double currentCost_ = 0, bestCost_ = 0, T0_ = 1;
    std::vector<double> dWeights_, rWeights_, dScores_, rScores_;
    std::vector<int> dUses_, rUses_;
    int segment_ = 0;
};

// One ALNS search (a copy): parameters from the run, the warm start for a share of the copies.
class AlnsSearch final : public SlicedSearch {
public:
    AlnsSearch(const VrpInstance& inst, const SolverControl& ctl, uint32_t copy, uint32_t copies) {
        // Dynamic instances accept lateness (penalized): an order must be served even if late.
        const bool soft = inst.isDynamic();
        Alns::Params p;
        p.destroyMin = ctl.param("destroyMin", 5.0f) / 100.0;
        p.destroyMax = std::max(p.destroyMin, double(ctl.param("destroyMax", 30.0f)) / 100.0);
        p.destroyCap = int(ctl.param("destroyCap", 40.0f));
        p.worseAtStart = ctl.param("worseAtStart", 5.0f) / 100.0;
        p.endTemperature = ctl.param("endTemperature", 0.1f) / 100.0;
        p.scoreBest = ctl.param("scoreBest", 33.0f);
        p.scoreBetter = ctl.param("scoreBetter", 9.0f);
        p.scoreAccepted = ctl.param("scoreAccepted", 13.0f);
        p.segment = int(ctl.param("segment", 100.0f));
        p.reaction = ctl.param("reaction", 10.0f) / 100.0;
        p.minWeight = ctl.param("minWeight", 5.0f) / 100.0;
        // A share of the copies start from the warm start (if any), the rest from scratch.
        const uint32_t warm = uint32_t(std::lround(copies * std::clamp(ctl.param("warmShare", 50.0f), 0.0f, 100.0f) / 100.0f));
        search_ = std::make_unique<Alns>(inst, ctl.seed * 7919 + copy, soft, p);
        search_->start(copy < warm ? ctl.initial.get() : nullptr);
        first_ = true;
    }
    bool slice(int ms, SolverControl& ctl) override {
        Alns& s = *search_;
        if (first_) {
            first_ = false;
            ctl.offer(s.bestSolution());
        }
        const uint64_t before = s.iterations;
        s.run(std::chrono::steady_clock::now() + std::chrono::milliseconds(ms), ctl, ctl.timeLimit);
        ctl.iterations += s.iterations - before;
        if (s.improved) {
            s.improved = false;
            ctl.offer(s.bestSolution());
        }
        return true;
    }

private:
    std::unique_ptr<Alns> search_;
    bool first_ = false;
};

}  // namespace

std::unique_ptr<SlicedSearch> makeAlnsSearch(const VrpInstance& inst, const SolverControl& ctl, uint32_t copy, uint32_t copies) {
    return std::make_unique<AlnsSearch>(inst, ctl, copy, copies);
}

}  // namespace mobilog::opt
