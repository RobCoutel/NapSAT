#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_all.hpp>

#include "../src/utils/bitset.hpp"
#include <bitset>
#include <iostream>

TEST_CASE("inplace: 0 |= 1") {
  bitset a(1000);
  bitset b(1000);
  b.set(0, true);
  a |= b;
  REQUIRE(a.get(0) == true);
}

TEST_CASE("inplace: and") {
  const size_t CNT = 2000;
  const size_t SZE = 4000;

  bitset a(SZE), b(SZE);

  for (size_t i = 0; i < CNT; ++i) {
    unsigned r = random();
    if (r & 0x1) a.set(i, true);
    if (r & 0x2) b.set(i, true);
  }

  bitset ref = a & b;
  a &= b;
  REQUIRE(a == ref);
}

TEST_CASE("inplace: or") {
  const size_t CNT = 2000;
  const size_t SZE = 4000;

  bitset a(SZE), b(SZE);

  for (size_t i = 0; i < CNT; ++i) {
    unsigned r = random();
    if (r & 0x1) a.set(i, true);
    if (r & 0x2) b.set(i, true);
  }

  bitset ref = a | b;
  a |= b;
  REQUIRE(a == ref);
}

TEST_CASE("inplace: xor") {
  const size_t CNT = 2000;
  const size_t SZE = 4000;

  bitset a(SZE), b(SZE);

  for (size_t i = 0; i < CNT; ++i) {
    unsigned r = random();
    if (r & 0x1) a.set(i, true);
    if (r & 0x2) b.set(i, true);
  }

  bitset ref = a ^ b;
  a ^= b;
  REQUIRE(a == ref);
}

TEST_CASE("clear") {
  bitset a(2*4032);
  REQUIRE(a.capacity() == 2*4032);
  a.clear();
  REQUIRE(a.capacity() == 2*4032);
}

TEST_CASE("resize") {
  bitset a(1);
  REQUIRE(a.capacity() == 4032);
  a.resize(4032);
  REQUIRE(a.capacity() == 4032);

  bitset b(4032);
  REQUIRE(b.capacity() == 4032);
  b.resize(4032 * 2);
  REQUIRE(b.capacity() == 2*4032);
}




TEST_CASE("iterator mini") {
  const size_t CNT = GENERATE(1, 10, 100, 1000, 4000, 6000, 10000, 12000);

  bitset a(CNT);
  a.set(3, true);

  auto it = a.cbegin();
  REQUIRE(it != a.cend());
  REQUIRE(*it == 3);
  auto nit = it++;
  REQUIRE(it == a.cend());
  REQUIRE(*nit == 3);
  auto nnit = ++nit;
  REQUIRE(nnit == nit);
  REQUIRE(nit == a.cend());
}

TEST_CASE("iterator empty") {
  const size_t CNT = GENERATE(1, 10, 100, 1000, 4000, 6000, 10000, 12000);
  bitset a(CNT);
  REQUIRE(a.empty());
  auto it = a.cbegin();
  REQUIRE(it == a.cend());
}

TEST_CASE("iterator metadata") {
  const size_t CNT = GENERATE(1, 10, 100, 1000, 4000, 6000, 10000, 12000, 24000, 54000);

  bitset a(CNT);

  for (size_t i = 0; i < CNT; i += 3) {
    a.set(i, true);
  }

  auto it = a.cbegin();
  unsigned cnt = 0;
  while(it != a.cend()) {
    REQUIRE(cnt == *it);
    ++it;
    cnt += 3;
  }
}

TEST_CASE("one big set") {
  const size_t CNT = GENERATE(10, 100, 1000, 4000, 6000, 10000, 12000, 24000, 54000);

  bitset a(CNT);

  a.set(CNT - 3, true);

  auto it = a.cbegin();
  REQUIRE_FALSE(it == a.cend());
  REQUIRE(*it == CNT - 3);
  ++it;
  REQUIRE(it == a.cend());
}


TEST_CASE("Counter with three metadata blocks") {
  bitset a(8065);
  a.set(3928, true);
  a.set(3967, true);
  a.set(4007, true);
  a.set(5000, true);

  REQUIRE(a.count() == 4);

  a.clear();
  REQUIRE(a.count() == 0);
  a.set(253, true);
  REQUIRE(a.count() == 1);
}


template<size_t I>
static bool operator==(const bitset &b, const std::bitset<I> &r) {
  assert(b.capacity() >= I);

  for (size_t i = 0; i < I; ++i) {
    if (b[i] != r[i]) return false;
  }

  return true;
}

static std::ostream& operator<<(std::ostream& os, const bitset& b) {
  for (size_t i = 0; i < b.capacity(); ++i) {
    os << (b[i] ? '1' : '0');
  }
  return os;
}


