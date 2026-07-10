/**
 * @file hitting-set.cpp
 * @author Robin Coutelier
 *
 * @brief Computes minimal hitting sets: given a family of sets `to_hit`, finds
 * all inclusion-minimal sets that intersect every set in the family.
 */
#include "hitting-set.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <queue>
#include <utility>

using namespace std;

namespace napsat
{

namespace
{
  constexpr unsigned NO_ELEMENT = 0xFFFFFFFF;

  /**
   * @brief A node of the best-first search over hitting-set prefixes.
   * @details `unsatisfied` holds the indices (into `to_hit`) of the sets not
   * yet hit by `prefix`. Those sets are, by construction, disjoint from
   * `prefix`, so expanding by an element `c` only requires testing each of
   * them for membership of `c`, rather than re-intersecting the whole next
   * prefix against every set in `to_hit`. This also means `c` can never
   * already be set in `prefix`, so the new size is always `size + 1`, with no
   * need to recount bits.
   */
  struct hitting_set_node {
    bitset prefix;
    unsigned size; // popcount(prefix), maintained incrementally
    vector<unsigned> unsatisfied;
  };

  struct compare_hitting_set_nodes {
    bool operator()(const hitting_set_node& a, const hitting_set_node& b) const {
      return (a.size + a.unsatisfied.size()) > (b.size + b.unsatisfied.size());
    }
  };
}

void compute_hitting_sets(const vector<bitset>& to_hit,
                          vector<bitset>& hitting_set,
                          unsigned limit)
{
  assert(!to_hit.empty());
  assert(all_of(to_hit.begin(), to_hit.end(),
      [](const bitset& s){ return !s.empty(); }
    ));
  assert(hitting_set.empty());

  unsigned capacity = to_hit[0].capacity();

  priority_queue<hitting_set_node, vector<hitting_set_node>, compare_hitting_set_nodes> queue;
  // Sizes of accepted hitting sets, kept parallel to `hitting_set`. A node is only
  // accepted once its priority (size + unsatisfied.size()) equals its size, and the
  // priority queue pops nodes in non-decreasing priority order, so this is always
  // sorted: hitting sets are discovered in non-decreasing size order.
  vector<unsigned> hitting_set_sizes;

  hitting_set_node root;
  root.prefix = bitset(capacity);
  root.size = 0;
  root.unsatisfied.resize(to_hit.size());
  for (unsigned i = 0; i < to_hit.size(); i++)
    root.unsatisfied[i] = i;
  queue.push(std::move(root));

  // Scratch space to collect, for the node currently being expanded, the distinct
  // elements appearing across its unsatisfied sets. `bitset::operator|=` is
  // deliberately not used for this: it compacts into a sparse vector and pays an
  // O(size-so-far) `vector::insert` for every previously-unseen word, which is
  // the dominant cost once the O(m) full rescan is avoided. A dense "seen"
  // buffer makes recording an element O(1); `touched` lets us reset only what
  // we touched instead of clearing the whole buffer every node.
  vector<bool> seen(capacity, false);
  vector<unsigned> touched;

  /**
   * @brief When we already found a set S such that S - prefix = {c}, we can skip expanding the prefix with c, because it will be subsumed by S.
   */
  bitset unit_sets(capacity);
  while (!queue.empty()) {
    if (hitting_set.size() >= limit && limit > 0)
      break;
    unit_sets.clear();

    hitting_set_node node = queue.top();
    queue.pop();

    // by virtue of the priority queue, we know that future prefixes cannot subsume older ones.
    // But it can still be the case that an older prefix subsumes a newer one.
    // `hitting_set`/`hitting_set_sizes` are sorted by size (see above), so we can stop
    // as soon as a hitting set is too large to be a subset of `node.prefix`.
    bool subsumed = false;
    for (size_t i = 0; i < hitting_set.size(); i++) {
      if (hitting_set_sizes[i] > node.size)
        break;
      bitset diff = hitting_set[i] - node.prefix;
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
      hitting_set.push_back(std::move(node.prefix));
      continue;
    }

    unsigned min_index_in_prefix = NO_ELEMENT;
    if (!node.prefix.empty())
      min_index_in_prefix = *node.prefix.cbegin();

    bool impossible_to_cover = false;
    // Calculate which elements can still be required
    assert(touched.empty());
    for (unsigned idx : node.unsatisfied) {
      auto it = to_hit[idx].cbegin();
      unsigned min_index_in_set = NO_ELEMENT;
      auto end = to_hit[idx].cend();
      for (; it != end; ++it) {
        unsigned c = *it;
        if (seen[c] || unit_sets[c])
          continue;
        seen[c] = true;
        touched.push_back(c);
        if (c < min_index_in_set) {
          min_index_in_set = c;
        }
      }
      if (min_index_in_set > min_index_in_prefix) {
        // This set cannot be hit by any prefix that expands to the left of the current prefix, so it is impossible to cover it.
        impossible_to_cover = true;
        break;
      }
    }
    if (impossible_to_cover) {
      for (unsigned c : touched)
        seen[c] = false;
      touched.clear();
      continue;
    }
    assert(!touched.empty());

    // expand the prefix
    unsigned min_element = node.prefix.empty() ? NO_ELEMENT : *node.prefix.cbegin();

    for (unsigned c : touched) {
      // reset the scratch buffer for the next node as we go, instead of a
      // second full pass over `touched` after the loop
      seen[c] = false;
      if (c > min_element) {
        // Only add prefixes that expand to the left
        // The ones on the left should be expanded elsewhere.
        // This prevents duplicates and makes the search more efficient, but is more complex to implement.
        continue;
      }

      hitting_set_node child;
      child.prefix = node.prefix;
      child.prefix.set(c, true);
      child.size = node.size + 1;
      child.unsatisfied.reserve(node.unsatisfied.size());
      for (unsigned idx : node.unsatisfied)
        if (!to_hit[idx].get(c))
          child.unsatisfied.push_back(idx);

      queue.push(std::move(child));
    }
    touched.clear();
  }
}

}
