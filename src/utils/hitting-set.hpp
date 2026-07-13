/**
 * @file hitting-set.hpp
 * @author Robin Coutelier
 */

#pragma once

#include "bitset.hpp"

#include <vector>

namespace napsat
{
  void compute_hitting_sets(const std::vector<bitset>& to_hit,
                           std::vector<bitset>& hitting_set,
                           unsigned limit = 0, bool approximate = false);

  /**
   * @brief Experimental alternative to compute_hitting_sets, for benchmarking
   * (see benchmarks/bench-hitting-set.cpp) which branching strategy performs
   * better. Branches only on the smallest not-yet-hit set at each node
   * (Reiter-style HS-tree) instead of the union of all not-yet-hit sets, and
   * drops the strictly-decreasing element-index ordering compute_hitting_sets
   * relies on to avoid duplicate generation (the two are incompatible: see
   * the comment at the branching step in compute_hitting_sets). Duplicate
   * generation of the same *complete* hitting set is still caught by the
   * existing found-set subsumption check, so results are correct, but
   * reconvergent *partial* prefixes (same elements so far, different order)
   * are not deduplicated and may be explored more than once. Not used by the
   * solver.
   */
  void compute_hitting_sets_smallest_first(const std::vector<bitset>& to_hit,
                           std::vector<bitset>& hitting_set,
                           unsigned limit = 0, bool approximate = false);
}