TEST_CASE("insert bits") {
  const size_t CNT = 4000;
  const size_t ROUNDS = 10000;

  bitset bs;
  std::bitset<CNT> ref;

  static_assert(RAND_MAX > CNT);
  REQUIRE( bs.capacity() == 4032 );

  for (size_t i = 0; i < ROUNDS; ++i) {
    unsigned r = random() % CNT;
    bool val = random() % 4;

    bs.set(r, val);
    ref.set(r, val);
  }

  REQUIRE(ref.count() == bs.count());
  REQUIRE(bs == ref);
}

TEST_CASE("test increase") {
  const size_t CNT = 12000;
  const size_t ROUNDS = 10000;

  bitset bs;
  std::bitset<CNT> ref;

  static_assert(RAND_MAX > CNT);
  REQUIRE(bs.capacity() == 4032);

  for (size_t i = 0; i < ROUNDS; ++i) {
    unsigned r = random() % CNT / 3;
    bool val = random() % 2;

    bs.set(r, val);
    ref.set(r, val);
  }

  bs.resize(CNT);
  bs.set(CNT-1, true);
  ref.set(CNT-1, true);
  REQUIRE(bs.capacity() == 3 * 4032);
  REQUIRE(bs == ref);
}

TEST_CASE("equality") {
  const size_t CNT = 4000;
  const size_t ROUNDS = 10000;

  bitset bs;
  std::bitset<CNT> ref;

  static_assert(RAND_MAX > CNT);
  REQUIRE(bs.capacity() == 4032);

  for (size_t i = 0; i < ROUNDS; ++i) {
    unsigned r = random() % CNT;
    bool val = random() % 2;

    bs.set(r, val);
    ref.set(r, val);
  }

  bitset bs2;
  REQUIRE(bs2.empty());
  REQUIRE(bs2.capacity() == 4032);
  for (size_t i = 0; i < CNT; ++i) {
    if (ref[i]) bs2.set(i, true);
  }

  REQUIRE(bs == bs2);
  REQUIRE_FALSE(bs != bs2);
}

TEST_CASE("clearing") {
  const size_t CNT = 8000;
  const size_t ROUNDS = 10000;

  bitset bs(CNT);

  static_assert(RAND_MAX > CNT);
  REQUIRE(bs.capacity() == 2 * 4032);

  for (size_t i = 0; i < ROUNDS; ++i) {
    unsigned r = random() % CNT;
    bool val = random() % 2;

    bs.set(r, val);
  }

  for (size_t i = 0; i < CNT; ++i) {
    bs.set(i, false);
  }

  bitset bs2;
  REQUIRE(bs2.empty());
  REQUIRE(bs2.capacity() == 4032);
  bs2.resize(CNT);
  REQUIRE(bs2.capacity() == 2 * 4032);

  REQUIRE(bs == bs2);
  REQUIRE_FALSE(bs != bs2);
}

TEST_CASE("and") {
  const size_t MAX = 12000;
  const size_t RND = 4000;

  bitset b1(MAX), b2(MAX);
  std::bitset<MAX> r1, r2;

  for (size_t i = 0; i < RND; ++i) {
    unsigned r = random() % MAX;
    bool val = random() % 2;
    b1.set(r, val);
    r1.set(r, val);
  }

  for (size_t i = 0; i < RND; ++i) {
    unsigned r = random() % MAX;
    bool val = random() % 2;
    b2.set(r, val);
    r2.set(r, val);
  }

  REQUIRE(b1 == r1);
  REQUIRE(b2 == r2);
  bitset b = b1 & b2;
  auto r = r1 & r2;
  REQUIRE(b == r);
}

TEST_CASE("or") {
  const size_t MAX = 3000;
  const size_t RND = 500;

  bitset b1(MAX), b2(MAX);
  std::bitset<MAX> r1, r2;

  for (size_t i = 0; i < RND; ++i) {
    unsigned r = random() % MAX;
    bool val = random() % 2;
    b1.set(r, val);
    r1.set(r, val);
  }

  for (size_t i = 0; i < RND; ++i) {
    unsigned r = random() % MAX;
    bool val = random() % 2;
    b2.set(r, val);
    r2.set(r, val);
  }

  REQUIRE(b1 == r1);
  REQUIRE(b2 == r2);
  bitset b = b1 | b2;
  auto r = r1 | r2;
  REQUIRE(b == r);
}

TEST_CASE("xor") {
  const size_t MAX = 3000;
  const size_t RND = 500;

  bitset b1(MAX), b2(MAX);
  std::bitset<MAX> r1, r2;

  for (size_t i = 0; i < RND; ++i) {
    unsigned r = random() % MAX;
    bool val = random() % 2;
    b1.set(r, val);
    r1.set(r, val);
  }

  for (size_t i = 0; i < RND; ++i) {
    unsigned r = random() % MAX;
    bool val = random() % 2;
    b2.set(r, val);
    r2.set(r, val);
  }

  REQUIRE(b1 == r1);
  REQUIRE(b2 == r2);
  bitset b = b1 ^ b2;
  auto r = r1 ^ r2;
  REQUIRE(b == r);
}

