/**
 * @file reachability-closure.cpp
 * @author Robin Coutelier
 *
 * @brief See reachability-closure.hpp.
 */
#include "reachability-closure.hpp"

using namespace std;

namespace napsat
{

namespace
{
  bool subsumed(const vector<bitset>& completions, const bitset& b)
  {
    for (const bitset& c : completions) {
      if (c <= b) {
        return true;
      }
    }
    return false;
  }

  void extend_closure(const vector<bitset>& parents,
                      const bitset& forbidden,
                      bitset current,
                      bitset processed,
                      vector<bitset>& completions,
                      unsigned limit)
  {
    // check for subsumption
    if (subsumed(completions, current)) {
      return;
    }

    // elements of current that still have to be checked for a path to a leaf
    bitset frontier = current - processed;

    if (frontier.empty()) {
      completions.push_back(current);
      return;
    }

    for (auto it = frontier.cbegin(); it != frontier.cend(); ++it) {
      unsigned u = *it;
      bitset next_processed = processed;
      next_processed.set(u, true);

      // parents of u still missing from current
      bitset needed = parents[u] - current;
      if (needed.empty()) {
        // u is a leaf, or one of its parents is already in current: either
        // way, its path to a leaf is already contained in current
        extend_closure(parents, forbidden, current, next_processed, completions, limit);
        if (limit > 0 && completions.size() > limit) {
          return;
        }
        continue;
      }

      bitset candidates = needed - forbidden;
      if (candidates.empty()) {
        // every parent still missing is forbidden: u can never reach a
        // leaf, so this whole completion must be discarded, not treated
        // as satisfied
        return;
      }

      for (auto jt = candidates.cbegin(); jt != candidates.cend(); ++jt) {
        unsigned p = *jt;
        bitset next_current = current;
        next_current.set(p, true);
        extend_closure(parents, forbidden, next_current, next_processed, completions, limit);
        if (limit > 0 && completions.size() > limit) {
          return;
        }
      }
    }
  }
} // namespace

void compute_reachability_closures(const vector<bitset>& parents,
                                   const bitset& forbidden,
                                   const bitset& start,
                                   vector<bitset>& completions,
                                   unsigned limit)
{
  extend_closure(parents, forbidden, start, bitset(start.capacity()), completions, limit);
}

} // namespace napsat
