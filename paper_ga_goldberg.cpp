#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <queue>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;
namespace fs = std::filesystem;

struct Instance {
    int n_plants = 0;
    int n_depots = 0;
    int n_customers = 0;
    vector<double> plant_cap, depot_cap, demand;
    vector<double> plant_fixed, depot_fixed;
    vector<vector<double>> c, d;
    vector<int> ch2;
    double total_demand = 0.0;
};

struct Config {
    int population = 100;
    int generations = 2000;
    int elitism_interval = 50;
    int checkpoint_interval = 5;
    double mutation_rate = 0.03;
    int seed = 42;
    int flow_scale = 100;
    int cost_scale = 100;
    bool resume = false;
    int elitism_top_k = 100;
    bool ls_single_pass = false;
    bool elitism_final_only = false;
};

struct Edge {
    int to, rev;
    long long cap;
    long long cost;
};

static vector<string> split_csv(const string& line) {
    vector<string> out;
    string cell;
    stringstream ss(line);
    while (getline(ss, cell, ',')) out.push_back(cell);
    return out;
}

static vector<vector<string>> read_csv(const string& path) {
    ifstream in(path);
    if (!in) throw runtime_error("Cannot open " + path);
    vector<vector<string>> rows;
    string line;
    while (getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) rows.push_back(split_csv(line));
    }
    return rows;
}

static vector<vector<double>> read_double_matrix(const string& path) {
    auto rows = read_csv(path);
    vector<vector<double>> matrix;
    for (const auto& row : rows) {
        vector<double> vals;
        for (const auto& cell : row) vals.push_back(stod(cell));
        matrix.push_back(vals);
    }
    return matrix;
}

static vector<int> read_chromosome_csv(const string& path) {
    auto rows = read_csv(path);
    if (rows.empty()) throw runtime_error("Empty chromosome file " + path);
    vector<int> chrom;
    for (const auto& cell : rows.front()) chrom.push_back(stoi(cell));
    return chrom;
}

static string join_chromosome(const vector<int>& chrom) {
    stringstream ss;
    for (int i = 0; i < static_cast<int>(chrom.size()); ++i) {
        if (i) ss << ",";
        ss << chrom[i];
    }
    return ss.str();
}

static vector<int> parse_chromosome_line(const string& line) {
    vector<int> chrom;
    for (const auto& cell : split_csv(line)) chrom.push_back(stoi(cell));
    return chrom;
}

static vector<vector<int>> read_checkpoint_population(const string& path) {
    ifstream in(path);
    if (!in) throw runtime_error("Cannot open checkpoint " + path);
    string line;
    getline(in, line);
    getline(in, line);
    getline(in, line);
    getline(in, line);
    getline(in, line);
    auto dims = split_csv(line);
    if (dims.size() != 2) throw runtime_error("Invalid checkpoint dimensions");
    int pop_size = stoi(dims[0]);
    int chrom_size = stoi(dims[1]);
    vector<vector<int>> pop;
    for (int i = 0; i < pop_size; ++i) {
        if (!getline(in, line)) throw runtime_error("Truncated checkpoint population");
        auto chrom = parse_chromosome_line(line);
        if (static_cast<int>(chrom.size()) != chrom_size) throw runtime_error("Invalid checkpoint chromosome length");
        pop.push_back(std::move(chrom));
    }
    return pop;
}

static Instance load_instance(const string& dir) {
    Instance inst;
    auto plants = read_csv(dir + "/plants.csv");
    auto depots = read_csv(dir + "/depots.csv");
    auto customers = read_csv(dir + "/customers.csv");
    inst.n_plants = static_cast<int>(plants.size());
    inst.n_depots = static_cast<int>(depots.size());
    inst.n_customers = static_cast<int>(customers.size());
    for (auto& row : plants) {
        inst.plant_cap.push_back(stod(row[1]));
        inst.plant_fixed.push_back(stod(row[2]));
    }
    for (auto& row : depots) {
        inst.depot_cap.push_back(stod(row[1]));
        inst.depot_fixed.push_back(stod(row[2]));
    }
    for (auto& row : customers) {
        inst.demand.push_back(stod(row[1]));
        inst.total_demand += inst.demand.back();
    }
    inst.c = read_double_matrix(dir + "/plant_depot_cost.csv");
    inst.d = read_double_matrix(dir + "/depot_customer_cost.csv");
    auto ch2_rows = read_csv(dir + "/ch2.csv");
    if (!ch2_rows.empty()) {
        for (auto& cell : ch2_rows[0]) inst.ch2.push_back(stoi(cell));
    }
    return inst;
}

