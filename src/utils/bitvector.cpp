#include "bitvector.hpp"

#include <cassert>
#include <vector>
#include <iostream>

using namespace std;

napsat::bitvector::bitvector(size_t size) : _size(size) { }

void napsat::bitvector::set(size_t index, bool value) {
  auto [block_index, bit_index] = find(index);
  size_t block_number = find_block(block_index);
  if (block_index >= _bits.size() || _bits[block_number].first != block_index) {
    // push a new block at the right position
    _bits.insert(_bits.begin() + block_number, {block_index, 0});
  }
  long unsigned& block_value = _bits[block_number].second;
  if (value)
    block_value |= (1UL << bit_index);
  else
    block_value &= ~(1UL << bit_index);
  if (block_value == 0) {
    // remove the block if it is empty
    _bits.erase(_bits.begin() + block_number);
  }
}

size_t napsat::bitvector::find_block(size_t block_index) const
{
  // binary search for the block index
  size_t left = 0;
  size_t right = _bits.size();
  while (left < right && _bits[left].first != block_index) {
    size_t mid = left + (right - left) / 2;
    if (_bits[mid].first < block_index) {
      left = mid + 1;
    } else {
      right = mid;
    }
  }
  return left;
}

bool napsat::bitvector::get(size_t index) const {
  auto [block_index, bit_index] = find(index);
  size_t block_number = find_block(block_index);
  if (block_number >= _bits.size() || _bits[block_number].first != block_index) {
    return false; // bit is not set
  }
  return (_bits[block_number].second & (1UL << bit_index));
}

size_t napsat::bitvector::size() const {
  return _size;
}

void napsat::bitvector::clear() {
  _bits.clear();
}

bool napsat::bitvector::empty() const {
  return _bits.empty();
}

void napsat::bitvector::resize(size_t new_size) {
  assert (new_size >= _size);
  _size = new_size;
}

napsat::bitvector napsat::bitvector::operator&(const bitvector& other) const {
  assert(_size == other._size);
  bitvector result(_size);
  size_t i = 0, j = 0;
  while (i < _bits.size() && j < other._bits.size()) {
    if (_bits[i].first < other._bits[j].first) {
      i++;
      continue;
    }
    if (_bits[i].first > other._bits[j].first) {
      j++;
      continue;
    }
    assert(_bits[i].first == other._bits[j].first);
    long unsigned common_bits = _bits[i].second & other._bits[j].second;
    if (common_bits != 0)
      result._bits.push_back({_bits[i].first, common_bits});
    i++;
    j++;
  }
  return result;
}

napsat::bitvector napsat::bitvector::operator|(const bitvector& other) const {
  assert(_size == other._size);
  bitvector result(_size);
  size_t i = 0, j = 0;
  while (i < _bits.size() && j < other._bits.size()) {
    if (_bits[i].first < other._bits[j].first) {
      result._bits.push_back(_bits[i]);
      i++;
      continue;
    }
    if (_bits[i].first > other._bits[j].first) {
      result._bits.push_back(other._bits[j]);
      j++;
      continue;
    }
    assert(_bits[i].first == other._bits[j].first);
    result._bits.push_back({_bits[i].first, _bits[i].second | other._bits[j].second});
    i++;
    j++;
  }
  while (i < _bits.size()) {
    result._bits.push_back(_bits[i++]);
  }
  while (j < other._bits.size()) {
    result._bits.push_back(other._bits[j++]);
  }
  return result;
}

napsat::bitvector napsat::bitvector::operator^(const bitvector& other) const {
  assert(_size == other._size);
  bitvector result(_size);
  size_t i = 0, j = 0;
  while (i < _bits.size() && j < other._bits.size()) {
    if (_bits[i].first < other._bits[j].first) {
      result._bits.push_back(_bits[i]);
      i++;
      continue;
    }
    if (_bits[i].first > other._bits[j].first) {
      result._bits.push_back(other._bits[j]);
      j++;
      continue;
    }
    assert(_bits[i].first == other._bits[j].first);
    // XOR the bits
    long unsigned xor_bits = _bits[i].second ^ other._bits[j].second;
    if (xor_bits != 0)
      result._bits.push_back({_bits[i].first, xor_bits});
    i++;
    j++;
  }
  while (i < _bits.size()) {
    result._bits.push_back(_bits[i++]);
  }
  while (j < other._bits.size()) {
    result._bits.push_back(other._bits[j++]);
  }
  return result;
}

