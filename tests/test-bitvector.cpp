#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>
#include "../src/utils/bitvector.hpp"
#include <iostream>

using namespace napsat;

TEST_CASE("BitVector basic operations", "[bitvector]") {
  bitvector bv(10);
  REQUIRE(bv.size() == 10);
  for (size_t i = 0; i < bv.size(); ++i) {
    REQUIRE(bv.get(i) == false);
  }
  bv.set(3, true);
  REQUIRE(bv.get(3) == true);
  bv.set(3, false);
  REQUIRE(bv.get(3) == false);
}

TEST_CASE("BitVector set and get", "[bitvector]") {
  bitvector bv(100);
  bv.set(50, true);
  REQUIRE(bv.get(50) == true);
  bv.set(50, false);
  REQUIRE(bv.get(50) == false);
  bv.set(99, true);
  REQUIRE(bv.get(99) == true);
}

TEST_CASE("BitVector negation", "[bitvector]") {
  bitvector bv(5);
  bv.set(2, true);
  bv.negate();
  REQUIRE(bv.get(0) == true);
  REQUIRE(bv.get(1) == true);
  REQUIRE(bv.get(2) == false);
  REQUIRE(bv.get(3) == true);
  REQUIRE(bv.get(4) == true);
}

TEST_CASE("BitVector negation large", "[bitvector]") {
  bitvector bv(256);
  bv.set(2, true);
  bv.set(153, true);
  bv.negate();
  for (size_t i = 0; i < bv.size(); ++i) {
    REQUIRE(bv.get(i) == (i != 2 && i != 153));
  }
}

TEST_CASE("BitVector clear", "[bitvector]") {
  bitvector bv(8);
  for (size_t i = 0; i < bv.size(); ++i) {
    bv.set(i, true);
  }
  bv.clear();
  for (size_t i = 0; i < bv.size(); ++i) {
    REQUIRE(bv.get(i) == false);
  }
}

TEST_CASE("BitVector bitwise or", "[bitvector]") {
  bitvector bv1(10);
  bitvector bv2(10);
  bv1.set(1, true);
  bv1.set(3, true);
  bv2.set(2, true);
  bv2.set(3, true);

  bitvector result = bv1 | bv2;

  REQUIRE(result.get(0) == false);
  REQUIRE(result.get(1) == true);
  REQUIRE(result.get(2) == true);
  REQUIRE(result.get(3) == true);
  REQUIRE(result.get(4) == false);
}

TEST_CASE("BitVector bitwise local or", "[bitvector]") {
  bitvector bv1(10);
  bitvector bv2(10);
  bv1.set(1, true);
  bv1.set(3, true);
  bv2.set(2, true);
  bv2.set(3, true);

  bv1 |= bv2;

  REQUIRE(bv1.get(0) == false);
  REQUIRE(bv1.get(1) == true);
  REQUIRE(bv1.get(2) == true);
  REQUIRE(bv1.get(3) == true);
  REQUIRE(bv1.get(4) == false);
}

TEST_CASE("BitVector bitwise local or 2", "[bitvector]") {
  bitvector bv1(10);
  bitvector bv2(10);
  bv2.set(2, true);
  bv2.set(3, true);

  bv1 |= bv2;

  REQUIRE(bv1.get(0) == false);
  REQUIRE(bv1.get(1) == false);
  REQUIRE(bv1.get(2) == true);
  REQUIRE(bv1.get(3) == true);
  REQUIRE(bv1.get(4) == false);
}

TEST_CASE("BitVector bitwise and", "[bitvector]") {
  bitvector bv1(10);
  bitvector bv2(10);
  bv1.set(1, true);
  bv1.set(3, true);
  bv2.set(2, true);
  bv2.set(3, true);

  bitvector result = bv1 & bv2;

  REQUIRE(result.get(0) == false);
  REQUIRE(result.get(1) == false);
  REQUIRE(result.get(2) == false);
  REQUIRE(result.get(3) == true);
  REQUIRE(result.get(4) == false);
}

TEST_CASE("BitVector bitwise xor", "[bitvector]") {
  bitvector bv1(10);
  bitvector bv2(10);
  bv1.set(1, true);
  bv1.set(3, true);
  bv2.set(2, true);
  bv2.set(3, true);

  bitvector result = bv1 ^ bv2;

  REQUIRE(result.get(0) == false);
  REQUIRE(result.get(1) == true);
  REQUIRE(result.get(2) == true);
  REQUIRE(result.get(3) == false);
  REQUIRE(result.get(4) == false);
}

TEST_CASE("BitVector set operations", "[bitvector]") {
  bitvector bv1(4);
  bitvector bv2(4);
  bv1.set(2, true);
  bv2.set(3, true);

  REQUIRE(bv1.count_non_zero() == 1);
  REQUIRE(bv2.count_non_zero() == 1);
  REQUIRE(!(bv1 == bv2));
  REQUIRE(!(bv1 > bv2));
  REQUIRE(!(bv1 < bv2));
}
