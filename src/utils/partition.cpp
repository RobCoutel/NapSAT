/*
 * This file is part of the source code of the software program
 * NapSAT. It is protected by applicable copyright laws.
 *
 * This source code is protected by the terms of the MIT License.
 */
/**
 * @file src/utils/partition.hpp
 * @author Robin Coutelier
 *
 * @brief This file is part of the NapSAT solver. It contains the implementation
 * of the AVL tree and the memory partition system for memory allocation.
 */
#include "partition.hpp"

#include <vector>
#include <cmath>
#include <cassert>
#include <iostream>

using namespace std;


/******************************************************************************/
/*                                  AVL TREE                                  */
/******************************************************************************/
/**
 * The implementation of the AVL tree is based on the following resources:
 * https://www.geeksforgeeks.org/what-is-avl-tree-avl-tree-meaning
 *
 * However, the implementation here is slightly more complicated because we avoid recursion.
 */

/******************************************************************************/
/*                                  AVL NODE                                  */
/******************************************************************************/

bool utils::AVLTree::AVLNode::is_balanced()
{
  int balance = balance_factor();
  if (balance > 1 || balance < -1)
    return false;
  if (_left && !_left->is_balanced())
    return false;
  if (_right && !_right->is_balanced())
    return false;
  return true;
}

bool utils::AVLTree::AVLNode::is_bst()
{
  if (_left) {
    if (_left->_key > _key || (_left->_key == _key && _left->_data > _data))
      return false;
    if (!_left->is_bst())
      return false;
  }
  if (_right) {
    if (_right->_key < _key || (_right->_key == _key && _right->_data < _data))
      return false;
    if (!_right->is_bst())
      return false;
  }
  return true;
}

void utils::AVLTree::AVLNode::print(unsigned depth)
{
  if (_left)
    _left->print(depth + 1);
  for (unsigned i = 0; i < depth; i++)
    std::cout << " -- ";
  std::cout << _key << " (" << _height << ")" << endl;
  if (_right) {
    _right->print(depth + 1);
  }
}

/******************************************************************************/
/*                                  AVL TREE                                  */
/******************************************************************************/

utils::AVLTree::AVLNode* utils::AVLTree::rotate_left(AVLNode* b)
{
  /**
   *      b                   d
   *    /   \               /   \
   *   a     d      =>     b     e
   *        / \           / \
   *       c   e         a   c
   */
  AVLNode* d = b->_right;
  assert(d);
  AVLNode* c = d->_left;
  d->_left = b;
  b->_right = c;

  b->update_height();
  d->update_height();
  return d;
}

utils::AVLTree::AVLNode* utils::AVLTree::rotate_right(AVLNode* d)
{
  /**
   *      d                   b
   *    /   \               /   \
   *   b     e      =>     a     d
   *  / \                       / \
   * a   c                     c   e
   */
  AVLNode* b = d->_left;
  assert(b);
  AVLNode* c = b->_right;
  b->_right = d;
  d->_left = c;

  d->update_height();
  b->update_height();
  return b;
}

bool utils::AVLTree::insert(unsigned key, void* data)
{
  _path.clear();
  if (!_root) {
    _root = new AVLNode(key, data);
    return true;
  }
  AVLNode* node = _root;
  // Go down the tree to find the right location
  while (true) {
    _path.push_back(node);
    assert(node);
    assert(node->balance_factor() <= 1 && node->balance_factor() >= -1);
    if (key == node->_key && data == node->_data)
      return false;
    // search left
    if (key < node->_key || (key == node->_key && data < node->_data)) {
      if (!node->_left) {
        node->_left = new AVLNode(key, data);
        break;
      }
      else
        node = node->_left;
    }
    // search right
    else {
      if (!node->_right) {
        node->_right = new AVLNode(key, data);
        break;
      }
      else
        node = node->_right;
    }
  }

  // Rebalance the tree
  while (!_path.empty()) {
    node = _path.back();
    _path.pop_back();
    AVLNode* parent = _path.empty() ? nullptr : _path.back();
    assert(!parent || node == parent->_left || node == parent->_right);

    unsigned old_height = node->_height;
    node->update_height();
    int balance = node->balance_factor();
    if (old_height == node->_height && balance >= -1 && balance <= 1)
      break;


    AVLNode* node_after_rotation = nullptr;
    // Left heavy
    if (balance > 1) {
      // Left left case
      if (key < node->_left->_key || (key == node->_left->_key && data < node->_left->_data))
        node_after_rotation = rotate_right(node);
      // Left right case
      else {
        node->_left = rotate_left(node->_left);
        node_after_rotation = rotate_right(node);
      }
    }
    // Right heavy
    else if (balance < -1) {
      // Right right case
      if (key > node->_right->_key || (key == node->_right->_key && data > node->_right->_data))
        node_after_rotation = rotate_left(node);
      // Right left case
      else {
        node->_right = rotate_right(node->_right);
        node_after_rotation = rotate_left(node);
      }
    }
    else
      continue;
    if (!parent)
      _root = node_after_rotation;
    else if (parent->_left == node)
      parent->_left = node_after_rotation;
    else if (parent->_right == node)
      parent->_right = node_after_rotation;
    else
      assert(false);
  }
  assert (is_balanced());
  assert (is_bst());

  return true;
}