// subset for std::bitset
template<size_t I>
static bool operator<=(const std::bitset<I> &l, const std::bitset<I> &r) {
  return (l & ~r) == 0;
}

template<size_t I>
static bool operator<(const std::bitset<I> &l, const std::bitset<I> &r) {
  return l <= r && l != r;
}

TEST_CASE("ref: subset") {
  const size_t MAX = 3000;
  const size_t RND = 500;

  bitset b1(MAX), b2(MAX);
  std::bitset<MAX> r1, r2;

  for (size_t i = 0; i < RND; ++i) {
    unsigned r = random() % MAX;
    bool val = random() % 2;
    b1.set(r, val);
    r1.set(r, val);
  }

  for (size_t i = 0; i < RND; ++i) {
    unsigned r = random() % MAX;
    bool val = random() % 2;
    b2.set(r, val);
    r2.set(r, val);
  }

  REQUIRE(b1 == r1);
  REQUIRE(b2 == r2);
  bool b = b1 < b2;
  bool r = r1 < r2;
  REQUIRE(b == r);
}

TEST_CASE("ref: subset-proper") {
  const size_t MAX = 3000;
  const size_t RND = 500;

  bitset b1(MAX), b2(MAX);
  std::bitset<MAX> r1, r2;

  for (size_t i = 0; i < RND; ++i) {
    unsigned r = random() % MAX;
    bool val = random() % 2;
    b1.set(r, val);
    r1.set(r, val);
  }

  for (size_t i = 0; i < RND; ++i) {
    unsigned r = random() % MAX;
    bool val = random() % 2;
    b2.set(r, val);
    r2.set(r, val);
  }

  REQUIRE(b1 == r1);
  REQUIRE(b2 == r2);
  bool b = b1 <= b2;
  bool r = r1 <= r2;
  REQUIRE(b == r);
}



TEST_CASE("subset") {
  const size_t CNT = GENERATE(1, 10, 1000, 1000, 4000, 6000, 12000);
  bitset super(CNT), sub(CNT);

  for (size_t i = 0; i < CNT; ++i) {
    if (random() % 4 != 0) continue;
    super.set(i, true);
    if (random() % 2) continue;
    sub.set(i, true);
  }

  super.set(4001, true);
  sub.set(4001, false);

  REQUIRE(super >= sub);
  REQUIRE(super > sub);
  REQUIRE(sub <= super);
  REQUIRE(sub < super);

  REQUIRE(sub <= sub);
  REQUIRE(super <= super);
  REQUIRE(sub >= sub);
  REQUIRE(super >= super);
  REQUIRE_FALSE(sub < sub);
  REQUIRE_FALSE(super < super);
  REQUIRE_FALSE(sub > sub);
  REQUIRE_FALSE(super > super);
}

TEST_CASE("subset empty") {
  const size_t CNT = GENERATE(1, 10, 1000, 1000, 4000, 6000, 12000);

  bitset empty1(CNT);
  bitset empty2(CNT);

  REQUIRE(empty1 <= empty2);
  REQUIRE(empty1 >= empty2);
  REQUIRE_FALSE(empty1 < empty2);
  REQUIRE_FALSE(empty1 > empty2);
  REQUIRE(empty1 == empty2);
  REQUIRE_FALSE(empty1 != empty2);
}

TEST_CASE("subset small") {
  const int CNT = GENERATE(1, 10, 1000, 1000, 4000, 6000, 12000);

  bitset s1(CNT);
  bitset s2(CNT);

  const unsigned POS = GENERATE_COPY(take(10, random(0, CNT)));
  s2.set(POS, true);

  REQUIRE(s1 <= s2);
  REQUIRE_FALSE(s1 >= s2);
  REQUIRE(s1 < s2);
  REQUIRE_FALSE(s1 > s2);
  REQUIRE_FALSE(s1 == s2);
  REQUIRE(s1 != s2);
}

TEST_CASE("intersection") {
  const int CNT = GENERATE(10, 1000, 1000, 4000, 6000, 12000);

  bitset s1(CNT);
  bitset s2(CNT);

  s1.set(0, true);
  s1.set(1, true);
  s2.set(0, true);

  for (int i = 2; i < CNT; ++i) {
    int r = random();
    if (r % 2) continue;
    s1.set(i, true);
    if (r % 4) continue;
    s2.set(i, true);
  }

  REQUIRE(s1.has_intersection(s1));
  REQUIRE(s2.has_intersection(s2));

  REQUIRE_FALSE(s1.has_difference(s1));
  REQUIRE_FALSE(s2.has_difference(s2));

  REQUIRE(s1.has_intersection(s2));
  REQUIRE(s1.has_difference(s2));
}
