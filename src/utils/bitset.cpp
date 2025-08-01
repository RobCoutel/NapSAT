#include "bitset.hpp"

#include <cassert>
#include <climits>
#include <algorithm>

#if __cplusplus >= 202002L
#include <bit>
#endif

// mask the lower/upper n bits
#define MSK_LO(N)  msk_lo(N)
#define MSK_HI(N)  msk_hi(N)
static inline constexpr uint64_t msk_lo(const unsigned n) {
  assert(n <= 64);
  return ~(-1ULL << (n & 0x3F)) - (n >> 6);
}

static inline constexpr uint64_t msk_hi(const unsigned n) {
  assert(n <= 64);
  return ~(-1ULL >> (n & 0x3F)) - (n >> 6);
}

// lowest bit
#define LB(X) (X & MSK_BT(0))

// bitmask of the next metadata identifier (MSBit) and data fields
#define M_NEXT_MSK  MSK_HI(1)
#define M_DATA_MSK  (~M_NEXT_MSK)

static inline constexpr unsigned count_bits(uint64_t b, uint64_t m = -1) {
#if __cplusplus >= 202002L
  return std::popcount(b & m);
#else
  return __builtin_popcountll(b & m);
#endif
}

static inline constexpr unsigned count_zero_r(uint64_t b) {
#if __cplusplus >= 202002L
  return std::countr_zero(b);
#else
  // not defined for 0
  return b ? __builtin_ctzll(b) : sizeof(uint64_t) * CHAR_BIT;
#endif
}

/** sets p to the index of the next 1 in b. returns true if one exist */
static inline bool next_bit(uint64_t b, size_t &p) {
  assert(p < BITS);
  uint64_t v = b & ~MSK_LO(p + 1);
  assert(!v || p < count_zero_r(v));
  p = count_zero_r(v);
  return v;
}

static inline bool first_bit(uint64_t b, size_t &p) {
  p = count_zero_r(b);
  return b;
}

static inline void set_bit(uint64_t &b, unsigned n) {
  assert(n < BITS);
  b |= MSK_BT(n);
}

static inline void clear_bit(uint64_t &b, unsigned n) {
  assert(n < BITS);
  b &= ~MSK_BT(n);
}

/** Counts the number of bits set in b within the first n bits. */
static inline constexpr unsigned count_bits_lo(uint64_t b, unsigned n) {
  return count_bits(b, MSK_LO(n));
}

bitset::bitset() noexcept : _bits(1) {
  assert(_bits[0] == 0);
}

bitset::bitset(size_t size) noexcept : _bits(MP(size-1) + 1, M_NEXT_MSK) {
  // clear the last entry
  _bits.back() = 0;
}

bitset::bitset(bitstore &&v) noexcept : _bits(v) { }

void bitset::resize(size_t size) {
  if (size <= capacity())
    return;

  auto it = _bits.begin();
  while(*(it++) & M_NEXT_MSK);
  *(it - 1) |= M_NEXT_MSK;

  size_t cnt_n = MP(size-1) + 1;
  size_t cnt_m = it - _bits.begin();
  assert(cnt_m < cnt_n);
  unsigned n = cnt_n - cnt_m;
  it = _bits.insert(it, n, M_NEXT_MSK);
  *(it + n - 1) = 0x00;
}

// TODO: all with the same size? use a manager? Pass data offset via param from manager?

/** Gets the offset in the data, based on the metadata and index. */
unsigned get_offset(const uint64_t *md, size_t index) {
  unsigned cnt = 0;
  const uint64_t *p = md;
  while (index >= MMS) {
    assert(*md & M_NEXT_MSK);
    cnt += count_bits(*(p++), M_DATA_MSK);
    index -= MMS;
  }
  assert(MP(index) == 0);
  cnt += count_bits_lo(*p, MO(index));
  // skip the remaining metadata blocks
  while (*(p++) & M_NEXT_MSK);
  return cnt + (p - md);
}

