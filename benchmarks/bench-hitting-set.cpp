/**
 * @file bench-hitting-set.cpp
 * @author Robin Coutelier
 *
 * @brief Performance benchmarks for napsat::compute_hitting_sets, mirroring
 * the shape of the calls made by graph backtracking (see
 * NapSAT::compute_backtrack_possibilities in NapSAT-conflict.cpp): a handful
 * to a few hundred small conflict sets over a much larger element (chunk)
 * universe, with approximate = true and limit = 1000, matching the solver's
 * defaults.
 *
 * Each scenario below runs both compute_hitting_sets (union-of-unsatisfied
 * branching + index-ordering dedup) and compute_hitting_sets_smallest_first
 * (smallest-unsatisfied-set branching, no ordering, dedup only via
 * completed-set subsumption -- see hitting-set.hpp) back to back on the same
 * instance, so the two numbers are directly comparable.
 *
 * Run via `make benchmarks`. Unlike tests/, this binary is built at -O3 with
 * assertions disabled (NDEBUG), since debug-build timings aren't
 * representative of what the solver actually experiences.
 */
#include "../src/utils/hitting-set.hpp"
#include "../src/utils/bitset.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark_all.hpp>

#include <algorithm>
#include <random>
#include <string>
#include <vector>

using namespace std;
using namespace napsat;

namespace
{
  // Builds `n_sets` random subsets of a `universe`-element universe, each of
  // size in [min_set_size, max_set_size]. Seeded for reproducibility, so
  // before/after comparisons of the algorithm aren't muddied by instance
  // variance.
  vector<bitset> random_instance(unsigned universe, unsigned n_sets,
                                  unsigned min_set_size, unsigned max_set_size,
                                  unsigned seed)
  {
    mt19937 rng(seed);
    uniform_int_distribution<unsigned> size_dist(min_set_size, max_set_size);
    uniform_int_distribution<unsigned> elem_dist(0, universe - 1);

    vector<bitset> sets;
    sets.reserve(n_sets);
    for (unsigned i = 0; i < n_sets; i++) {
      bitset s(universe);
      unsigned target = min(size_dist(rng), universe);
      while (s.count() < target)
        s.set(elem_dist(rng), true);
      sets.push_back(std::move(s));
    }
    return sets;
  }

  // `n_sets` sets that all share element 0 and are otherwise disjoint: the
  // adversarial-ordering shape from the "bad ordering" correctness tests in
  // tests/test-hitting-set.cpp, scaled up. The cheap size-1 answer ({0}) is
  // found almost immediately, but a much larger alternative hitting set
  // remains reachable and must still be explored/pruned.
  vector<bitset> shared_element_instance(unsigned n_sets)
  {
    unsigned universe = n_sets + 1;
    vector<bitset> sets;
    sets.reserve(n_sets);
    for (unsigned i = 0; i < n_sets; i++) {
      bitset s(universe);
      s.set(0, true);
      s.set(i + 1, true);
      sets.push_back(std::move(s));
    }
    return sets;
  }
}

TEST_CASE("hitting-set: scaling with number of conflicts", "[hitting-set]") {
  for (unsigned n_sets : {5u, 10u, 25u, 50u, 100u, 200u}) {
    vector<bitset> to_hit = random_instance(/*universe=*/500, n_sets, /*min=*/2, /*max=*/6, /*seed=*/n_sets);
    BENCHMARK("n_sets=" + to_string(n_sets) + " union+order") {
      vector<bitset> result;
      compute_hitting_sets(to_hit, result, /*limit=*/1000, /*approximate=*/true);
      return result.size();
    };
    BENCHMARK("n_sets=" + to_string(n_sets) + " smallest-first") {
      vector<bitset> result;
      compute_hitting_sets_smallest_first(to_hit, result, /*limit=*/1000, /*approximate=*/true);
      return result.size();
    };
  }
}

TEST_CASE("hitting-set: scaling with universe size", "[hitting-set]") {
  for (unsigned universe : {100u, 500u, 2000u, 8000u}) {
    vector<bitset> to_hit = random_instance(universe, /*n_sets=*/50, /*min=*/2, /*max=*/6, /*seed=*/universe);
    BENCHMARK("universe=" + to_string(universe) + " union+order") {
      vector<bitset> result;
      compute_hitting_sets(to_hit, result, /*limit=*/1000, /*approximate=*/true);
      return result.size();
    };
    BENCHMARK("universe=" + to_string(universe) + " smallest-first") {
      vector<bitset> result;
      compute_hitting_sets_smallest_first(to_hit, result, /*limit=*/1000, /*approximate=*/true);
      return result.size();
    };
  }
}

TEST_CASE("hitting-set: adversarial shared-element ordering", "[hitting-set]") {
  for (unsigned n_sets : {10u, 50u, 100u, 200u}) {
    vector<bitset> to_hit = shared_element_instance(n_sets);
    BENCHMARK("n_sets=" + to_string(n_sets) + " union+order") {
      vector<bitset> result;
      compute_hitting_sets(to_hit, result, /*limit=*/1000, /*approximate=*/true);
      return result.size();
    };
    BENCHMARK("n_sets=" + to_string(n_sets) + " smallest-first") {
      vector<bitset> result;
      compute_hitting_sets_smallest_first(to_hit, result, /*limit=*/1000, /*approximate=*/true);
      return result.size();
    };
  }
}
