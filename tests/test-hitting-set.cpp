/*
 * This file is part of the source code of the software program
 * NapSAT. It is protected by applicable copyright laws.
 *
 * This source code is protected by the terms of the MIT License.
 */
/**
 * @file test-hitting-set.cpp
 * @author Robin Coutelier
 *
 * @brief Unit tests for napsat::compute_hitting_sets: given a family of sets,
 * it must return exactly the inclusion-minimal sets that intersect every set
 * in the family.
 */
#include "../src/utils/hitting-set.hpp"
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

  bool hits_everything(const bitset& candidate, const vector<bitset>& to_hit)
  {
    for (const bitset& s : to_hit)
      if (!candidate.has_intersection(s))
        return false;
    return true;
  }

  // No result may be a (non-strict) superset of another: every returned set
  // must be inclusion-minimal within the returned family.
  bool all_pairwise_incomparable(const vector<bitset>& sets)
  {
    for (size_t i = 0; i < sets.size(); i++)
      for (size_t j = 0; j < sets.size(); j++)
        if (i != j && sets[j] <= sets[i])
          return false;
    return true;
  }
}

TEST_CASE("hitting-set: single set, single element") {
  vector<bitset> to_hit = { make_set(64, {5}) };
  vector<bitset> result;
  compute_hitting_sets(to_hit, result);
  REQUIRE(same_family(result, {{5}}));
}

TEST_CASE("hitting-set: single set, multiple elements") {
  // Any singleton drawn from the set is a minimal hitting set on its own.
  // limit is passed generously so the search doesn't stop after the first one.
  vector<bitset> to_hit = { make_set(64, {2, 5, 9}) };
  vector<bitset> result;
  compute_hitting_sets(to_hit, result, /*limit=*/10);
  REQUIRE(same_family(result, {{2}, {5}, {9}}));
}

TEST_CASE("hitting-set: two disjoint sets require one element from each") {
  vector<bitset> to_hit = { make_set(64, {0, 1}), make_set(64, {10, 11}) };
  vector<bitset> result;
  compute_hitting_sets(to_hit, result, /*limit=*/10);
  REQUIRE(same_family(result, {{0, 10}, {0, 11}, {1, 10}, {1, 11}}));
}

TEST_CASE("hitting-set: shared element subsumes the combinations that don't use it") {
  // {0,1,2} and {2,3,4} share chunk 2: {2} alone hits both, so every
  // combination that could be built without it ({0,3}, {0,4}, {1,3}, {1,4})
  // is a genuinely separate minimal hitting set, while any combination that
  // includes 2 alongside something else ({0,2}, {2,3}, ...) is subsumed.
  vector<bitset> to_hit = { make_set(64, {0, 1, 2}), make_set(64, {2, 3, 4}) };
  vector<bitset> result;
  compute_hitting_sets(to_hit, result, /*limit=*/10);
  REQUIRE(same_family(result, {{2}, {0, 3}, {0, 4}, {1, 3}, {1, 4}}));
}

TEST_CASE("hitting-set: element common to all sets yields the unique size-1 answer") {
  vector<bitset> to_hit = {
    make_set(64, {7, 100, 200}),
    make_set(64, {7, 150, 250}),
    make_set(64, {7, 12}),
  };
  vector<bitset> result;
  compute_hitting_sets(to_hit, result);
  REQUIRE(same_family(result, {{7}}));
}

TEST_CASE("hitting-set: every returned set actually hits every input set") {
  vector<bitset> to_hit = {
    make_set(128, {1, 40, 90}),
    make_set(128, {2, 40, 91}),
    make_set(128, {1, 2, 92}),
  };
  vector<bitset> result;
  compute_hitting_sets(to_hit, result, /*limit=*/10);
  REQUIRE(!result.empty());
  for (const bitset& candidate : result)
    REQUIRE(hits_everything(candidate, to_hit));
}

TEST_CASE("hitting-set: no returned set is redundant with another") {
  vector<bitset> to_hit = {
    make_set(128, {1, 40, 90}),
    make_set(128, {2, 40, 91}),
    make_set(128, {1, 2, 92}),
  };
  vector<bitset> result;
  compute_hitting_sets(to_hit, result, /*limit=*/10);
  REQUIRE(all_pairwise_incomparable(result));
}

TEST_CASE("hitting-set: limit caps the number of results") {
  // Two disjoint pairs give 4 minimal hitting sets; ask for far more than
  // that to confirm the search actually enumerates all of them when unbound.
  vector<bitset> to_hit = { make_set(64, {0, 1}), make_set(64, {10, 11}) };

  vector<bitset> unlimited;
  compute_hitting_sets(to_hit, unlimited, /*limit=*/100);
  REQUIRE(unlimited.size() == 4);

  vector<bitset> limited;
  compute_hitting_sets(to_hit, limited, /*limit=*/1);
  // the loop only breaks once size() > limit, so it may overshoot by one
  REQUIRE(limited.size() <= 2);
  REQUIRE(limited.size() >= 1);
  for (const bitset& candidate : limited)
    REQUIRE(hits_everything(candidate, to_hit));
}

