/**
 * @file hitting-set.cpp
 * @author Robin Coutelier
 *
 * @brief Computes minimal hitting sets: given a family of sets `to_hit`, finds
 * all inclusion-minimal sets that intersect every set in the family.
 *
 * @details The search is a best-first branch-and-bound over prefixes. At each
 * node, the prefix is extended with one element drawn from the union of the
 * not-yet-hit sets' own elements; every branch therefore hits at least one
 * more set, so the tree depth is bounded by `to_hit.size()`. Duplicate
 * generation of the same hitting set via different branch orders is avoided
 * by only ever adding elements in strictly decreasing index order (see
 * `min_index_in_prefix` below) — which is also why branching must consider
 * every not-yet-hit set rather than just one of them (see the comment at the
 * branching step for why that matters for correctness).
 *
 * Two data structures keep per-node work cheap even when `to_hit` holds many
 * sets over a large element universe:
 *  - `elem_sets` (built once): for each element, the indices of the `to_hit`
 *    sets containing it, as a dense bitmask over set indices. This lets a
 *    node's `unsatisfied` mask be updated by a word-level `&= ~elem_sets[c]`
 *    instead of testing membership of `c` in every unsatisfied set.
 *  - a cheap greedy hitting set computed upfront, used only to seed the
 *    `approximate` pruning bound so it is effective from the first node
 *    instead of only after the search happens to reach its first solution.
 */
#include "hitting-set.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <queue>
#include <utility>

#if __cplusplus >= 202002L
#include <bit>
#endif

using namespace std;

namespace napsat
{

namespace
{
  constexpr unsigned NO_ELEMENT = 0xFFFFFFFF;

  inline constexpr unsigned popcount64(uint64_t x) {
#if __cplusplus >= 202002L
    return std::popcount(x);
#else
    return __builtin_popcountll(x);
#endif
  }

  inline constexpr unsigned ctz64(uint64_t x) {
#if __cplusplus >= 202002L
    return std::countr_zero(x);
#else
    return __builtin_ctzll(x);
#endif
  }

  /**
   * @brief A dense bitmask over the indices of `to_hit`.
   * @details Unlike the sparse `bitset` used for elements (whose universe
   * can be large while individual sets stay small), the number of sets to
   * hit is small enough that a flat, fixed-width word array beats the
   * sparse representation here: O(1) set/test, O(words) bulk ops, no
   * metadata traversal. This is what makes per-node bookkeeping scale with
   * `to_hit.size() / 64` instead of `to_hit.size()`.
   */
  class set_mask {
  public:
    set_mask() = default;
    explicit set_mask(size_t n_sets) : _words((n_sets + 63) / 64, 0) {}

    void set(size_t i) { _words[i >> 6] |= (uint64_t{1} << (i & 63)); }

    bool empty() const {
      for (uint64_t w : _words) if (w) return false;
      return true;
    }

    unsigned count() const {
      unsigned c = 0;
      for (uint64_t w : _words) c += popcount64(w);
      return c;
    }

    /** this &= ~other, i.e. drop every set index also present in `other`. */
    void remove_all(const set_mask& other) {
      for (size_t i = 0; i < _words.size(); i++)
        _words[i] &= ~other._words[i];
    }

    /** popcount(this & other), without materializing the intersection. */
    unsigned count_intersection(const set_mask& other) const {
      unsigned c = 0;
      for (size_t i = 0; i < _words.size(); i++)
        c += popcount64(_words[i] & other._words[i]);
      return c;
    }

    /** Calls f(idx) for every set index present in the mask. */
    template <typename F>
    void for_each(F&& f) const {
      for (size_t w = 0; w < _words.size(); w++) {
        uint64_t bits = _words[w];
        while (bits) {
          unsigned b = ctz64(bits);
          f(static_cast<unsigned>(w * 64 + b));
          bits &= bits - 1;
        }
      }
    }

  private:
    vector<uint64_t> _words;
  };

  /**
   * @brief A node of the best-first search over hitting-set prefixes.
   * @details `unsatisfied` holds the indices (into `to_hit`) of the sets not
   * yet hit by `prefix`, as a `set_mask`. Those sets are, by construction,
   * disjoint from `prefix`. This also means `c` can never already be set in
   * `prefix`, so the new size is always `size + 1`, with no need to recount
   * bits.
   */
  struct hitting_set_node {
    bitset prefix;
    unsigned size; // popcount(prefix), maintained incrementally
    set_mask unsatisfied;
  };

