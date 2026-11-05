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

static inline constexpr unsigned count_zero_l(uint64_t b) {
#if __cplusplus >= 202002L
  return std::countl_zero(b);
#else
  // not defined for 0
  return b ? __builtin_clzll(b) : sizeof(uint64_t) * CHAR_BIT;
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
  _bits.resize(it - _bits.begin());
}

bool bitset::empty() const {
  auto it = _bits.cbegin();
  assert(it != _bits.cend());
  do {
    if (*it & M_DATA_MSK) return false;
  } while (*(it++) & M_NEXT_MSK);
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
    const uint64_t v1 = *m1 & M_DATA_MSK;
    const uint64_t v2 = *m2 & M_DATA_MSK;
    assert(!(v1 & ~v2)); // checked above
    // Walk the data words of b (a only has words where b has one), and stop as soon as
    // there is no word of a left in this metadata block.
    uint64_t u = v2;
    for (; u & v1; u &= u - 1) {
      if (v1 & u & -u) {
        if (!subset(*it1, *it2)) return false;
        equal = equal && (*it1 == *it2);
        ++it1;
      }
      ++it2;
    }
    // skip the remaining words of b, that a does not have
    assert(!u || !equal);
    if (u) it2 += count_bits(u);
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

/*
 * Word-wise operations. Besides the operation itself, each one describes
 * - relevant(v1, v2): the data positions (as metadata bits) where the result can be non-zero,
 * - grows:   whether op(0, b) can be non-zero, i.e. the result can have words a does not have,
 * - shrinks: whether op(a, b) can be zero for a != 0, i.e. words of a can disappear.
 * Taking them as template parameters lets the compiler inline the operation and drop the
 * code paths that cannot happen for it.
 */
struct op_and {
  static constexpr bool grows = false, shrinks = true;
  static uint64_t apply(uint64_t a, uint64_t b) { return a & b; }
  static uint64_t relevant(uint64_t v1, uint64_t v2) { return v1 & v2; }
};

struct op_or {
  static constexpr bool grows = true, shrinks = false;
  static uint64_t apply(uint64_t a, uint64_t b) { return a | b; }
  static uint64_t relevant(uint64_t v1, uint64_t v2) { return v1 | v2; }
};

struct op_xor {
  static constexpr bool grows = true, shrinks = true;
  static uint64_t apply(uint64_t a, uint64_t b) { return a ^ b; }
  static uint64_t relevant(uint64_t v1, uint64_t v2) { return v1 | v2; }
};

struct op_diff {
  static constexpr bool grows = false, shrinks = true;
  static uint64_t apply(uint64_t a, uint64_t b) { return a & ~b; }
  static uint64_t relevant(uint64_t v1, uint64_t /*v2*/) { return v1; }
};

/** Number of metadata words of a bitset. */
static inline size_t metadata_size(const bitset::bitstore &a) {
  auto it = a.cbegin();
  while (*(it++) & M_NEXT_MSK);
  return it - a.cbegin();
}

template <class Op>
static bitset::bitstore combine(const bitset::bitstore &a, const bitset::bitstore &b) {
  // count the metadata words, and an upper bound on the number of data words of the result
  const size_t n_meta = metadata_size(a);
  assert(n_meta == metadata_size(b));
  size_t bound = 0;
  for (size_t mi = 0; mi < n_meta; mi++)
    bound += count_bits(Op::relevant(a[mi] & M_DATA_MSK, b[mi] & M_DATA_MSK));

  bitset::bitstore out(n_meta);
  out.reserve(n_meta + bound);

  auto it1 = a.cbegin() + n_meta;
  auto it2 = b.cbegin() + n_meta;
  for (size_t mi = 0; mi < n_meta; mi++) {
    const uint64_t v1 = a[mi] & M_DATA_MSK;
    const uint64_t v2 = b[mi] & M_DATA_MSK;
    const uint64_t rel = Op::relevant(v1, v2);
    uint64_t o = 0;
    // visit the positions where either bitset has a data word, as long as a relevant one remains
    uint64_t u = v1 | v2;
    for (; u & rel; u &= u - 1) {
      const uint64_t low = u & -u; // lowest set bit of u
      const uint64_t a1 = (v1 & low) ? *(it1++) : 0;
      const uint64_t a2 = (v2 & low) ? *(it2++) : 0;
      const uint64_t v = Op::apply(a1, a2);
      if (v) {
        o |= low;
        out.push_back(v);
      }
    }
    // skip the words that cannot contribute to the result
    if (u && mi + 1 < n_meta) {
      it1 += count_bits(v1 & u);
      it2 += count_bits(v2 & u);
    }
    out[mi] = o | (a[mi] & M_NEXT_MSK);
  }
  assert(out.size() <= n_meta + bound);
  return out;
}

bitset bitset::operator&(const bitset &other) const {
  assert(capacity() == other.capacity());
  return bitset(combine<op_and>(_bits, other._bits));
}

bitset bitset::operator|(const bitset &other) const {
  assert(capacity() == other.capacity());
  return bitset(combine<op_or>(_bits, other._bits));
}

bitset bitset::operator^(const bitset &other) const {
  assert(capacity() == other.capacity());
  return bitset(combine<op_xor>(_bits, other._bits));
}

bitset bitset::operator-(const bitset &other) const {
  assert(capacity() == other.capacity());
  return bitset(combine<op_diff>(_bits, other._bits));
}

template <class Op>
static void update(bitset::bitstore &a, const bitset::bitstore &b) {
  // count the metadata words, and the number of data words of b that a does not have
  const size_t n_meta = metadata_size(a);
  assert(n_meta == metadata_size(b));
  size_t n_new = 0;
  if (Op::grows)
    for (size_t mi = 0; mi < n_meta; mi++)
      n_new += count_bits(b[mi] & ~a[mi] & M_DATA_MSK);

  if (Op::grows && n_new > 0) {
    // The result can have more data words than a. Grow a once, then merge from the back so
    // that no word of a is overwritten before being read: the write position never goes below
    // the read position. Words that become zero are dropped, leaving a gap removed at the end.
    const size_t old_size = a.size();
    a.resize(old_size + n_new);
    size_t r1 = old_size;
    size_t r2 = b.size();
    size_t w = a.size();
    for (size_t mi = n_meta; mi-- > 0;) {
      const uint64_t v1 = a[mi] & M_DATA_MSK;
      const uint64_t v2 = b[mi] & M_DATA_MSK;
      uint64_t o = 0;
      for (uint64_t u = v1 | v2; u;) {
        const uint64_t high = MSK_BT(BITS - 1 - count_zero_l(u)); // highest set bit of u
        u ^= high;
        const uint64_t a1 = (v1 & high) ? a[--r1] : 0;
        const uint64_t a2 = (v2 & high) ? b[--r2] : 0;
        const uint64_t v = Op::apply(a1, a2);
        if (!Op::shrinks || v) {
          assert(w > r1); // never overwrite a word of a that has not been read yet
          a[--w] = v;
          o |= high;
        }
      }
      a[mi] = o | (a[mi] & M_NEXT_MSK);
    }
    assert(r1 == n_meta && r2 == n_meta && w >= n_meta);
    if (Op::shrinks && w > n_meta)
      a.erase(a.begin() + n_meta, a.begin() + w);
    assert(Op::shrinks || w == n_meta);
    return;
  }

  // The result has no word that a does not have: compute it in place, front to back.
  // The write position never goes past the read position.
  size_t r1 = n_meta;
  size_t r2 = n_meta;
  size_t w = n_meta;
  for (size_t mi = 0; mi < n_meta; mi++) {
    const uint64_t v1 = a[mi] & M_DATA_MSK;
    const uint64_t v2 = b[mi] & M_DATA_MSK;
    const uint64_t rel = Op::relevant(v1, v2);
    uint64_t o = 0;
    uint64_t u = v1 | v2;
    for (; u & rel; u &= u - 1) {
      const uint64_t low = u & -u; // lowest set bit of u
      const uint64_t a1 = (v1 & low) ? a[r1++] : 0;
      const uint64_t a2 = (v2 & low) ? b[r2++] : 0;
      const uint64_t v = Op::apply(a1, a2);
      assert((v1 & low) || !v);
      if (!Op::shrinks || v) {
        a[w++] = v;
        o |= low;
      }
    }
    // the remaining words of a cannot be kept (op(a1, 0) == 0 when they are not relevant)
    if (u && mi + 1 < n_meta) {
      r1 += count_bits(v1 & u);
      r2 += count_bits(v2 & u);
    }
    a[mi] = o | (a[mi] & M_NEXT_MSK);
  }
  if (Op::shrinks)
    a.resize(w);
  assert(a.size() == w);
}

void bitset::operator&=(const bitset &other) {
  assert(capacity() == other.capacity());
  update<op_and>(_bits, other._bits);
}

void bitset::operator|=(const bitset &other) {
  assert(capacity() == other.capacity());
  update<op_or>(_bits, other._bits);
}

void bitset::operator^=(const bitset &other) {
  assert(capacity() == other.capacity());
  update<op_xor>(_bits, other._bits);
}

void bitset::operator-=(const bitset &other) {
  assert(capacity() == other.capacity());
  update<op_diff>(_bits, other._bits);
}

template <class Op>
static bool compare(const bitset::bitstore &a, const bitset::bitstore &b) {
  auto it1 = a.cbegin();
  auto it2 = b.cbegin();
  while (*(it1++) & *(it2++) & M_NEXT_MSK);

  // check data
  auto m1 = a.cbegin();
  auto m2 = b.cbegin();
  do {
    const uint64_t v1 = *m1 & M_DATA_MSK;
    const uint64_t v2 = *m2 & M_DATA_MSK;
    const uint64_t rel = Op::relevant(v1, v2);
    // visit the positions where either bitset has a data word, as long as a relevant one remains
    uint64_t u = v1 | v2;
    for (; u & rel; u &= u - 1) {
      const uint64_t low = u & -u; // lowest set bit of u
      const uint64_t a1 = (v1 & low) ? *(it1++) : 0;
      const uint64_t a2 = (v2 & low) ? *(it2++) : 0;
      if (Op::apply(a1, a2)) return true;
    }
    // no more metadata block: nothing left to check
    if (!(*m1 & *m2 & M_NEXT_MSK)) break;
    // skip the words that cannot contribute to the result
    if (u) {
      it1 += count_bits(v1 & u);
      it2 += count_bits(v2 & u);
    }
    ++m1; ++m2;
  } while (true);
  assert((*m1 & M_NEXT_MSK) == (*m2 & M_NEXT_MSK));
  return false;
}

/** returns true if (this & other) != 0 */
bool bitset::has_intersection(const bitset &other) const {
  assert(capacity() == other.capacity());
  return compare<op_and>(_bits, other._bits);
}

bool bitset::has_difference(const bitset &other) const {
  assert(capacity() == other.capacity());
  return compare<op_diff>(_bits, other._bits);
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
  ++_it_d;
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
    ++_it_m;
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

bool bitset::iterator::operator==(const bitset::iterator &other) const {
  assert(std::addressof(other._b) == std::addressof(_b));
  return _it_d == other._it_d && _pos_d == other._pos_d;
}

bool bitset::iterator::operator!=(const bitset::iterator &other) const {
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
  std::string result = "[";
  auto i = cbegin();
  unsigned lim = 1000;
  while (i != cend()) {
    result += std::to_string(*i);
    ++i;
    if (i != cend()) {
      result += ", ";
    }
    if (--lim == 0) {
      result += "... ";
      unsigned left = count() - 20;
      result += std::to_string(left) + " more";
      break;
    }
  }
  result += "]";
  return result;
}

#include <bitset>

std::string bitset::bit_string() const {
  std::string result;
  result.reserve(_bits.size() * 65);
  for (uint64_t block : _bits) {
    result += std::bitset<64>(block).to_string() + "\n";
  }
  return result;
}