void bitset::set(size_t index, bool value) {
  unsigned mp = MP(index), mo = MO(index);
  assert(capacity() >= index);
  bool mb = get_bit(_bits[mp], mo);

  // no need to do anything
  // TODO check if bool is normalize (and remove if so)
  if (!mb && !value) return;

  // get the index of the data field
  unsigned d_idx = get_offset(_bits.data(), index);

  // check if the corresponding data field exists
  if (!mb) {
    assert(value);
    // insert the new field
    _bits.insert(_bits.begin() + d_idx, 0x00);
    set_bit(_bits[mp], mo);
  }

  (value ? set_bit : clear_bit)(_bits[d_idx], index % BITS);

  // clear the data field if empty
  if (_bits[d_idx] == 0x00) {
    assert(!value);
    _bits.erase(_bits.begin() + d_idx);
    clear_bit(_bits[mp], mo);
  }
}

void bitset::clear() {
  auto it = _bits.begin();
  do {
    *it &= M_NEXT_MSK;
  } while(*(it++) & M_NEXT_MSK);
}

bool bitset::empty() const {
  auto it = _bits.cbegin();
  assert(it != _bits.cend());
  do {
    if (*it & M_DATA_MSK) return false;
  } while (*(it++) & M_DATA_MSK);
  return true;
}

size_t bitset::capacity() const {
  auto it = _bits.cbegin();
  while(*(it++) & M_NEXT_MSK);
  assert(it <= _bits.cend());
  return BITS * MS * (it - _bits.cbegin());
}

size_t bitset::count() const {
  size_t cnt = 0;
  auto it = _bits.cbegin();
  while(*(it++) & M_NEXT_MSK);
  while(it != _bits.cend()) cnt += count_bits(*(it++));
  return cnt;
}

bool bitset::operator==(const bitset &other) const {
  assert(capacity() == other.capacity());
  return _bits == other._bits;
}

bool bitset::operator!=(const bitset &other) const {
  assert(capacity() == other.capacity());
  return _bits != other._bits;
}

/** Checks if the bitset of a is a subset of b. */
static inline bool subset(uint64_t a, uint64_t b) {
  /*
   * a b |  a & ~b
   * ----+-------
   * 0 0 |  0
   * 0 1 |  0
   * 1 0 |  1
   * 1 1 |  0
   */
  return (a & ~b) == 0;
}

static bool subset_check(const bitset::bitstore &a, const bitset::bitstore &b, bool proper) {
  // check metadata
  bool equal = proper;
  auto it1 = a.cbegin();
  auto it2 = b.cbegin();
  do {
    if (!subset(*it1, *it2)) return false;
    equal = equal && (*it1 == *it2);
  } while (*(it1++) & *(it2++) & M_NEXT_MSK);

  // check data
  auto m1 = a.cbegin();
  auto m2 = b.cbegin();
  do {
    uint64_t v1 = *m1 & M_DATA_MSK;
    uint64_t v2 = *m2 & M_DATA_MSK;
    uint64_t s = ~v1 & v2; // skip
    uint64_t c = v1 & v2;  // check
    assert(!(v1 & ~v2)); // checked above
    assert(!(s & c)); // either check or skip
    for (; c; c >>= 1, s >>= 1) {
      if (LB(c)) {
        if (!subset(*it1, *it2)) return false;
        equal = equal && (*it1 == *it2);
        ++it1; ++it2;
      } else if (LB(s)) {
        assert(!equal);
        ++it2;
      }
    }
    assert(!count_bits(s) || !equal);
    it2 += count_bits(s);
  } while (*(m1++) & *(m2++) & M_NEXT_MSK);

  // check for proper => !equal
  return !proper || !equal;
}

bool bitset::operator<=(const bitset &other) const {
  assert(capacity() == other.capacity());
  return subset_check(_bits, other._bits, false);
}

bool bitset::operator>=(const bitset &other) const {
  assert(capacity() == other.capacity());
  return subset_check(other._bits, _bits, false);
}

bool bitset::operator<(const bitset &other) const {
  assert(capacity() == other.capacity());
  return subset_check(_bits, other._bits, true);
}

