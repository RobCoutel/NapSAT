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
  return best_candidate;
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