class MinCostFlow {
public:
    explicit MinCostFlow(int n)
        : g(n),
          price(n),
          excess(n),
          in_queue(n, false),
          current(n, 0),
          cost_multiplier(max(1, n + 1)) {}

    void add_edge(int from, int to, long long cap, long long cost) {
        cost *= cost_multiplier;
        Edge a{to, static_cast<int>(g[to].size()), cap, cost};
        Edge b{from, static_cast<int>(g[from].size()), 0, -cost};
        g[from].push_back(a);
        g[to].push_back(b);
    }

    pair<bool, long long> solve(int s, int t, long long need) {
        fill(price.begin(), price.end(), 0);
        fill(excess.begin(), excess.end(), 0);
        fill(in_queue.begin(), in_queue.end(), false);
        fill(current.begin(), current.end(), 0);

        excess[s] = need;
        excess[t] = -need;
        long long epsilon = 1;
        for (const auto& adj : g) {
            for (const auto& e : adj) epsilon = max(epsilon, llabs(e.cost));
        }

        while (epsilon >= 1) {
            if (!refine(epsilon)) return {false, 0};
            epsilon /= 2;
        }

        for (long long value : excess) {
            if (value != 0) return {false, 0};
        }
        return {true, total_cost() / cost_multiplier};
    }

private:
    vector<vector<Edge>> g;
    vector<long long> price, excess;
    vector<char> in_queue;
    vector<int> current;
    long long cost_multiplier;

    long long reduced_cost(int from, const Edge& e) const {
        return e.cost + price[from] - price[e.to];
    }

    void enqueue(queue<int>& active, int node) {
        if (excess[node] > 0 && !in_queue[node]) {
            active.push(node);
            in_queue[node] = true;
        }
    }

    void push(int from, int edge_index, queue<int>& active) {
        Edge& e = g[from][edge_index];
        long long delta = min(excess[from], e.cap);
        if (delta <= 0) return;
        e.cap -= delta;
        g[e.to][e.rev].cap += delta;
        excess[from] -= delta;
        excess[e.to] += delta;
        enqueue(active, e.to);
    }

    void relabel(int node, long long epsilon) {
        price[node] -= epsilon;
        current[node] = 0;
    }

    bool discharge(int node, long long epsilon, queue<int>& active) {
        int scans = 0;
        int limit = max(1, static_cast<int>(g.size()) * 8);
        while (excess[node] > 0) {
            bool pushed = false;
            for (int& i = current[node]; i < static_cast<int>(g[node].size()); ++i) {
                Edge& e = g[node][i];
                if (e.cap > 0 && reduced_cost(node, e) < 0) {
                    push(node, i, active);
                    pushed = true;
                    if (excess[node] == 0) break;
                }
            }
            if (!pushed) {
                relabel(node, epsilon);
                if (++scans > limit * 1000) return false;
            }
        }
        return true;
    }