  struct compare_hitting_set_nodes {
    bool operator()(const hitting_set_node& a, const hitting_set_node& b) const {
      double score_a = a.size + 1.1 * a.unsatisfied.count();
      double score_b = b.size + 1.1 * b.unsatisfied.count();
      return score_a > score_b; // min-heap
    }
  };
}

void compute_hitting_sets(const vector<bitset>& to_hit,
                          vector<bitset>& hitting_sets,
                          unsigned limit, bool approximate)
{
  assert(!to_hit.empty());
  assert(all_of(to_hit.begin(), to_hit.end(),
      [](const bitset& s){ return !s.empty(); }
    ));
  assert(hitting_sets.empty());

  // if there is only one set, the minimal hitting sets are the singletons of its elements
  if (to_hit.size() == 1) {
    for (auto it = to_hit[0].cbegin(); it != to_hit[0].cend(); ++it) {
      bitset singleton(to_hit[0].capacity());
      singleton.set(*it, true);
      hitting_sets.push_back(std::move(singleton));
    }
    return;
  }

  unsigned capacity = to_hit[0].capacity();

  bitset all_elements(capacity);
  for (const bitset& s : to_hit) {
    all_elements |= s;
  }

  // Inverted index: elem_sets[c] holds the indices of the `to_hit` sets that
  // contain element c. Built once, in time proportional to the total size
  // of `to_hit`, and reused by every node of the search below.
  vector<set_mask> elem_sets(capacity, set_mask(to_hit.size()));
  for (unsigned i = 0; i < to_hit.size(); i++)
    for (auto it = to_hit[i].cbegin(); it != to_hit[i].cend(); ++it)
      elem_sets[*it].set(i);

  set_mask all_unsatisfied(to_hit.size());
  for (unsigned i = 0; i < to_hit.size(); i++)
    all_unsatisfied.set(i);

  unsigned shortest_set = 0xFFFFFFFF;
  // Seed `shortest_set` with a fast greedy hitting set (repeatedly pick the
  // element that hits the most still-unsatisfied sets) so the `approximate`
  // cutoff below can start pruning from the very first node instead of only
  // after the search happens to stumble onto its first solution.
  if (approximate) {
    set_mask greedy_unsatisfied = all_unsatisfied;
    unsigned greedy_size = 0;
    while (!greedy_unsatisfied.empty()) {
      unsigned best = NO_ELEMENT;
      unsigned best_hits = 0;
      for (auto it = all_elements.cbegin(); it != all_elements.cend(); ++it) {
        unsigned c = *it;
        unsigned hits = elem_sets[c].count_intersection(greedy_unsatisfied);
        if (hits > best_hits) {
          best_hits = hits;
          best = c;
        }
      }
      assert(best != NO_ELEMENT);
      greedy_unsatisfied.remove_all(elem_sets[best]);
      greedy_size++;
    }
    shortest_set = greedy_size;
  }

  priority_queue<hitting_set_node, vector<hitting_set_node>, compare_hitting_set_nodes> queue;
  // Sizes of accepted hitting sets, kept parallel to `hitting_sets`. A node is only
  // accepted once its priority (size + unsatisfied.size()) equals its size, and the
  // priority queue pops nodes in non-decreasing priority order, so this is always
  // sorted: hitting sets are discovered in non-decreasing size order.
  vector<unsigned> hitting_set_sizes;

  hitting_set_node root;
  root.prefix = bitset(capacity);
  root.size = 0;
  root.unsatisfied = all_unsatisfied;
  queue.push(std::move(root));

  /**
   * @brief When we already found a set S such that S - prefix = {c}, we can skip expanding the prefix with c, because it will be subsumed by S.
   */
  bitset unit_sets(capacity);
  // The set of elements that can still be used to expand the current prefix
  // (see the branching step below).
  bitset remaining(capacity);
  while (!queue.empty()) {
    if (hitting_sets.size() >= limit && limit > 0)
      break;
    unit_sets.clear();
    remaining.clear();

    hitting_set_node node = queue.top();
    queue.pop();

    if (approximate && node.size > 2 * shortest_set) {
      // we already found a hitting set of size shortest_set, and this node is larger, so it cannot lead to a smaller one.
      continue;
    }

    // by virtue of the priority queue, we know that future prefixes cannot subsume older ones.
    // But it can still be the case that an older prefix subsumes a newer one.
    // `hitting_sets`/`hitting_set_sizes` are sorted by size (see above), so we can stop
    // as soon as a hitting set is too large to be a subset of `node.prefix`.
    bool subsumed = false;
    for (size_t i = 0; i < hitting_sets.size(); i++) {
      if (hitting_set_sizes[i] > node.size)
        break;
      bitset diff = hitting_sets[i] - node.prefix;
      if (diff.count() == 1) {
        unsigned c = *diff.cbegin();
        unit_sets.set(c, true);
      }
      if (diff.empty()) {
        subsumed = true;
        break;
      }
    }
    if (subsumed)
      continue;

    if (node.unsatisfied.empty()) {
      // all sets are hit, we can stop
      hitting_set_sizes.push_back(node.size);
      hitting_sets.push_back(std::move(node.prefix));
      if (approximate && node.size < shortest_set) {
        shortest_set = node.size;
      }
      continue;
    }
    unsigned min_index_in_prefix = capacity;
    if (!node.prefix.empty())
      min_index_in_prefix = *node.prefix.cbegin();
    assert(min_index_in_prefix <= capacity);

    // Candidate elements to expand the prefix with: the union, over every
    // not-yet-hit set, of its elements that are usable — below
    // min_index_in_prefix (so each hitting set is generated via exactly one
    // strictly-decreasing-index order, which is what avoids generating the
    // same set twice) and not in unit_sets (would make the child immediately
    // subsumed by an existing hitting set).
    //
    // NB: branching must consider *every* not-yet-hit set here, not just the
    // smallest one — e.g. for {0,1} and {10,11}, restricting to the smaller
    // (tied) set {0,1} would only ever offer 0 or 1 as the *first* element
    // added, and since later elements must be strictly smaller, 10 and 11
    // could then never be added at all, silently losing the hitting sets
    // {0,10}, {0,11}, {1,10}, {1,11}. Walking each to_hit[idx] directly
    // (small, one per not-yet-hit set) instead of scanning the whole element
    // universe is what keeps this cheap despite covering every set.
    bool impossible_to_cover = false;
    node.unsatisfied.for_each([&](unsigned idx) {
      if (impossible_to_cover)
        return;
      bool any = false;
      for (auto it = to_hit[idx].cbegin(); it != to_hit[idx].cend(); ++it) {
        unsigned c = *it;
        if (c < min_index_in_prefix && !unit_sets.get(c)) {
          remaining.set(c, true);
          any = true;
        }
      }
      if (!any) {
        // This set cannot be hit by any prefix that expands to the left of the current prefix, so it is impossible to cover it.
        impossible_to_cover = true;
      }
    });
    if (impossible_to_cover) {
      continue;
    }

    for (auto it = remaining.cbegin(); it != remaining.cend(); ++it) {
      unsigned c = *it;
      assert(c < min_index_in_prefix);

      hitting_set_node child;
      child.prefix = node.prefix;
      child.prefix.set(c, true);
      child.size = node.size + 1;
      // Sets containing c move from unsatisfied to hit in O(words) via the
      // precomputed inverted index, instead of testing membership of c in
      // every unsatisfied set individually.
      child.unsatisfied = node.unsatisfied;
      child.unsatisfied.remove_all(elem_sets[c]);

      queue.push(std::move(child));
    }
  }

}

void compute_hitting_sets_smallest_first(const vector<bitset>& to_hit,
                          vector<bitset>& hitting_sets,
                          unsigned limit, bool approximate)
{
  assert(!to_hit.empty());
  assert(all_of(to_hit.begin(), to_hit.end(),
      [](const bitset& s){ return !s.empty(); }
    ));
  assert(hitting_sets.empty());

  if (to_hit.size() == 1) {
    for (auto it = to_hit[0].cbegin(); it != to_hit[0].cend(); ++it) {
      bitset singleton(to_hit[0].capacity());
      singleton.set(*it, true);
      hitting_sets.push_back(std::move(singleton));
    }
    return;
  }

  unsigned capacity = to_hit[0].capacity();

  bitset all_elements(capacity);
  for (const bitset& s : to_hit) {
    all_elements |= s;
  }

  vector<set_mask> elem_sets(capacity, set_mask(to_hit.size()));
  for (unsigned i = 0; i < to_hit.size(); i++)
    for (auto it = to_hit[i].cbegin(); it != to_hit[i].cend(); ++it)
      elem_sets[*it].set(i);

  set_mask all_unsatisfied(to_hit.size());
  for (unsigned i = 0; i < to_hit.size(); i++)
    all_unsatisfied.set(i);

  unsigned shortest_set = 0xFFFFFFFF;
  if (approximate) {
    set_mask greedy_unsatisfied = all_unsatisfied;
    unsigned greedy_size = 0;
    while (!greedy_unsatisfied.empty()) {
      unsigned best = NO_ELEMENT;
      unsigned best_hits = 0;
      for (auto it = all_elements.cbegin(); it != all_elements.cend(); ++it) {
        unsigned c = *it;
        unsigned hits = elem_sets[c].count_intersection(greedy_unsatisfied);
        if (hits > best_hits) {
          best_hits = hits;
          best = c;
        }
      }
      assert(best != NO_ELEMENT);
      greedy_unsatisfied.remove_all(elem_sets[best]);
      greedy_size++;
    }
    shortest_set = greedy_size;
  }

  priority_queue<hitting_set_node, vector<hitting_set_node>, compare_hitting_set_nodes> queue;
  // Unlike compute_hitting_sets, sizes here are not guaranteed sorted
  // relative to *every* hitting set that could still be found (reconvergent
  // duplicates can complete slightly out of size order relative to each
  // other in rare cases), but they are still non-decreasing overall since
  // the queue is best-first on the same priority; kept for the same
  // early-break optimization as compute_hitting_sets.
  vector<unsigned> hitting_set_sizes;

  hitting_set_node root;
  root.prefix = bitset(capacity);
  root.size = 0;
  root.unsatisfied = all_unsatisfied;
  queue.push(std::move(root));

  bitset unit_sets(capacity);
  while (!queue.empty()) {
    if (hitting_sets.size() >= limit && limit > 0)
      break;
    unit_sets.clear();

    hitting_set_node node = queue.top();
    queue.pop();

    if (approximate && node.size > 2 * shortest_set) {
      continue;
    }

    bool subsumed = false;
    for (size_t i = 0; i < hitting_sets.size(); i++) {
      if (hitting_set_sizes[i] > node.size)
        break;
      bitset diff = hitting_sets[i] - node.prefix;
      if (diff.count() == 1) {
        unsigned c = *diff.cbegin();
        unit_sets.set(c, true);
      }
      if (diff.empty()) {
        subsumed = true;
        break;
      }
    }
    if (subsumed)
      continue;

    if (node.unsatisfied.empty()) {
      hitting_set_sizes.push_back(node.size);
      hitting_sets.push_back(std::move(node.prefix));
      if (approximate && node.size < shortest_set) {
        shortest_set = node.size;
      }
      continue;
    }

    // Pick the not-yet-hit set with the fewest usable elements (not in
    // unit_sets -- prefix membership is already excluded, since unsatisfied
    // sets are disjoint from prefix by construction) and branch only on its
    // elements. No index-ordering restriction here, unlike
    // compute_hitting_sets: see the class comment for what that costs.
    unsigned best_idx = NO_ELEMENT;
    unsigned best_count = 0xFFFFFFFF;
    bool impossible_to_cover = false;
    node.unsatisfied.for_each([&](unsigned idx) {
      if (impossible_to_cover)
        return;
      unsigned count = 0;
      for (auto it = to_hit[idx].cbegin(); it != to_hit[idx].cend(); ++it)
        if (!unit_sets.get(*it))
          count++;
      if (count == 0) {
        impossible_to_cover = true;
        return;
      }
      if (count < best_count) {
        best_count = count;
        best_idx = idx;
      }
    });
    if (impossible_to_cover) {
      continue;
    }

    for (auto it = to_hit[best_idx].cbegin(); it != to_hit[best_idx].cend(); ++it) {
      unsigned c = *it;
      if (unit_sets.get(c))
        continue;

      hitting_set_node child;
      child.prefix = node.prefix;
      child.prefix.set(c, true);
      child.size = node.size + 1;
      child.unsatisfied = node.unsatisfied;
      child.unsatisfied.remove_all(elem_sets[c]);

      queue.push(std::move(child));
    }
  }
}

}
