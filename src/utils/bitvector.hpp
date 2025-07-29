/*
 * This file is part of the source code of the software program
 * NapSAT. It is protected by applicable copyright laws.
 *
 * This source code is protected by the terms of the MIT License.
 */
/**
 * @file src/utils/bitset.hpp
 * @author Robin Coutelier
 *
 * @brief This file defines the bitset data structure used to store bit information in a sparse manner.
 */

#pragma once

#include <vector>
#include <string>

namespace napsat {
  class bitset {
    typedef unsigned long Tblock; // type of a block of bits, can be changed to a larger type if needed
    public:
      bitset() = default;
      bitset(size_t size);

      void set(size_t index, bool value);
      bool get(size_t index) const;

      inline bool operator[](size_t index) const {
        return get(index);
      }


      size_t size() const;

      void clear();
      bool empty() const;
      void resize(size_t new_size);
      /**
       * @brief Counts the number of bits set to true in the bitset.
       * @return The number of bits set to true.
       */
      unsigned count() const;

      // bitwise operations
      bitset operator&(const bitset& other) const;
      bitset operator|(const bitset& other) const;
      bitset operator^(const bitset& other) const;
      bitset operator~() const;
      /**
       * @brief Subtracts the bits of another bitset from this one.
       * @details b1 - b2 is equivalent to b1 ^ (b1 & b2).
       */
      bitset operator-(const bitset& other) const;

      void operator&=(const bitset& other);
      void operator|=(const bitset& other);
      void operator^=(const bitset& other);
      void negate();

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

      void start_enumeration();
      int next_non_zero();

      double sparsity(){
        if (_size == 0)
          return 0.0;
        unsigned used_size = _size / BLOCK_SIZE;
        if (_size < BLOCK_SIZE) {
          used_size = 1; // at least one block is used
        }
        return (double) _bits.size() / used_size;
      }

      // printing
      std::string to_string() const;

    private:
      std::vector<std::pair<size_t, Tblock>> _bits; // pairs of (index, value)
      size_t _size = 0; // number oaf bits in the bitset
      unsigned next_non_zero_bit = -1;
      unsigned current_block = 0;
      static const size_t BLOCK_SIZE = 8 * sizeof(Tblock); // total size of the bitset

      static inline std::pair<size_t, size_t> find(size_t index) {
        size_t block_index = index / BLOCK_SIZE;
        size_t bit_index = index % BLOCK_SIZE;
        return {block_index, bit_index};
      }

      size_t find_block(size_t block_index) const;

  };
}