TEST_CASE("hitting-set: default limit returns just the smallest hitting set") {
  vector<bitset> to_hit = { make_set(64, {0, 1}), make_set(64, {10, 11}) };
  vector<bitset> result;
  compute_hitting_sets(to_hit, result); // limit defaults to 0
  REQUIRE(result.size() == 1);
  REQUIRE(hits_everything(result[0], to_hit));
}

TEST_CASE("hitting-set: large hitting set if bad ordering") {
  vector<bitset> to_hit = {
    make_set(64, {0, 1}),
    make_set(64, {0, 2}),
    make_set(64, {0, 3}),
    make_set(64, {0, 4}),
    make_set(64, {0, 5}),
    make_set(64, {0, 6}),
    make_set(64, {0, 7}),
    make_set(64, {0, 8}),
    make_set(64, {0, 9}),
    make_set(64, {0, 10}),
    make_set(64, {0, 11}),
    make_set(64, {0, 12}),
    make_set(64, {0, 13}),
    make_set(64, {0, 14}),
    make_set(64, {0, 15}),
    make_set(64, {0, 16}),
    make_set(64, {0, 17}),
    make_set(64, {0, 18}),
    make_set(64, {0, 19}),
 };
  vector<bitset> result;
  compute_hitting_sets(to_hit, result); // limit defaults to 0
  REQUIRE(result.size() == 2);
  REQUIRE(hits_everything(result[0], to_hit));
  REQUIRE(hits_everything(result[1], to_hit));
  REQUIRE(same_family(result, {{0}, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19}}));
}

TEST_CASE("hitting-set: double large hitting set if bad ordering") {
  vector<bitset> to_hit = {
    make_set(64, {0, 1}),
    make_set(64, {0, 2}),
    make_set(64, {0, 3}),
    make_set(64, {0, 4}),
    make_set(64, {0, 5}),
    make_set(64, {0, 6}),
    make_set(64, {0, 7}),
    make_set(64, {0, 8}),
    make_set(64, {0, 9}),
    make_set(64, {10, 11}),
    make_set(64, {10, 12}),
    make_set(64, {10, 13}),
    make_set(64, {10, 14}),
    make_set(64, {10, 15}),
    make_set(64, {10, 16}),
    make_set(64, {10, 17}),
    make_set(64, {10, 18}),
    make_set(64, {10, 19}),
 };
  vector<bitset> result;
  compute_hitting_sets(to_hit, result); // limit defaults to 0
  REQUIRE(result.size() == 4);
  for (const bitset& candidate : result)
    REQUIRE(hits_everything(candidate, to_hit));
  REQUIRE(all_pairwise_incomparable(result));

  REQUIRE(same_family(result, {
    {0, 10},
    {1, 2, 3, 4, 5, 6, 7, 8, 9, 10},
    {0, 11, 12, 13, 14, 15, 16, 17, 18, 19},
    {1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13, 14, 15, 16, 17, 18, 19},
  }));
}

TEST_CASE("hitting-set: works across multiple bitset metadata blocks") {
  // Each metadata block covers 63*64 = 4032 bits, so this spans two blocks.
  const unsigned capacity = 2*4032;
  vector<bitset> to_hit = {
    make_set(capacity, {10, 5000, 8000}),
    make_set(capacity, {10, 5001}),
  };
  vector<bitset> result;
  compute_hitting_sets(to_hit, result, /*limit=*/10);
  REQUIRE(same_family(result, {{10}, {5000, 5001}, {8000, 5001}}));
}

TEST_CASE("hitting-set: previous bug: no hitting set found") {
  vector<bitset> to_hit = {
    make_set(128, {58, 60, 64}),
    make_set(128, {6, 9, 15, 17, 34}),
    make_set(128, {6, 15, 17, 34}),
 };
  vector<bitset> result;
  compute_hitting_sets(to_hit, result); // limit defaults to 0
  for (const bitset& candidate : result)
    REQUIRE(hits_everything(candidate, to_hit));
  REQUIRE(all_pairwise_incomparable(result));
  REQUIRE(same_family(result, {
    { 6, 58},
    {15, 58},
    {17, 58},
    {34, 58},
    { 6, 60},
    {15, 60},
    {17, 60},
    {34, 60},
    { 6, 64},
    {15, 64},
    {17, 64},
    {34, 64},
  }));
}
