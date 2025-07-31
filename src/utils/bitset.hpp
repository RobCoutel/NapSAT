#ifndef BITSET_HPP
#define BITSET_HPP

#include <vector>
#include <cstdint>
#include <string>

class bitset {
public:
  bitset() noexcept;
  explicit bitset(size_t size) noexcept;

  void set(size_t index, bool value);
  bool get(size_t index) const;

  inline bool operator[](size_t index) const {
    return get(index);
  }

  /** Clears the bitset. */
  void clear();

  /** Checks if the bitset is empty. */
  bool empty() const;

  /** Gets the current capacity of the bitset. */
  size_t capacity() const;

  /** Resizes the bitset to accomondate at least size values. */
  void resize(size_t size);

  /** Counts the number of bits set to true in the bitset. */
  size_t count() const;

  // bitwise operations
  bitset operator&(const bitset& other) const;
  bitset operator|(const bitset& other) const;
  bitset operator^(const bitset& other) const;
  // not yet implemented (too expensive)
  //bitset operator~() const;

  /**
   * @brief Subtracts the bits of another bitset from this one.
   * @details b1 - b2 is equivalent to b1 ^ (b1 & b2).
   */
  bitset operator-(const bitset& other) const;

  void operator&=(const bitset& other);
  void operator|=(const bitset& other);
  void operator^=(const bitset& other);

  // comparison
  bool operator==(const bitset& other) const;
  bool operator!=(const bitset& other) const;

  /**
   * @brief Checks if this bitset is a subset of another bitset.
   */
  bool operator<=(const bitset& other) const;
  bool operator>=(const bitset& other) const;
  bool operator<(const bitset& other) const;
  bool operator>(const bitset& other) const;

  std::string to_string() const;
  using bitstore = std::vector<uint64_t>;

  struct iterator {
    using iterator_category = std::forward_iterator_tag;

    unsigned operator*() const;
    iterator& operator++();
    iterator operator++(int);
    bool operator==(const iterator &other);
    bool operator!=(const iterator &other);

  private:
    friend bitset;

    explicit iterator(const bitset &bs);
    static bitset::iterator begin(const bitset &bs);
    static bitset::iterator end(const bitset &bs);
    void next();

    const bitset &_b;
    bitstore::const_iterator _it_m;
    bitstore::const_iterator _it_d;
    size_t _pos_m{}; // position inside the metadata word
    size_t _pos_d{}; // position inside the data word
  };

  iterator cbegin() const { return bitset::iterator::begin(*this); };
  iterator cend() const { return bitset::iterator::end(*this); };

private:
  explicit bitset(bitstore &&v) noexcept;
  bitstore _bits;
};

#endif //BITSET_HPP
