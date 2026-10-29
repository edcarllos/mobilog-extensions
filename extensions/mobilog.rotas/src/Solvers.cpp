// The package's CVRP heuristics and its catalog.
//  - Nearest neighbour: greedy baseline.
//  - Clarke-Wright savings (asymmetric variant): classic constructive heuristic.
//  - Savings + local search, multi-start: randomized savings restarts improved with relocate / swap /
//    2-opt, one restart after another within each slice (the app runs several copies in parallel).
#include "Search.h"

#include <algorithm>
#include <atomic>
#include <random>

namespace mobilog::opt {

namespace {

constexpr double kEps = 1e-3;

VrpSolution nearestNeighbour(const VrpInstance& inst) {
    VrpSolution sol;
    std::vector<uint8_t> done(inst.n + 1, 0);
    uint32_t remaining = inst.n;
    while (remaining > 0) {
        std::vector<uint32_t> route;
        uint32_t at = 0;
        int load = 0;
        while (true) {
            uint32_t best = 0;
            float bestT = 1e30f;
            for (uint32_t c = 1; c <= inst.n; ++c) {
                if (done[c] || load + inst.demand[c] > inst.capacity) continue;
                if (inst.t(at, c) < bestT) {
                    bestT = inst.t(at, c);
                    best = c;
                }
            }
            if (best == 0) break;
            done[best] = 1;
            --remaining;
            load += inst.demand[best];
            route.push_back(best);
            at = best;
        }
        if (route.empty()) break;  // a customer that fits no vehicle: leave unserved (infeasible)
        sol.routes.push_back(std::move(route));
    }
    evaluate(inst, sol);
    return sol;
}

// Savings for merging "... i] + [j ..." : t(i,0) + t(0,j) - λ t(i,j). `noise` randomizes the order;
// λ above 1 favours joining close customers (rounder routes), below 1 long radial ones.
VrpSolution savings(const VrpInstance& inst, float noise, std::mt19937_64* rng, float lambda = 1.0f) {
    const uint32_t n = inst.n;
    struct Saving {
        float value;
        uint32_t i, j;
    };
    std::vector<Saving> list;
    list.reserve(size_t(n) * n);
    std::uniform_real_distribution<float> jitter(1.0f - noise, 1.0f + noise);
    for (uint32_t i = 1; i <= n; ++i)
        for (uint32_t j = 1; j <= n; ++j) {
            if (i == j) continue;
            float s = inst.t(i, 0) + inst.t(0, j) - lambda * inst.t(i, j);
            if (rng && noise > 0) s *= jitter(*rng);
            if (s > 0) list.push_back({s, i, j});
        }
    // Ties (a symmetric matrix has one for each pair) broken by the pair: the same plan with any C++ library.
    std::sort(list.begin(), list.end(), [](const Saving& a, const Saving& b) {
        return a.value != b.value ? a.value > b.value : a.i != b.i ? a.i < b.i : a.j < b.j;
    });

    std::vector<std::vector<uint32_t>> routes(n + 1);
    std::vector<uint32_t> routeOf(n + 1);
    std::vector<int> load(n + 1);
    for (uint32_t c = 1; c <= n; ++c) {
        routes[c] = {c};
        routeOf[c] = c;
        load[c] = inst.demand[c];
    }
    for (const Saving& s : list) {
        uint32_t ri = routeOf[s.i], rj = routeOf[s.j];
        if (ri == rj || routes[ri].back() != s.i || routes[rj].front() != s.j) continue;
        if (load[ri] + load[rj] > inst.capacity) continue;
        for (uint32_t c : routes[rj]) routeOf[c] = ri;
        routes[ri].insert(routes[ri].end(), routes[rj].begin(), routes[rj].end());
        load[ri] += load[rj];
        routes[rj].clear();
    }
    VrpSolution sol;
    for (auto& r : routes)
        if (!r.empty()) sol.routes.push_back(std::move(r));
    evaluate(inst, sol);
    return sol;
}

// First-improvement local search: inter-route relocate and swap (O(1) deltas), intra-route
// relocate and 2-opt (recomputed, routes are short). Asymmetric costs are handled exactly.
void localSearch(const VrpInstance& inst, VrpSolution& sol, const SolverControl& ctl) {
    auto& routes = sol.routes;
    std::vector<int> load(routes.size());
    for (size_t r = 0; r < routes.size(); ++r) load[r] = routeLoad(inst, routes[r]);
    auto prevOf = [&](const std::vector<uint32_t>& r, size_t i) { return i == 0 ? 0u : r[i - 1]; };
    auto nextOf = [&](const std::vector<uint32_t>& r, size_t i) { return i + 1 == r.size() ? 0u : r[i + 1]; };
    // The matrix read straight (the loops below write the routes, and the compiler would otherwise
    // reload the instance's size and data on every lookup).
    const float* T = inst.time.data();
    const size_t m = inst.m();
    auto t = [T, m](uint32_t a, uint32_t b) { return T[a * m + b]; };
    auto cost = [&](const std::vector<uint32_t>& r) {
        if (r.empty()) return 0.0;
        double c = t(0, r.front()) + t(r.back(), 0);
        for (size_t i = 1; i < r.size(); ++i) c += t(r[i - 1], r[i]);
        return c;
    };

    bool improved = true;
    int passes = 0;
    while (improved && passes++ < 200 && !ctl.shouldStop()) {
        improved = false;

        // Inter-route relocate.
        for (size_t a = 0; a < routes.size() && !improved; ++a) {
            for (size_t i = 0; i < routes[a].size() && !improved; ++i) {
                const uint32_t c = routes[a][i];
                const uint32_t p = prevOf(routes[a], i), q = nextOf(routes[a], i);
                const double removeGain = t(p, c) + t(c, q) - t(p, q);
                for (size_t b = 0; b < routes.size() && !improved; ++b) {
                    if (b == a || load[b] + inst.demand[c] > inst.capacity) continue;
                    const auto& rb = routes[b];
                    for (size_t k = 0; k <= rb.size(); ++k) {
                        uint32_t x = k == 0 ? 0u : rb[k - 1], y = k == rb.size() ? 0u : rb[k];
                        double delta = t(x, c) + t(c, y) - t(x, y) - removeGain;
                        if (delta < -kEps) {
                            routes[a].erase(routes[a].begin() + i);
                            routes[b].insert(routes[b].begin() + k, c);
                            load[a] -= inst.demand[c];
                            load[b] += inst.demand[c];
                            improved = true;
                            break;
                        }
                    }
                }
            }
        }
        if (improved) {
            for (size_t r = routes.size(); r-- > 0;)  // drop emptied routes (fleet reduction)
                if (routes[r].empty()) {
                    routes.erase(routes.begin() + r);
                    load.erase(load.begin() + r);
                }
            continue;
        }

        // Inter-route swap.
        for (size_t a = 0; a < routes.size() && !improved; ++a) {
            for (size_t b = a + 1; b < routes.size() && !improved; ++b) {
                for (size_t i = 0; i < routes[a].size() && !improved; ++i) {
                    const uint32_t c = routes[a][i];
                    const uint32_t pa = prevOf(routes[a], i), qa = nextOf(routes[a], i);
                    for (size_t j = 0; j < routes[b].size(); ++j) {
                        const uint32_t d = routes[b][j];
                        if (load[a] - inst.demand[c] + inst.demand[d] > inst.capacity ||
                            load[b] - inst.demand[d] + inst.demand[c] > inst.capacity)
                            continue;
                        const uint32_t pb = prevOf(routes[b], j), qb = nextOf(routes[b], j);
                        double delta = t(pa, d) + t(d, qa) - t(pa, c) - t(c, qa) +
                                       t(pb, c) + t(c, qb) - t(pb, d) - t(d, qb);
                        if (delta < -kEps) {
                            std::swap(routes[a][i], routes[b][j]);
                            load[a] += inst.demand[d] - inst.demand[c];
                            load[b] += inst.demand[c] - inst.demand[d];
                            improved = true;
                            break;
                        }
                    }
                }
            }
        }
        if (improved) continue;

        // Intra-route: relocate and 2-opt (segment reversal).
        for (size_t a = 0; a < routes.size() && !improved; ++a) {
            auto& r = routes[a];
            const double base = cost(r);
            std::vector<uint32_t> cand;
            for (size_t i = 0; i < r.size() && !improved; ++i) {
                for (size_t j = i + 1; j < r.size() && !improved; ++j) {
                    cand = r;
                    std::reverse(cand.begin() + i, cand.begin() + j + 1);
                    if (cost(cand) < base - kEps) {
                        r.swap(cand);
                        improved = true;
                        break;
                    }
                    cand = r;
                    uint32_t c = cand[i];
                    cand.erase(cand.begin() + i);
                    cand.insert(cand.begin() + j, c);
                    if (cost(cand) < base - kEps) {
                        r.swap(cand);
                        improved = true;
                    }
                }
            }
        }
    }
    evaluate(inst, sol);
}

// A construction (nearest neighbour, Clarke-Wright): one solution, then done.
class ConstructionSearch final : public SlicedSearch {
public:
    ConstructionSearch(const VrpInstance& inst, bool savingsOnly) : inst_(inst), savings_(savingsOnly) {}
    bool slice(int, SolverControl& ctl) override {
        ctl.offer(savings_ ? savings(inst_, 0.0f, nullptr, ctl.param("lambda", 1.0f)) : nearestNeighbour(inst_));
        ++ctl.iterations;
        return false;
    }

private:
    const VrpInstance& inst_;
    bool savings_;
};

// Savings + local search, restarted with noise until the time is up.
class MultiStartSearch final : public SlicedSearch {
public:
    MultiStartSearch(const VrpInstance& inst, const SolverControl& ctl, uint32_t copy) : inst_(inst), copy_(copy) {
        lambda_ = ctl.param("lambda", 1.0f);
        noiseMin_ = ctl.param("noiseMin", 5.0f) / 100.0f;
        noiseMax_ = std::max(noiseMin_, ctl.param("noiseMax", 35.0f) / 100.0f);
    }
    bool slice(int ms, SolverControl& ctl) override {
        const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
        if (copy_ == 0 && restarts_ == 0) {  // one copy starts from the classic savings
            VrpSolution first = savings(inst_, 0.0f, nullptr, lambda_);
            localSearch(inst_, first, ctl);
            ctl.offer(first);
            ++restarts_;
        }
        do {
            std::mt19937_64 rng(ctl.seed * 0x9E3779B97F4A7C15ull + (uint64_t(copy_) << 32) + restarts_++);
            const float noise = std::uniform_real_distribution<float>(noiseMin_, noiseMax_)(rng);
            VrpSolution s = savings(inst_, noise, &rng, lambda_);
            localSearch(inst_, s, ctl);
            ctl.offer(s);
            ++ctl.iterations;
        } while (!ctl.shouldStop() && std::chrono::steady_clock::now() < until);
        return true;
    }

private:
    const VrpInstance& inst_;
    uint32_t copy_;
    uint64_t restarts_ = 0;
    float lambda_ = 1, noiseMin_ = 0.05f, noiseMax_ = 0.35f;
};

}  // namespace

const std::vector<SolverDecl>& routingSolvers() {
    // Parameters: key, label, what it does, default, min, max, step, format, advanced.
    static const SolverParam kLambda{"lambda", "Peso da distância entre clientes (λ)",
                                     "Na economia t(i,0) + t(0,j) − λ·t(i,j): acima de 1 junta clientes próximos (rotas mais "
                                     "redondas), abaixo de 1 favorece rotas longas e radiais. 1 é o Clarke-Wright clássico.",
                                     1.0f, 0.1f, 2.0f, 0.1f, "%.1f"};
    static const std::vector<SolverDecl> registry = {
        {"vizinho", {"Vizinho mais próximo", kFeatCapacity, nullptr}, false},
        {"cw", {"Clarke-Wright", kFeatCapacity, nullptr, {kLambda}}, false},
        {"cwls", {"CW + busca local (paralelo)", kFeatCapacity, nullptr,
         {{"noiseMin", "Ruído mínimo nas economias (%)",
           "Cada reinício sorteia um ruído entre o mínimo e o máximo e multiplica as economias por 1 ± ruído: "
           "planos iniciais diferentes para a busca local melhorar.", 5, 0, 100, 1},
          {"noiseMax", "Ruído máximo nas economias (%)", "O maior ruído de um reinício (ver o mínimo).", 35, 0, 100, 1},
          {kLambda.key, kLambda.label, kLambda.tip, kLambda.def, kLambda.min, kLambda.max, kLambda.step, kLambda.format, true}}},
         true},
        {"alns", {"ALNS (paralelo)", kFeatCapacity | kFeatTimeWindows | kFeatPickupDelivery | kFeatDynamic | kFeatHeterogeneous, nullptr,
         {{"destroyMin", "Destruição mínima (% dos pedidos)",
           "A cada iteração, a parte da solução retirada e reinserida é sorteada entre o mínimo e o máximo.", 5, 1, 50, 1},
          {"destroyMax", "Destruição máxima (% dos pedidos)", "O maior pedaço retirado numa iteração (ver o mínimo).", 30, 2, 80, 1},
          {"destroyCap", "Máximo de pedidos retirados", "Limite absoluto, para instâncias grandes não ficarem lentas por iteração.",
           40, 2, 400, 5},
          {"worseAtStart", "Pior aceita no início (%)",
           "Simulated annealing: no começo, uma solução esse tanto pior é aceita com 50% de chance (explora mais); depois, "
           "cada vez menos.", 5, 0, 50, 0.5f, "%.1f"},
          {"endTemperature", "Temperatura final (% da inicial)",
           "Até onde a temperatura cai no fim do tempo: menor = termina só aceitando melhoras.", 0.1f, 0.001f, 100, 0.05f, "%.3f", true},
          {"scoreBest", "Pontos: nova melhor solução", "Pontos dados ao par de operadores que achou a melhor solução até agora.",
           33, 0, 100, 1, "%.0f", true},
          {"scoreBetter", "Pontos: melhorou a atual", "Pontos quando a solução nova é melhor que a atual (não a melhor).", 9, 0, 100, 1,
           "%.0f", true},
          {"scoreAccepted", "Pontos: pior aceita", "Pontos quando uma solução pior é aceita pelo annealing.", 13, 0, 100, 1, "%.0f",
           true},
          {"segment", "Iterações por ajuste de pesos", "A cada tantas iterações os pesos dos operadores seguem os pontos ganhos.",
           100, 10, 2000, 10, "%.0f", true},
          {"reaction", "Reação dos pesos (%)", "Quanto o peso de um operador muda a cada ajuste (0 = nunca, 100 = só o último).", 10,
           0, 100, 1, "%.0f", true},
          {"minWeight", "Peso mínimo de um operador (%)", "Nenhum operador deixa de ser sorteado: o peso não cai abaixo disso.", 5, 0,
           100, 1, "%.0f", true},
          {"warmShare", "Buscas a partir do plano atual (%)",
           "Com um plano de partida (delivery reotimizando), essa parte das buscas paralelas começa dele; as outras, do zero.", 50, 0,
           100, 10, "%.0f", true},
          {"sliceMs", "Fatia de trabalho (ms)", "Cada busca trabalha esse tempo e devolve a thread (a simulação divide os núcleos).",
           40, 5, 500, 5, "%.0f", true}}},
         true},
        {"hgs", {"HGS · genético híbrido (paralelo)", kFeatCapacity | kFeatTimeWindows, nullptr,
         {{"mu", "População mínima (μ)", "Quantos indivíduos sobram em cada subpopulação depois da seleção.", 25, 4, 100, 1},
          {"lambda", "Filhos por geração (λ)", "Quantos indivíduos entram antes de a população ser reduzida de volta a μ.", 40, 4, 200, 1},
          {"elite", "Elite", "Os melhores que a seleção protege mesmo com pouca diversidade.", 4, 1, 25, 1},
          {"close", "Vizinhos na diversidade", "A diversidade de um indivíduo é a distância média aos tantos mais parecidos.", 5, 1, 20,
           1},
          {"gamma", "Vizinhança granular (Γ)", "A busca local só tenta mover um cliente para perto dos Γ clientes mais próximos.", 20,
           3, 100, 1, "%.0f", true},
          {"initFactor", "População inicial (× μ)", "Indivíduos aleatórios criados antes de começar os cruzamentos.", 4, 1, 10, 1,
           "%.0f", true},
          {"targetLow", "Viáveis: alvo mínimo (%)", "Abaixo disso de filhos viáveis, as penalidades sobem.", 15, 1, 90, 1, "%.0f",
           true},
          {"targetHigh", "Viáveis: alvo máximo (%)", "Acima disso de filhos viáveis, as penalidades descem.", 25, 1, 100, 1, "%.0f",
           true},
          {"penUp", "Penalidade: aumento (%)", "Quanto as penalidades de carga e de horário sobem num ajuste.", 20, 1, 200, 1, "%.0f",
           true},
          {"penDown", "Penalidade: redução (%)", "Quanto elas descem num ajuste.", 15, 1, 90, 1, "%.0f", true},
          {"adaptEvery", "Filhos por ajuste de penalidade", "A cada tantos filhos as penalidades são ajustadas.", 100, 10, 1000, 10,
           "%.0f", true},
          {"repair", "Reparo de inviáveis (%)", "Chance de um filho inviável ganhar uma segunda busca com penalidades maiores.", 50, 0,
           100, 5, "%.0f", true},
          {"repairMul", "Reparo: multiplicador da penalidade", "Quanto as penalidades crescem nessa segunda busca.", 10, 2, 100, 1,
           "%.0f", true},
          {"sliceMs", "Fatia de trabalho (ms)", "Cada ilha trabalha esse tempo e devolve a thread (a simulação divide os núcleos).", 40,
           5, 500, 5, "%.0f", true}}},
         true},
    };
    return registry;
}

std::unique_ptr<SlicedSearch> makeConstructionSearch(const std::string& id, const VrpInstance& inst, const SolverControl& ctl,
                                                     uint32_t copy, uint32_t) {
    if (id == "vizinho") return std::make_unique<ConstructionSearch>(inst, false);
    if (id == "cw") return std::make_unique<ConstructionSearch>(inst, true);
    if (id == "cwls") return std::make_unique<MultiStartSearch>(inst, ctl, copy);
    return nullptr;
}

std::unique_ptr<SlicedSearch> makeSearch(const std::string& id, const VrpInstance& inst, const SolverControl& ctl, uint32_t copy,
                                         uint32_t copies) {
    if (id == "alns") return makeAlnsSearch(inst, ctl, copy, copies);
    if (id == "hgs") return makeHgsSearch(inst, ctl, copy, copies);
    return makeConstructionSearch(id, inst, ctl, copy, copies);
}

}  // namespace mobilog::opt