    bool refine(long long epsilon) {
        queue<int> active;
        fill(current.begin(), current.end(), 0);

        // Cost-scaling refine starts by saturating all negative reduced-cost
        // residual arcs; otherwise the pseudo-flow can be feasible but not
        // epsilon-optimal after discharge.
        for (int u = 0; u < static_cast<int>(g.size()); ++u) {
            for (int i = 0; i < static_cast<int>(g[u].size()); ++i) {
                Edge& e = g[u][i];
                if (e.cap <= 0 || reduced_cost(u, e) >= 0) continue;
                long long delta = e.cap;
                e.cap -= delta;
                g[e.to][e.rev].cap += delta;
                excess[u] -= delta;
                excess[e.to] += delta;
            }
        }

        for (int v = 0; v < static_cast<int>(g.size()); ++v) enqueue(active, v);

        long long iterations = 0;
        long long max_iterations = static_cast<long long>(g.size()) * 100000;
        while (!active.empty()) {
            int v = active.front();
            active.pop();
            in_queue[v] = false;
            if (excess[v] <= 0) continue;
            if (!discharge(v, epsilon, active)) return false;
            if (++iterations > max_iterations) return false;
        }
        return true;
    }

    long long total_cost() const {
        long long cost = 0;
        for (int u = 0; u < static_cast<int>(g.size()); ++u) {
            for (const Edge& e : g[u]) {
                if (e.cost >= 0) {
                    const Edge& rev = g[e.to][e.rev];
                    cost += rev.cap * e.cost;
                }
            }
        }
        return cost;
    }
};

class Solver {
public:
    Solver(Instance inst, Config cfg) : inst(std::move(inst)), cfg(cfg), rng(cfg.seed) {}

    vector<int> ch1() {
        vector<int> y(inst.n_plants, 0), z(inst.n_depots, 0);
        vector<double> plant_score(inst.n_plants);
        for (int i = 0; i < inst.n_plants; ++i) {
            double sum_c = accumulate(inst.c[i].begin(), inst.c[i].end(), 0.0);
            plant_score[i] = (inst.plant_fixed[i] + sum_c) / inst.plant_cap[i];
        }
        open_until(y, inst.plant_cap, plant_score);

        vector<double> depot_score(inst.n_depots);
        for (int j = 0; j < inst.n_depots; ++j) {
            double inbound = 0.0;
            for (int i = 0; i < inst.n_plants; ++i) inbound += inst.c[i][j] * y[i];
            double outbound = accumulate(inst.d[j].begin(), inst.d[j].end(), 0.0);
            depot_score[j] = (inbound + inst.depot_fixed[j] + outbound) / inst.depot_cap[j];
        }
        open_until(z, inst.depot_cap, depot_score);

        vector<int> chrom = y;
        chrom.insert(chrom.end(), z.begin(), z.end());
        return chrom;
    }

    double evaluate(const vector<int>& chrom) {
        string key;
        key.reserve(chrom.size());
        for (int bit : chrom) key.push_back(bit ? '1' : '0');
        auto it = cache.find(key);
        if (it != cache.end()) return it->second;
        double val = evaluate_uncached(chrom);
        cache[key] = val;
        return val;
    }

    bool feasible(const vector<int>& chrom) const {
        double pc = 0.0, dc = 0.0;
        for (int i = 0; i < inst.n_plants; ++i) pc += inst.plant_cap[i] * chrom[i];
        for (int j = 0; j < inst.n_depots; ++j) dc += inst.depot_cap[j] * chrom[inst.n_plants + j];
        return pc + 1e-9 >= inst.total_demand && dc + 1e-9 >= inst.total_demand;
    }