bool utils::AVLTree::remove(unsigned key, void* data)
{
  _path.clear();
  AVLNode* node = _root;
  // Go down the tree to find the right location
  while (node) {
    _path.push_back(node);
    assert(node->balance_factor() <= 1 && node->balance_factor() >= -1);
    if (key == node->_key && data == node->_data)
      break;
    if (key < node->_key || (key == node->_key && data < node->_data))
      node = node->_left;
    else if (key > node->_key || (key == node->_key && data > node->_data))
      node = node->_right;
  }
  if (!node)
    return false;
  assert(node->_key == key && node->_data == data);

  assert(_path.size() >= 1);
  AVLNode* parent = _path.size() == 1 ? nullptr : _path[_path.size() - 2];
  // Case 1: Node has no children
  if (!node->_left && !node->_right) {
    if (!parent)
      _root = nullptr;
    else if (parent->_left == node)
      parent->_left = nullptr;
    else
      parent->_right = nullptr;
    // remove the children so that they are not deleted
    node->_left = nullptr;
    node->_right = nullptr;
    delete node;
  }
  // Case 2: Node has one child
  else if (!node->_left || !node->_right) {
    AVLNode* child = !node->_left ? node->_right : node->_left;
    if (!parent)
      _root = child;
    else if (parent->_left == node)
      parent->_left = child;
    else
      parent->_right = child;
    // remove the children so that they are not deleted
    node->_left = nullptr;
    node->_right = nullptr;
    delete node;
  }
  // Case 3: Node has two children
  else {
    // successor is the smallest element that is greater than node
    AVLNode* successor = node->_right;
    while(true) {
      _path.push_back(successor);
      if (!successor->_left)
        break;
      successor = successor->_left;
    }
    // Exchange the data of the node and the successor
    node->_key = successor->_key;
    node->_data = successor->_data;

    node = successor;
    assert(node);
    // Delete the successor since it is now in the position of the node
    assert(successor == _path.back());
    parent = _path[_path.size() - 2];
    assert(parent);
    assert(parent->_left == node || parent->_right == node);
    if (parent->_left == node)
      parent->_left = node->_right;
    else
      parent->_right = node->_right;
    // remove the children so that they are not deleted
    node->_left = nullptr;
    node->_right = nullptr;
    delete node;
  }
  _path.pop_back();

  // Rebalance the tree
  while (!_path.empty()) {
    node = _path.back();
    _path.pop_back();
    assert(node);
    AVLNode* parent = _path.empty() ? nullptr : _path.back();
    assert(!parent || node == parent->_left || node == parent->_right);

    unsigned old_height = node->_height;
    node->update_height();
    int balance = node->balance_factor();
    if (old_height == node->_height && balance >= -1 && balance <= 1)
      continue;

    AVLNode* node_after_rotation = nullptr;
    // Left heavy
    if (balance > 1) {
      // Left left case
      if (node->_left->balance_factor() >= 0)
        node_after_rotation = rotate_right(node);
      // Left right case
      else {
        node->_left = rotate_left(node->_left);
        node_after_rotation = rotate_right(node);
      }
    }
    // Right heavy
    else if (balance < -1) {
      // Right right case
      if (node->_right->balance_factor() <= 0)
        node_after_rotation = rotate_left(node);
      // Right left case
      else {
        node->_right = rotate_right(node->_right);
        node_after_rotation = rotate_left(node);
      }
    }
    else
      continue;

    // Update the parent
    if (!parent)
      _root = node_after_rotation;
    else if (parent->_left == node)
      parent->_left = node_after_rotation;
    else if (parent->_right == node)
      parent->_right = node_after_rotation;
    else
      assert(false);
  }

  assert (is_balanced());
  assert (is_bst());

  return true;
}

void* utils::AVLTree::find_best_fit(unsigned key)
{
  AVLNode* best_candidate = nullptr;
  AVLNode* node = _root;
  while (node) {
    if (key == node->_key)
      return node->_data;
    if (key < node->_key) {
      best_candidate = node;
      node = node->_left;
    }
    else
      node = node->_right;
  }
  if (!best_candidate)
    return nullptr;
  return best_candidate->_data;
}