bool bitset::operator>(const bitset &other) const {
  assert(capacity() == other.capacity());
  return subset_check(other._bits, _bits, true);
}

typedef uint64_t (*op_t)(uint64_t, uint64_t);

static bitset::bitstore combine(const bitset::bitstore &a, const bitset::bitstore &b, const op_t op) {
  auto it1 = a.cbegin();
  auto it2 = b.cbegin();
  while (*(it1++) & *(it2++) & M_NEXT_MSK);

  bitset::bitstore out(it1 - a.cbegin());

  // check data
  auto m1 = a.cbegin();
  auto m2 = b.cbegin();
  size_t mo = 0;
  do {
    uint64_t v1 = *m1 & M_DATA_MSK;
    uint64_t v2 = *m2 & M_DATA_MSK;
    uint64_t o = 0;
    int rem = BITS - 1;
    for (; v1 | v2; v1 >>= 1, v2 >>= 1, o >>= 1) {
      -- rem;
      uint64_t a1 = LB(v1) ? *(it1++) : 0;
      uint64_t a2 = LB(v2) ? *(it2++) : 0;
      uint64_t v = op(a1, a2);
      if (v) {
        o |= MSK_HI(1);
        out.push_back(v);
      }
    }
    assert(rem >= 0);
    o >>= rem;
    assert((o & M_DATA_MSK) == o);
    out[mo++] = o | M_NEXT_MSK;
  } while (*(m1++) & *(m2++) & M_NEXT_MSK);
  assert((*(m1-1) & M_NEXT_MSK) == (*(m2-1) & M_NEXT_MSK));
  // clear the next bit for the last metadata word
  out[mo-1] &= ~M_NEXT_MSK;

  return out;
}

bitset bitset::operator&(const bitset &other) const {  // check metadata
  assert(capacity() == other.capacity());
  const auto o = [](uint64_t a, uint64_t b) { return a & b; };
  return bitset(std::move(combine(_bits, other._bits, o)));
}

bitset bitset::operator|(const bitset &other) const {
  assert(capacity() == other.capacity());
  const auto o = [](uint64_t a, uint64_t b) { return a | b; };
  return bitset(std::move(combine(_bits, other._bits, o)));
}

bitset bitset::operator^(const bitset &other) const {
  assert(capacity() == other.capacity());
  const auto o = [](uint64_t a, uint64_t b) { return a ^ b; };
  return bitset(std::move(combine(_bits, other._bits, o)));
}

bitset bitset::operator-(const bitset &other) const {
  assert(capacity() == other.capacity());
  const auto o = [](uint64_t a, uint64_t b) { return a ^ (a & b); };
  return bitset(std::move(combine(_bits, other._bits, o)));
}

static void update(bitset::bitstore &a, const bitset::bitstore &b, const op_t op) {
  auto it1 = a.begin();
  auto it2 = b.cbegin();
  while (*(it1++) & *(it2++) & M_NEXT_MSK);

  // check data
  unsigned m1 = 0;
  unsigned m2 = 0;
  do {
    uint64_t v1 = a[m1] & M_DATA_MSK;
    uint64_t v2 = b[m2] & M_DATA_MSK;
    uint64_t o = 0;
    int rem = BITS - 1;
    for (; v1 | v2; v1 >>= 1, v2 >>= 1, o >>= 1) {
      -- rem;
      uint64_t a1 = LB(v1) ? *(it1) : 0;
      uint64_t a2 = LB(v2) ? *(it2++) : 0;
      uint64_t v = op(a1, a2);
      if (LB(v1)) {
        *it1 = v;
        it1++;
        o |= (v ? MSK_HI(1) : 0);
      } else if (v) {
        it1 = a.insert(it1, v);
        it1++;
        o |= MSK_HI(1);
      }
    }
    assert(rem >= 0);
    o >>= rem;
    a[m1] = o | (b[m2] & M_NEXT_MSK);
    assert((a[m1] & M_NEXT_MSK) == (b[m2] & M_NEXT_MSK));
  } while (a[m1++] & b[m2++] & M_NEXT_MSK);

  // clear all empty fields
  a.erase(std::remove(a.begin() + m1, a.end(), 0), a.end());
}