napsat::bitvector napsat::bitvector::operator~() const {
  bitvector result(_size);
  size_t i = 0, j = 0;
  while (j < _bits.size()) {
    if (j < _bits.size() && _bits[j].first == i) {
      result._bits.push_back({_bits[j].first, ~_bits[j].second});
      j++;
    } else {
      result._bits.push_back({i, ~0UL}); // all bits set
    }
  }
  return result;
}

napsat::bitvector napsat::bitvector::operator-(const bitvector& other) const
{
  return *this ^ (other & *this);
}

void napsat::bitvector::operator&=(const bitvector& other) {
  assert(_size == other._size);
  size_t i = 0, j = 0;
  while (i < _bits.size() && j < other._bits.size()) {
    if (_bits[i].first < other._bits[j].first) {
      i++;
      continue;
    }
    if (_bits[i].first > other._bits[j].first) {
      j++;
      continue;
    }
    assert(_bits[i].first == other._bits[j].first);
    // AND the bits
    _bits[i].second &= other._bits[j].second;
    if (_bits[i].second == 0) {
      // remove the block if it is empty
      _bits.erase(_bits.begin() + i);
      // do not increment i, as we need to check the next block at the same index
      continue;
    }
    i++;
    j++;
  }
}

void napsat::bitvector::operator|=(const bitvector& other) {
  assert(_size == other._size);
  size_t i = 0, j = 0;
  while (i < _bits.size() && j < other._bits.size()) {
    if (_bits[i].first < other._bits[j].first) {
      i++;
      continue;
    }
    if (_bits[i].first > other._bits[j].first) {
      _bits.insert(_bits.begin() + i, other._bits[j]);
      j++;
      continue;
    }
    assert(_bits[i].first == other._bits[j].first);
    _bits[i].second |= other._bits[j].second;
    i++;
    j++;
  }
  while (j < other._bits.size()) {
    _bits.push_back(other._bits[j++]);
  }

}

void napsat::bitvector::operator^=(const bitvector& other) {
  assert(_size == other._size);
  size_t i = 0, j = 0;
  while (i < _bits.size() && j < other._bits.size()) {
    if (_bits[i].first < other._bits[j].first) {
      i++;
      continue;
    }
    if (_bits[i].first > other._bits[j].first) {
      _bits.insert(_bits.begin() + i, {other._bits[j].first, other._bits[j].second});
      j++;
      continue;
    }
    assert(_bits[i].first == other._bits[j].first);
    _bits[i].second ^= other._bits[j].second;
    if (_bits[i].second == 0) {
      // remove the block if it is empty
      _bits.erase(_bits.begin() + i);
      // do not increment i, as we need to check the next block at the same index
      continue;
    }
    i++;
    j++;
  }
}

void napsat::bitvector::negate() {
  size_t j = 0;
  for (size_t i = 0; i < (_size + BLOCK_SIZE - 1) / BLOCK_SIZE; ++i) {
    if (j < _bits.size() && _bits[j].first == i) {
      _bits[j].second = ~_bits[j].second;
      j++;
    } else {
      _bits.insert(_bits.begin() + j, {i, ~0UL}); // all bits set
      j++;
    }
  }
}

bool napsat::bitvector::operator==(const bitvector& other) const {
  if (_size != other._size || _bits.size() != other._bits.size()) {
    return false;
  }
  for (size_t i = 0; i < _bits.size(); ++i) {
    if (_bits[i] != other._bits[i]) {
      return false;
    }
  }
  return true;
}

bool napsat::bitvector::operator!=(const bitvector& other) const {
  return !(*this == other);
}

bool napsat::bitvector::operator<=(const bitvector& other) const {
  return (*this | other) == other; // this is a subset of other if union is equal to other
}

bool napsat::bitvector::operator>=(const bitvector& other) const {
  return (*this | other) == *this; // this is a superset of other if union is equal to this
}

bool napsat::bitvector::operator<(const bitvector& other) const
{
  return (*this | other) == other && *this != other; // this is a proper subset of other if union is equal to other and they are not equal
}

bool napsat::bitvector::operator>(const bitvector& other) const
{
  return (*this | other) == *this && *this != other; // this is a proper superset of other if union is equal to this and they are not equal
}

std::string napsat::bitvector::to_string() const {
  std::string result;
  for (size_t i = 0; i < _size; ++i) {
    result += get(i) ? '1' : '0';
    if (i % BLOCK_SIZE == BLOCK_SIZE - 1) {
      result += '\n'; // new line every BLOCK_SIZE bits
    } else if (i % 8 == 7) {
      result += ' '; // add space every 8 bits for readability
    }
  }
  return result;
}
