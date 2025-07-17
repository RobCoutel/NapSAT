/*
 * This file is part of the source code of the software program
 * NapSAT. It is protected by applicable copyright laws.
 *
 * This source code is protected by the terms of the MIT License.
 */
/**
 * @file src/utils/bitvector.hpp
 * @author Robin Coutelier
 *
 * @brief This file defines the bitvector data structure used to store bit information in a sparse manner.
 */

#pragma once

#include <vector>
#include <string>

namespace napsat {
  class bitvector {
    public:
      bitvector() = default;
      bitvector(size_t size);

      void set(size_t index, bool value);
      bool get(size_t index) const;

      inline bool operator[](size_t index) const {
        return get(index);
      }


      size_t size() const;

      void clear();
      bool empty() const;
      void resize(size_t new_size);

      // bitwise operations
      bitvector operator&(const bitvector& other) const;
      bitvector operator|(const bitvector& other) const;
      bitvector operator^(const bitvector& other) const;
      bitvector operator~() const;
      /**
       * @brief Subtracts the bits of another bitvector from this one.
       * @details b1 - b2 is equivalent to b1 ^ (b1 & b2).
       */
      bitvector operator-(const bitvector& other) const;

      void operator&=(const bitvector& other);
      void operator|=(const bitvector& other);
      void operator^=(const bitvector& other);
      void negate();

      // comparison
      bool operator==(const bitvector& other) const;
      bool operator!=(const bitvector& other) const;
      /**
       * @brief Checks if this bitvector is a subset of another bitvector.
       */
      bool operator<=(const bitvector& other) const;

      bool operator>=(const bitvector& other) const;

      bool operator<(const bitvector& other) const;
      bool operator>(const bitvector& other) const;

      // printing
      std::string to_string() const;

    private:
      std::vector<std::pair<size_t, long unsigned>> _bits; // pairs of (index, value)
      size_t _size = 0; // number of bits in the bitvector
      static const size_t BLOCK_SIZE = 8 * sizeof(long unsigned); // total size of the bitvector

      static inline std::pair<size_t, size_t> find(size_t index) {
        size_t block_index = index / BLOCK_SIZE;
        size_t bit_index = index % BLOCK_SIZE;
        return {block_index, bit_index};
      }

      size_t find_block(size_t block_index) const;

  };
}
