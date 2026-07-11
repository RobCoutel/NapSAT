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
      double score_a = a.size + 1.1 * a.unsatisfied.size();
      double score_b = b.size + 1.1 * b.unsatisfied.size();
      return score_a > score_b; // min-heap
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

  bitset all_elements(capacity);
  for (const bitset& s : to_hit) {
    all_elements |= s;
  }

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

  /**
   * @brief When we already found a set S such that S - prefix = {c}, we can skip expanding the prefix with c, because it will be subsumed by S.
   */
  bitset unit_sets(capacity);
  // The set of element that can still be used to expand the current prefix.
  bitset remaining(capacity);
  // add a filter that conly accepts element on the left of the min_index_in_prefix, so that we do not generate duplicates.
  bitset filter(capacity);
  while (!queue.empty()) {
    if (hitting_set.size() >= limit && limit > 0)
      break;
    unit_sets.clear();
    filter.clear();
    remaining.clear();

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
    unsigned min_index_in_prefix = capacity;
    if (!node.prefix.empty())
      min_index_in_prefix = *node.prefix.cbegin();
    assert(min_index_in_prefix <= capacity);
    for (auto it = all_elements.cbegin(); it != all_elements.cend(); ++it) {
      unsigned c = *it;
      if (c >= min_index_in_prefix) {
        filter.set(c, true);
      }
    }
    // now the filter also excludes the elements that would make this prefix subsumed by an existing hitting set
    filter |= unit_sets;

    bool impossible_to_cover = false;
    // Calculate which elements can still be required
    for (unsigned idx : node.unsatisfied) {
      bitset remaining_in_set = to_hit[idx] - filter;

      if (remaining_in_set.empty()) {
        // This set cannot be hit by any prefix that expands to the left of the current prefix, so it is impossible to cover it.
        impossible_to_cover = true;
        break;
      }
      remaining |= remaining_in_set;
    }
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
      child.unsatisfied.reserve(node.unsatisfied.size());
      for (unsigned idx : node.unsatisfied)
        if (!to_hit[idx].get(c))
          child.unsatisfied.push_back(idx);

      queue.push(std::move(child));
    }
  }

}

}