void utils::AVLTree::print()
{
  if (!_root) {
    std::cout << "Empty tree" << endl;
    return;
  }
  _root->print(0);
}

bool utils::AVLTree::is_balanced()
{
  if (!_root)
    return true;
  return _root->is_balanced();
}

bool utils::AVLTree::is_bst()
{
  if (!_root)
    return true;
  return _root->is_bst();
}

/******************************************************************************/
/*                              MEMORY PARTITION                              */
/******************************************************************************/

/******************************************************************************/
/*                                MEMORY NODE                                 */
/******************************************************************************/

// /**
//  * @brief Check if a number is a power of two.
//  * @param x the number to check.
//  * @return true if x is a power of two, false otherwise.
//  */
// static bool is_power_of_two(size_t x)
// {
//   return (x & (x - 1)) == 0;
// }

// /**
//  * @brief Find the largest power of two that is less than or equal to x.
//  * @param x the number to find the largest power of two for.
//  * @return the largest power of two that is less than or equal to x.
//  */
// static size_t previous_power_of_two(size_t x)
// {
//   size_t p = 1;
//   while (p < x)
//     p <<= 1;
//   return p >> 1;
// }

// /**
//  * @brief Find the smallest power of two that is greater than or equal to x.
//  */
// static size_t next_power_of_two(size_t x)
// {
//   size_t p = 1;
//   while (p < x)
//     p <<= 1;
//   return p;
// }

// static inline bool is_aligned_with_cache(byte* ptr)
// {
//   return (unsigned long) ptr & !utils::CACHE_MASK == 0;
// }

// static inline bool is_on_same_cache_line(byte* ptr1, byte* ptr2)
// {
//   return (unsigned long) ptr1 & utils::CACHE_MASK == (unsigned long) ptr2 & utils::CACHE_MASK;
// }

// static inline unsigned long number_of_cache_lines(size_t size)
// {
//   return (size + utils::CACHE_LINE_SIZE - 1) / utils::CACHE_LINE_SIZE;
// }

// static inline unsigned long number_of_cache_lines(byte* start, byte* end)
// {
//   return ((unsigned long) start & utils::CACHE_MASK
//         - (unsigned long) end & utils::CACHE_MASK)
//         >> 6;
// }

// inline bool utils::MemoryPartition::MemoryNode::is_leaf() const
// {
//   return !_left && !_right;
// }

// bool utils::MemoryPartition::MemoryNode::is_free() const
// {
//   return _free;
// }

// unsigned utils::MemoryPartition::MemoryNode::balance_factor() const
// {
//   unsigned left_height = _left ? _left->_height : 0;
//   unsigned right_height = _right ? _right->_height : 0;
//   return left_height - right_height;
// }

// void utils::MemoryPartition::MemoryNode::update_height()
// {
//   unsigned left_height = _left ? _left->_height : 0;
//   unsigned right_height = _right ? _right->_height : 0;
//   _height = std::max(left_height, right_height) + 1;
// }

// void utils::MemoryPartition::MemoryNode::split(size_t size)
// {
//   if (size == 0 || size == _size)
//     return;
//   assert(size < _size);
//   assert(_free);
//   assert(!_left && !_right);
//   // A free node either fits entirely in one cache line or is right aligned with a cache line.
//   assert(number_of_cache_lines(_size) > 1 || is_on_same_cache_line(_start, _start + _size - 1));
//   // If the size of the node is a multiple of the cache line size, then the node must be left aligned with the cache lines.
//   assert(_size % utils::CACHE_LINE_SIZE || is_aligned_with_cache(_start));

//   _left = new MemoryNode(_start, size);
//   _right = new MemoryNode(_start + size, _size - size);
//   // if the remaining memory is not aligned with cache lines, split it further
//   // the memory node must either be smaller than a cache line, and fit in one,
//   // or be aligned with the cache lines
//   byte* right_start = _right->_start;

//   /**
//    * We want to restore the invariant
//    * @invariant A free node either fits entirely in one cache line or is right aligned with a
//    */
//   if (_right->_size % utils::CACHE_LINE_SIZE && _right->_size >= utils::CACHE_LINE_SIZE) {
//     // the memory node is not aligned with the cache lines
//     // split the memory node further
//     size_t remaining_size = _right->_size % utils::CACHE_LINE_SIZE;
//     // the end of the memory node must be aligned with the cache lines
//     assert(is_aligned_with_cache(right_start + _right->_size));
//     _right->split(remaining_size);
//   }
//   update_height();
// }