    void solve(const string& out_dir) {
        fs::create_directories(out_dir);
        auto start = chrono::steady_clock::now();
        string checkpoint_path = out_dir + "/ga_checkpoint.txt";
        vector<vector<int>> pop;
        vector<int> best;
        double best_score = numeric_limits<double>::infinity();
        int start_generation = 1;

        ofstream log(out_dir + "/ga_progress.log", ios::app);
        auto log_progress = [&](int gen, const string& note) {
            double rt = chrono::duration<double>(chrono::steady_clock::now() - start).count();
            log << "generation=" << gen << ", runtime=" << fixed << setprecision(2) << rt
                << "s, best_objective=" << setprecision(6) << best_score << ", note=" << note << "\n";
            log.flush();
        };

        if (cfg.resume && fs::exists(checkpoint_path)) {
            int saved_generation = load_checkpoint(checkpoint_path, pop, best, best_score);
            start_generation = saved_generation + 1;
            log_progress(saved_generation, "resume");
        } else {
            if (static_cast<int>(inst.ch2.size()) == inst.n_plants + inst.n_depots) pop.push_back(inst.ch2);
            while (static_cast<int>(pop.size()) < cfg.population) pop.push_back(ch1());
            vector<double> scores;
            for (auto& ind : pop) scores.push_back(evaluate(ind));
            int bi = static_cast<int>(min_element(scores.begin(), scores.end()) - scores.begin());
            best = pop[bi];
            best_score = scores[bi];
            save_checkpoint(checkpoint_path, 0, pop, best, best_score);
            log_progress(0, "start");
        }

        for (int gen = start_generation; gen <= cfg.generations; ++gen) {
            vector<vector<int>> offspring;
            while (static_cast<int>(offspring.size()) < cfg.population) {
                int a = random_index(pop.size());
                int b = random_index(pop.size());
                while (b == a && pop.size() > 1) b = random_index(pop.size());
                auto child = mutate(crossover(pop[a], pop[b]));
                if (feasible(child)) offspring.push_back(std::move(child));
            }
            pop.insert(pop.end(), offspring.begin(), offspring.end());
            vector<pair<double, int>> order;
            for (int i = 0; i < static_cast<int>(pop.size()); ++i) order.push_back({evaluate(pop[i]), i});
            sort(order.begin(), order.end());
            vector<vector<int>> next_pop;
            for (int i = 0; i < cfg.population; ++i) next_pop.push_back(pop[order[i].second]);
            pop.swap(next_pop);

            double cur_best = evaluate(pop[0]);
            if (cur_best + 1e-9 < best_score) {
                best = intensify(pop[0], cfg.ls_single_pass);
                best_score = evaluate(best);
                pop[0] = best;
            }

            bool apply_elitism = cfg.elitism_final_only
                ? (gen == cfg.generations)
                : (gen % cfg.elitism_interval == 0);
            if (apply_elitism) {
                int limit = min(cfg.elitism_top_k, static_cast<int>(pop.size()));
                for (int i = 0; i < limit; ++i) {
                    pop[i] = intensify(pop[i], cfg.ls_single_pass);
                    double sc = evaluate(pop[i]);
                    if (sc + 1e-9 < best_score) {
                        best_score = sc;
                        best = pop[i];
                    }
                    log_progress(gen, "elitism " + to_string(i + 1) + "/" + to_string(limit));
                }
                vector<pair<double, int>> order_after_ls;
                for (int i = 0; i < static_cast<int>(pop.size()); ++i) order_after_ls.push_back({evaluate(pop[i]), i});
                sort(order_after_ls.begin(), order_after_ls.end());
                vector<vector<int>> sorted_pop;
                for (auto [unused_score, idx] : order_after_ls) sorted_pop.push_back(pop[idx]);
                pop.swap(sorted_pop);
            }
            if (gen % cfg.checkpoint_interval == 0 || gen == cfg.generations) {
                save_checkpoint(checkpoint_path, gen, pop, best, best_score);
                log_progress(gen, "checkpoint");
            }
        }
        log_progress(cfg.generations, "finished");
        ofstream result(out_dir + "/result.json");
        result << "{\n";
        result << "  \"best_objective\": " << fixed << setprecision(6) << best_score << ",\n";
        result << "  \"generations\": " << cfg.generations << ",\n";
        result << "  \"population\": " << cfg.population << ",\n";
        result << "  \"elitism_interval\": " << cfg.elitism_interval << ",\n";
        result << "  \"mutation_rate\": " << cfg.mutation_rate << ",\n";
        result << "  \"checkpoint_interval\": " << cfg.checkpoint_interval << ",\n";
        result << "  \"elitism_top_k\": " << cfg.elitism_top_k << ",\n";
        result << "  \"ls_single_pass\": " << (cfg.ls_single_pass ? "true" : "false") << ",\n";
        result << "  \"elitism_final_only\": " << (cfg.elitism_final_only ? "true" : "false") << ",\n";
        result << "  \"seed\": " << cfg.seed << "\n";
        result << "}\n";
    }

private:
    Instance inst;
    Config cfg;
    mt19937 rng;
    unordered_map<string, double> cache;

