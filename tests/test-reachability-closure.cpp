/*
 * This file is part of the source code of the software program
 * NapSAT. It is protected by applicable copyright laws.
 *
 * This source code is protected by the terms of the MIT License.
 */
/**
 * @file test-reachability-closure.cpp
 * @author Robin Coutelier
 *
 * @brief Unit tests for napsat::compute_reachability_closures: given a DAG
 * (parents[u] = u's parents) and a starting element set, it must return
 * exactly the completions of that set in which every non-leaf element has a
 * full path to a leaf contained in the completion.
 */
#include "../src/utils/reachability-closure.hpp"
#include "../src/utils/bitset.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

using namespace std;
using namespace napsat;

namespace
{
  bitset make_set(unsigned capacity, const vector<unsigned>& elems)
  {
    bitset b(capacity);
    for (unsigned e : elems)
      b.set(e, true);
    return b;
  }

  // A DAG over [0, capacity) with no edges yet (every node is a leaf).
  vector<bitset> make_dag(unsigned capacity)
  {
    return vector<bitset>(capacity, bitset(capacity));
  }

  // bitset iteration is ascending, so this is already sorted.
  vector<unsigned> elements(const bitset& b)
  {
    vector<unsigned> v;
    for (auto it = b.cbegin(); it != b.cend(); ++it)
      v.push_back(*it);
    return v;
  }

  vector<vector<unsigned>> element_families(const vector<bitset>& sets)
  {
    vector<vector<unsigned>> families;
    for (const bitset& s : sets)
      families.push_back(elements(s));
    sort(families.begin(), families.end());
    return families;
  }

  // Checks that `found` is exactly the family `expected` (as sets of element
  // lists, order-independent on both axes).
  bool same_family(const vector<bitset>& found, vector<vector<unsigned>> expected)
  {
    for (vector<unsigned>& e : expected)
      sort(e.begin(), e.end());
    sort(expected.begin(), expected.end());
    return element_families(found) == expected;
  }
}

TEST_CASE("reachability-closure: a start element that is already a leaf is returned unchanged") {
  // node 1 has no parent: it needs no extension at all.
  unsigned capacity = 64;
  vector<bitset> parents = make_dag(capacity);
  bitset forbidden(capacity);
  bitset start = make_set(capacity, {1});

  vector<bitset> result;
  compute_reachability_closures(parents, forbidden, start, result);
  REQUIRE(same_family(result, {{1}}));
}

TEST_CASE("reachability-closure: a single mandatory parent is added") {
  // 1 -> 2, and 2 is a leaf: the only completion is {1, 2}.
  unsigned capacity = 64;
  vector<bitset> parents = make_dag(capacity);
  parents[1].set(2, true);
  bitset forbidden(capacity);
  bitset start = make_set(capacity, {1});

  vector<bitset> result;
  compute_reachability_closures(parents, forbidden, start, result);
  REQUIRE(same_family(result, {{1, 2}}));
}

TEST_CASE("reachability-closure: alternative leaf parents branch into separate completions") {
  // 1 -> {2, 3}, both leaves: either one alone reaches a leaf, so each is
  // its own completion (an "or", not an "and": we don't need both).
  unsigned capacity = 64;
  vector<bitset> parents = make_dag(capacity);
  parents[1].set(2, true);
  parents[1].set(3, true);
  bitset forbidden(capacity);
  bitset start = make_set(capacity, {1});

  vector<bitset> result;
  compute_reachability_closures(parents, forbidden, start, result, /*limit=*/10);
  REQUIRE(same_family(result, {{1, 2}, {1, 3}}));
}

TEST_CASE("reachability-closure: worked example from the lazy-merge docs") {
  // 1 -> {2, 3}, 3 -> {4}, 2 and 4 are leaves. Picking 2 terminates
  // immediately; picking 3 forces 4 to be pulled in too.
  unsigned capacity = 64;
  vector<bitset> parents = make_dag(capacity);
  parents[1].set(2, true);
  parents[1].set(3, true);
  parents[3].set(4, true);
  bitset forbidden(capacity);
  bitset start = make_set(capacity, {1});

  vector<bitset> result;
  compute_reachability_closures(parents, forbidden, start, result, /*limit=*/10);
  REQUIRE(same_family(result, {{1, 2}, {1, 3, 4}}));
}

TEST_CASE("reachability-closure: a parent already present in start satisfies the node") {
  // 1 -> {2}, and 2 is already part of start: 1's path to a leaf is
  // already complete, nothing new needs to be added.
  unsigned capacity = 64;
  vector<bitset> parents = make_dag(capacity);
  parents[1].set(2, true);
  bitset forbidden(capacity);
  bitset start = make_set(capacity, {1, 2});

  vector<bitset> result;
  compute_reachability_closures(parents, forbidden, start, result);
  REQUIRE(same_family(result, {{1, 2}}));
}