void bitset::operator&=(const bitset &other) {
  assert(capacity() == other.capacity());
  const auto o = [](uint64_t a, uint64_t b) { return a & b; };
  update(_bits, other._bits, o);
}

void bitset::operator|=(const bitset &other) {
  assert(capacity() == other.capacity());
  const auto o = [](uint64_t a, uint64_t b) { return a | b; };
  update(_bits, other._bits, o);
}

void bitset::operator^=(const bitset &other) {
  assert(capacity() == other.capacity());
  const auto o = [](uint64_t a, uint64_t b) { return a ^ b; };
  update(_bits, other._bits, o);
}

unsigned bitset::iterator::operator*() const {
  assert(_pos_d < BITS);
  return ((_it_m - _b._bits.cbegin()) * MS + _pos_m) * BITS + _pos_d;
}

void bitset::iterator::next() {
  if (_it_d == _b._bits.cend()) {
    return;
  }

  // find the next bit in the data
  if (next_bit(*_it_d, _pos_d)) {
    // next bit found
    return;
  }

  // no more bits, get the next data byte
  _it_d++;
  // no more bytes, we're at the end
  if (_it_d == _b._bits.cend()) {
    _pos_d = 0;
    return;
  }

  // find the first bit in new word
  bool f = first_bit(*_it_d, _pos_d);
  assert(f); (void)f;

  // find the next metadata bit
  if (next_bit(*_it_m & M_DATA_MSK, _pos_m)) {
    // next bit found
    return;
  }

  // next metadata word
  do {
    assert(*_it_m & M_NEXT_MSK);
    _it_m++;
  } while (!first_bit(*_it_m & M_DATA_MSK, _pos_m));

}

bitset::iterator& bitset::iterator::operator++() {
  next();
  return *this;
}

bitset::iterator bitset::iterator::operator++(int) {
  auto o = *this;
  next();
  return o;
}

bool bitset::iterator::operator==(const bitset::iterator &other) {
  assert(std::addressof(other._b) == std::addressof(_b));
  return _it_d == other._it_d && _pos_d == other._pos_d;
}

bool bitset::iterator::operator!=(const bitset::iterator &other) {
  return !(*this == other);
}

bitset::iterator::iterator(const bitset &bs) : _b(bs) { }

bitset::iterator bitset::iterator::begin(const bitset &bs) {
  bitset::iterator it(bs);
  // set the iterator and skip metadata for data
  it._it_d = bs._bits.cbegin();
  while(*(it._it_d++) & M_NEXT_MSK);

  if (it._it_d == bs._bits.cend()) {
    // we're empty
    assert(bs.empty());
    it._it_m = bs._bits.cend();
    it._pos_m = it._pos_d = 0;
  } else {
    // find the first element
    it._it_m = bs._bits.cbegin();
    assert(*it._it_m); // either set of the next bit is set
    for (; !(*it._it_m & M_DATA_MSK); ++it._it_m);
    bool f_m = first_bit(*it._it_m, it._pos_m);
    bool f_d = first_bit(*it._it_d, it._pos_d);
    assert(f_m); (void)f_m;
    assert(f_d); (void)f_d;
  }
  return it;
}

bitset::iterator bitset::iterator::end(const bitset &bs) {
  bitset::iterator it(bs);
  it._it_d = bs._bits.cend();
  it._it_m = bs._bits.cend();
  it._pos_d = 0;
  it._pos_m = 0;
  return it;
}

std::string bitset::to_string() const {
  std::string result;
  result.reserve(capacity() + (capacity() / 8) + 1); // reserve space for the string
  for (size_t i = 0; i < capacity(); ++i) {
    if (_bits[i / (BITS*BITS)] >> ((i / BITS) % BITS) == 0) {
      result += " ..."; // add space for readability
      break;
    }
    result += get(i) ? '1' : '0';
    if (i % 8 == 7 && i < capacity() - 1) {
      result += ' ';
    }
  }
  return result;
}