    void save_checkpoint(
        const string& path,
        int generation,
        const vector<vector<int>>& pop,
        const vector<int>& best,
        double best_score
    ) {
        ofstream out(path, ios::trunc);
        if (!out) throw runtime_error("Cannot write checkpoint " + path);
        stringstream rng_state;
        rng_state << rng;
        out << generation << "\n";
        out << fixed << setprecision(12) << best_score << "\n";
        out << rng_state.str() << "\n";
        out << join_chromosome(best) << "\n";
        out << pop.size() << "," << best.size() << "\n";
        for (const auto& ind : pop) out << join_chromosome(ind) << "\n";
        out.close();
        if (!out) throw runtime_error("Failed writing checkpoint " + path);
    }

    int load_checkpoint(
        const string& path,
        vector<vector<int>>& pop,
        vector<int>& best,
        double& best_score
    ) {
        ifstream in(path);
        if (!in) throw runtime_error("Cannot open checkpoint " + path);
        string line;
        getline(in, line);
        int generation = stoi(line);
        getline(in, line);
        best_score = stod(line);
        getline(in, line);
        stringstream rng_state(line);
        rng_state >> rng;
        getline(in, line);
        best = parse_chromosome_line(line);
        getline(in, line);
        auto dims = split_csv(line);
        if (dims.size() != 2) throw runtime_error("Invalid checkpoint dimensions");
        int pop_size = stoi(dims[0]);
        int chrom_size = stoi(dims[1]);
        if (chrom_size != static_cast<int>(best.size())) throw runtime_error("Invalid checkpoint chromosome length");
        pop.clear();
        for (int i = 0; i < pop_size; ++i) {
            if (!getline(in, line)) throw runtime_error("Truncated checkpoint population");
            auto ind = parse_chromosome_line(line);
            if (static_cast<int>(ind.size()) != chrom_size) throw runtime_error("Invalid checkpoint individual length");
            pop.push_back(std::move(ind));
        }
        return generation;
    }

    int random_index(size_t n) {
        uniform_int_distribution<int> dist(0, static_cast<int>(n) - 1);
        return dist(rng);
    }

    void open_until(vector<int>& chosen, const vector<double>& cap, const vector<double>& score) {
        while (dot(chosen, cap) + 1e-9 < inst.total_demand) {
            vector<int> rem;
            vector<double> weights;
            for (int i = 0; i < static_cast<int>(chosen.size()); ++i) {
                if (!chosen[i]) {
                    rem.push_back(i);
                    weights.push_back(max(score[i], 1e-9));
                }
            }
            discrete_distribution<int> dist(weights.begin(), weights.end());
            chosen[rem[dist(rng)]] = 1;
        }
    }

    double dot(const vector<int>& x, const vector<double>& y) const {
        double s = 0.0;
        for (int i = 0; i < static_cast<int>(x.size()); ++i) s += x[i] * y[i];
        return s;
    }