TEST_CASE("reachability-closure: a forbidden-only path discards the whole completion") {
  // 1 -> {2}, but 2 is forbidden and not already in start: 1 can never
  // reach a leaf, so no completion is produced at all.
  unsigned capacity = 64;
  vector<bitset> parents = make_dag(capacity);
  parents[1].set(2, true);
  bitset forbidden = make_set(capacity, {2});
  bitset start = make_set(capacity, {1});

  vector<bitset> result;
  compute_reachability_closures(parents, forbidden, start, result);
  REQUIRE(result.empty());
}

TEST_CASE("reachability-closure: a forbidden alternative is skipped in favor of a viable one") {
  // 1 -> {2, 3}; 2 is forbidden, so only the branch through 3 survives.
  unsigned capacity = 64;
  vector<bitset> parents = make_dag(capacity);
  parents[1].set(2, true);
  parents[1].set(3, true);
  bitset forbidden = make_set(capacity, {2});
  bitset start = make_set(capacity, {1});

  vector<bitset> result;
  compute_reachability_closures(parents, forbidden, start, result);
  REQUIRE(same_family(result, {{1, 3}}));
}

TEST_CASE("reachability-closure: reconvergent duplicate is deduplicated") {
  // start = {1, 2}, both 1 -> 3 and 2 -> 3: the completion {1, 2, 3} is
  // reachable by resolving 1 first or 2 first, two distinct search paths
  // converging on the same set. It must still appear exactly once.
  unsigned capacity = 64;
  vector<bitset> parents = make_dag(capacity);
  parents[1].set(3, true);
  parents[2].set(3, true);
  bitset forbidden(capacity);
  bitset start = make_set(capacity, {1, 2});

  vector<bitset> result;
  compute_reachability_closures(parents, forbidden, start, result, /*limit=*/10);
  REQUIRE(same_family(result, {{1, 2, 3}}));
}

TEST_CASE("reachability-closure: results are appended, not cleared, and seed subsumption") {
  // Both 1 and 2 are leaves, so the only genuinely new completion would be
  // {1, 2}. But `result` is pre-seeded with {1}, a subset of start: start
  // is therefore already subsumed on entry, so nothing new is appended and
  // the pre-existing entry survives untouched.
  unsigned capacity = 64;
  vector<bitset> parents = make_dag(capacity);
  bitset forbidden(capacity);
  bitset start = make_set(capacity, {1, 2});

  vector<bitset> result = { make_set(capacity, {1}) };
  compute_reachability_closures(parents, forbidden, start, result);
  REQUIRE(same_family(result, {{1}}));
}

TEST_CASE("reachability-closure: limit caps the number of results") {
  // 1 has 20 independent leaf alternatives: {1,2}, {1,3}, ..., {1,21}, all
  // mutually incomparable, so none of them subsumes another.
  unsigned capacity = 64;
  vector<bitset> parents = make_dag(capacity);
  for (unsigned p = 2; p <= 21; p++)
    parents[1].set(p, true);
  bitset forbidden(capacity);
  bitset start = make_set(capacity, {1});

  vector<bitset> unlimited;
  compute_reachability_closures(parents, forbidden, start, unlimited, /*limit=*/100);
  REQUIRE(unlimited.size() == 20);

  vector<bitset> limited;
  compute_reachability_closures(parents, forbidden, start, limited, /*limit=*/5);
  // the loop only breaks once size() > limit, so it may overshoot by one
  REQUIRE(limited.size() <= 6);
  REQUIRE(limited.size() >= 1);
  for (const bitset& candidate : limited) {
    REQUIRE(candidate.get(1));
    REQUIRE(candidate.count() == 2);
  }
}

TEST_CASE("reachability-closure: works across multiple bitset metadata blocks") {
  // Each metadata block covers 63*64 = 4032 bits, so this spans two blocks.
  const unsigned capacity = 2 * 4032;
  vector<bitset> parents = make_dag(capacity);
  parents[10].set(5000, true);
  parents[10].set(8000, true);
  bitset forbidden(capacity);
  bitset start = make_set(capacity, {10});

  vector<bitset> result;
  compute_reachability_closures(parents, forbidden, start, result, /*limit=*/10);
  REQUIRE(same_family(result, {{10, 5000}, {10, 8000}}));
}


TEST_CASE("reachability-closure: nested duplications") {
  // 0 -> {1, 2}; 1 is a leaf; 2 -> {3, 4}; 4 -> {5, 6}; 3, 5, 6 are leaves.
  const unsigned capacity = 64;
  vector<bitset> parents = make_dag(capacity);
  parents[0].set(1, true);
  parents[0].set(2, true);
  parents[2].set(3, true);
  parents[2].set(4, true);
  parents[4].set(5, true);
  parents[4].set(6, true);
  bitset forbidden(capacity);
  bitset start = make_set(capacity, {0});

  vector<bitset> result;
  compute_reachability_closures(parents, forbidden, start, result, /*limit=*/10);
  REQUIRE(same_family(result, {{0, 1}, {0, 2, 3}, {0, 2, 4, 5}, {0, 2, 4, 6}}));
}
