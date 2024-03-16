/**
 * @file src/solver/heap.cpp
 * @author Robin Coutelier
 *
 * @brief This file is part of the SAT solver NapSAT. If implements a heap data structure for integer keys with double activities.
 */
#include "heap.hpp"

#include <cassert>
#include <iostream>
#include <vector>
#include <cstring>

using namespace sat_utils;

void heap::swap(unsigned i, unsigned j)
{
  assert(i < _heap.size());
  assert(j < _heap.size());
  unsigned key_i = _heap[i];
  unsigned key_j = _heap[j];
  _heap[i] = key_j;
  _heap[j] = key_i;
  _index[key_i] = j;
  _index[key_j] = i;
}

void heap::heapify_down(unsigned i)
{
  assert(i < _heap.size());
  while (left(i) < _heap.size()) {
    unsigned child;
    if (right(i) < _heap.size()
      && _activity[_heap[right(i)]] > _activity[_heap[left(i)]])
      child = right(i);
    else
      child = left(i);
    if (_activity[_heap[child]] <= _activity[_heap[i]])
      break;

    swap(i, child);
    i = child;
  }
}

void heap::heapify_up(unsigned i)
{
  assert(i < _heap.size());
  unsigned p = parent(i);
  while (i && _activity[_heap[i]] > _activity[_heap[p]]) {
    swap(i, p);
    i = parent(i);
    p = parent(i);
  }
}

void heap::insert(unsigned key, double activity)
{
  if (_index.size() <= key) {
    unsigned old_size = _index.size();
    _index.resize(key + 1);
    _activity.resize(key + 1);
    for (unsigned i = old_size; i < _index.size(); i++)
      _index[i] = LOCATION_UNDEF;
  }
  assert(_index[key] == LOCATION_UNDEF);
  _heap.push_back(key);
  _index[key] = _heap.size() - 1;
  _activity[key] = activity;
  heapify_up(_heap.size() - 1);
}

void heap::remove(unsigned key)
{
  assert(key < _index.size());
  assert(_index[key] != LOCATION_UNDEF);
  unsigned i = _index[key];
  swap(i, _heap.size() - 1);
  _heap.pop_back();
  _index[key] = LOCATION_UNDEF;
  if (i < _heap.size())
    heapify_down(i);
}

void sat_utils::heap::update(unsigned key, double activity)
{
  assert(_index[key] != LOCATION_UNDEF);
  _activity[key] = activity;
  heapify_up(_index[key]);
  heapify_down(_index[key]);
}

void sat_utils::heap::normalize(double factor)
{
  for (unsigned i = 0; i < _activity.size(); i++)
    _activity[i] *= factor;
}

bool sat_utils::heap::contains(unsigned key)
{
  return key < _index.size() && _index[key] != LOCATION_UNDEF;
}

void sat_utils::heap::increase_activity(unsigned key, double activity)
{
  assert(_index[key] != LOCATION_UNDEF);
  assert(_activity[key] <= activity);
  _activity[key] = activity;
  heapify_up(_index[key]);
}

unsigned heap::pop()
{
  assert(_heap.size() > 0);
  unsigned key = _heap[0];
  swap(0, _heap.size() - 1);
  _heap.pop_back();
  if (_heap.size() > 0)
    heapify_down(0);
  _index[key] = LOCATION_UNDEF;
  return key;
}

unsigned heap::top()
{
  assert(_heap.size() > 0);
  return _heap[0];
}

unsigned heap::size()
{
  return _heap.size();
}

bool heap::empty()
{
  return _heap.empty();
}

void sat_utils::heap::print()
{
  static std::vector<unsigned> bfs_queue;
  bfs_queue.clear();
  bfs_queue.push_back(0);
  unsigned i = 0;
  while (i < bfs_queue.size()) {
    unsigned j = bfs_queue[i];
    if (left(j) < _heap.size())
      bfs_queue.push_back(left(j));
    if (right(j) < _heap.size())
      bfs_queue.push_back(right(j));
    i++;
  }

  unsigned max_width = 1;
  unsigned max_height = 0;
  for (unsigned i = 0; i < _heap.size(); i = left(i)) {
    max_width = max_width << 1;
    max_height++;
  }
  unsigned height = 0;
  unsigned width = 1;
  unsigned next_level = 1;
  for (unsigned i = 0; i < bfs_queue.size(); i++) {
    for (unsigned k = 0; k < max_width / width / 2; k++)
      std::cout << "      ";
    if (i == next_level) {
      std::cout << std::endl;
      width = width << 1;
      next_level += width;
      height++;
    }
    unsigned j = bfs_queue[i];
    if (j < _heap.size()) {
      std::cout << _heap[j] << " (" << _activity[_heap[j]] << ") ";
    }
    else {
      std::cout << "X ";
    }
  }
}