    double evaluate_uncached(const vector<int>& chrom) {
        if (!feasible(chrom)) return numeric_limits<double>::infinity();
        int source = 0;
        int plant_start = 1;
        int depot_in_start = plant_start + inst.n_plants;
        int depot_out_start = depot_in_start + inst.n_depots;
        int customer_start = depot_out_start + inst.n_depots;
        int sink = customer_start + inst.n_customers;
        MinCostFlow mcf(sink + 1);
        long long total = llround(inst.total_demand * cfg.flow_scale);
        for (int i = 0; i < inst.n_plants; ++i) {
            if (!chrom[i]) continue;
            mcf.add_edge(source, plant_start + i, llround(inst.plant_cap[i] * cfg.flow_scale), 0);
            for (int j = 0; j < inst.n_depots; ++j) {
                if (!chrom[inst.n_plants + j]) continue;
                mcf.add_edge(plant_start + i, depot_in_start + j, total, llround(inst.c[i][j] * cfg.cost_scale));
            }
        }
        for (int j = 0; j < inst.n_depots; ++j) {
            if (!chrom[inst.n_plants + j]) continue;
            mcf.add_edge(depot_in_start + j, depot_out_start + j, llround(inst.depot_cap[j] * cfg.flow_scale), 0);
            for (int k = 0; k < inst.n_customers; ++k) {
                mcf.add_edge(depot_out_start + j, customer_start + k, total, llround(inst.d[j][k] * cfg.cost_scale));
            }
        }
        for (int k = 0; k < inst.n_customers; ++k) {
            mcf.add_edge(customer_start + k, sink, llround(inst.demand[k] * cfg.flow_scale), 0);
        }
        auto [ok, cost] = mcf.solve(source, sink, total);
        if (!ok) return numeric_limits<double>::infinity();
        double fixed = 0.0;
        for (int i = 0; i < inst.n_plants; ++i) fixed += chrom[i] * inst.plant_fixed[i];
        for (int j = 0; j < inst.n_depots; ++j) fixed += chrom[inst.n_plants + j] * inst.depot_fixed[j];
        return fixed + static_cast<double>(cost) / (cfg.flow_scale * cfg.cost_scale);
    }

    vector<int> crossover(const vector<int>& a, const vector<int>& b) {
        vector<int> child(a.size());
        uniform_int_distribution<int> bit(0, 1);
        for (int i = 0; i < static_cast<int>(a.size()); ++i) child[i] = bit(rng) ? a[i] : b[i];
        return child;
    }

    vector<int> mutate(vector<int> chrom) {
        uniform_real_distribution<double> prob(0.0, 1.0);
        for (int& bit : chrom) {
            if (prob(rng) < cfg.mutation_rate) bit = 1 - bit;
        }
        return chrom;
    }

    vector<int> ls1(vector<int> current, bool single_pass) {
        double current_cost = evaluate(current);
        bool improved = true;
        while (improved) {
            improved = false;
            for (int idx = 0; idx < static_cast<int>(current.size()); ++idx) {
                auto cand = current;
                cand[idx] = 1 - cand[idx];
                if (!feasible(cand)) continue;
                double cost = evaluate(cand);
                if (cost + 1e-9 < current_cost) {
                    current = std::move(cand);
                    current_cost = cost;
                    improved = true;
                    break;
                }
            }
            if (single_pass) break;
        }
        return current;
    }

    vector<int> ls2(vector<int> current, bool single_pass) {
        double current_cost = evaluate(current);
        bool improved = true;
        while (improved) {
            improved = false;
            vector<pair<int, int>> ranges = {{0, inst.n_plants}, {inst.n_plants, inst.n_plants + inst.n_depots}};
            for (auto [start, end] : ranges) {
                vector<int> opened, closed;
                for (int idx = start; idx < end; ++idx) (current[idx] ? opened : closed).push_back(idx);
                for (int i : opened) {
                    for (int j : closed) {
                        auto cand = current;
                        cand[i] = 0;
                        cand[j] = 1;
                        if (!feasible(cand)) continue;
                        double cost = evaluate(cand);
                        if (cost + 1e-9 < current_cost) {
                            current = std::move(cand);
                            current_cost = cost;
                            improved = true;
                            break;
                        }
                    }
                    if (improved) break;
                }
                if (improved) break;
            }
            if (single_pass) break;
        }
        return current;
    }

    vector<int> intensify(const vector<int>& chrom, bool single_pass) {
        return ls2(ls1(chrom, single_pass), single_pass);
    }
};

