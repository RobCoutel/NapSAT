/**
 * @file reachability-closure.hpp
 * @author Robin Coutelier
 */

#pragma once

#include "bitset.hpp"

#include <vector>

namespace napsat
{
  /**
   * @brief Given a DAG over element indices [0, parents.size()), where
   * parents[u] holds u's parents (its outgoing edges), computes every
   * inclusion-minimal completion of `start` in which every element that is
   * not a leaf has a full path to a leaf contained in the completion.
   *
   * @details An element u is a *leaf* iff parents[u] is empty. A completion
   * is grown from `start`: for every non-leaf u in the set that does not
   * already contain one of its parents, we need to add exactly one of u's
   * non-forbidden parents (only one, since a single path to a leaf is
   * enough) and recurse from there. Each viable parent is tried as its own
   * branch, so a u with several usable parents yields several completions.
   * If every parent still missing from u is forbidden, u can never reach a
   * leaf, and the whole completion under construction is discarded (not
   * appended at all). Completions that are a superset of an already-found
   * completion are dropped, so the result only holds inclusion-minimal
   * completions.
   *
   * @param parents parents[u] = u's parents in the DAG.
   * @param forbidden elements that can never be added to a completion (e.g.
   * elements pinned elsewhere that cannot be moved).
   * @param start the initial element set every completion extends.
   * @param completions appended with every completion found (not cleared).
   * @param limit stop the search once this many completions have been
   * appended (0 = unbounded).
   */
  void compute_reachability_closures(const std::vector<bitset>& parents,
                                     const bitset& forbidden,
                                     const bitset& start,
                                     std::vector<bitset>& completions,
                                     unsigned limit = 0);
}