static Config parse_config(
    int argc,
    char** argv,
    string& instance_dir,
    string& out_dir,
    string& eval_chromosome,
    string& benchmark_checkpoint,
    int& benchmark_limit
) {
    Config cfg;
    auto read_value = [&](int& i, const string& option) -> string {
        if (i + 1 >= argc) throw runtime_error("Missing value for " + option);
        return argv[++i];
    };
    auto read_path_value = [&](int& i, const string& option) -> string {
        if (i + 1 >= argc) throw runtime_error("Missing value for " + option);
        string value = argv[++i];
        while (i + 1 < argc) {
            string next = argv[i + 1];
            if (next.rfind("--", 0) == 0) break;
            value += " " + next;
            ++i;
        }
        return value;
    };
    for (int i = 1; i < argc; ++i) {
        string a = argv[i];
        if (a == "--instance-dir") instance_dir = read_path_value(i, a);
        else if (a == "--out-dir") out_dir = read_path_value(i, a);
        else if (a == "--eval-chromosome") eval_chromosome = read_path_value(i, a);
        else if (a == "--benchmark-checkpoint") benchmark_checkpoint = read_path_value(i, a);
        else if (a == "--benchmark-limit") benchmark_limit = stoi(read_value(i, a));
        else if (a == "--resume") cfg.resume = true;
        else if (a == "--ls-single-pass") cfg.ls_single_pass = true;
        else if (a == "--elitism-final-only") cfg.elitism_final_only = true;
        else if (a == "--elitism-top-k") cfg.elitism_top_k = stoi(read_value(i, a));
        else if (a == "--population") cfg.population = stoi(read_value(i, a));
        else if (a == "--generations") cfg.generations = stoi(read_value(i, a));
        else if (a == "--elitism-interval") cfg.elitism_interval = stoi(read_value(i, a));
        else if (a == "--checkpoint-interval") cfg.checkpoint_interval = stoi(read_value(i, a));
        else if (a == "--mutation-rate") cfg.mutation_rate = stod(read_value(i, a));
        else if (a == "--seed") cfg.seed = stoi(read_value(i, a));
    }
    if (instance_dir.empty()) throw runtime_error("--instance-dir is required");
    if (out_dir.empty() && eval_chromosome.empty() && benchmark_checkpoint.empty()) throw runtime_error("--out-dir is required");
    return cfg;
}

int main(int argc, char** argv) {
    try {
        string instance_dir, out_dir, eval_chromosome, benchmark_checkpoint;
        int benchmark_limit = 100;
        Config cfg = parse_config(argc, argv, instance_dir, out_dir, eval_chromosome, benchmark_checkpoint, benchmark_limit);
        Instance inst = load_instance(instance_dir);
        Solver solver(std::move(inst), cfg);
        if (!eval_chromosome.empty()) {
            vector<int> chrom = read_chromosome_csv(eval_chromosome);
            cout << fixed << setprecision(6) << solver.evaluate(chrom) << "\n";
            return 0;
        }
        if (!benchmark_checkpoint.empty()) {
            auto population = read_checkpoint_population(benchmark_checkpoint);
            int limit = min(benchmark_limit, static_cast<int>(population.size()));
            vector<double> times;
            vector<double> objectives;
            times.reserve(limit);
            objectives.reserve(limit);
            for (int i = 0; i < limit; ++i) {
                auto start = chrono::steady_clock::now();
                double objective = solver.evaluate(population[i]);
                double elapsed = chrono::duration<double>(chrono::steady_clock::now() - start).count();
                times.push_back(elapsed);
                objectives.push_back(objective);
            }
            double sum = accumulate(times.begin(), times.end(), 0.0);
            double min_time = *min_element(times.begin(), times.end());
            double max_time = *max_element(times.begin(), times.end());
            cout << fixed << setprecision(6);
            cout << "count=" << limit << "\n";
            cout << "avg_eval_s=" << (sum / limit) << "\n";
            cout << "min_eval_s=" << min_time << "\n";
            cout << "max_eval_s=" << max_time << "\n";
            cout << "first_objective=" << objectives.front() << "\n";
            cout << "last_objective=" << objectives.back() << "\n";
            return 0;
        }
        solver.solve(out_dir);
    } catch (const exception& exc) {
        cerr << "error: " << exc.what() << "\n";
        return 1;
    }
    return 0;
}
