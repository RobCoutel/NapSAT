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
 * @brief This file is part of the NapSAT solver. It contains the definition of the AVL tree
 * and the memory partition system for memory allocation.
 */
#pragma once

#include <cassert>
#include <vector>
#include <cstddef>
#include <cmath>

namespace utils
{
  // The length of a cache line in bytes.
  // size_t const CACHE_LINE_SIZE = 64;

  // Mask used to check if an address is aligned with the cache line.
  // or to check if two addresses are on the same cache line.
  // size_t CACHE_MASK = 0xFFFFFFFFFFFFFFC0;

  /**
   * @brief An AVL tree is a self-balancing binary search tree.
   * @details In this implementation, we allow duplicate keys. The second
   * discriminatory factor is the data associated with the key.
   */
  class AVLTree
  {
    friend class MemoryPartition;
    private:
      /********************************************************************************************
       * @brief A node in the AVL tree. It is a singly directed node (no parent pointer).
       * @details The node contains a key, a data and two children.
       ********************************************************************************************/
      typedef struct AVLNode
      {
        /**
         * @brief The key of the node.
         * @details The key is used to order the nodes in the tree. However, the key might not be
         * unique.
         */
        unsigned _key;

        /**
         * @brief The data associated with the key.
         * @details The data is used to discriminate between nodes with the same key.
         */
        void* _data;

        /**
         * @brief The height of the node. The height of a node is the length of the longest path to
         * a leaf.
         * @details The height of a leaf is 1. The height of a node is 1 + the maximum height of its
         * children.
         */
        unsigned _height;

        /**
         * @brief The left child of the node. The left child has a key less than the key of the node.
         */
        AVLNode* _left;

        /**
         * @brief The right child of the node. The right child has a key greater than the key of the
         * node.
         */
        AVLNode* _right;

        /**
         * @brief Constructor of the node.
         * @param key The key of the node.
         * @param data The data associated with the key.
         */
        AVLNode(unsigned key, void* data) :
          _key(key),
          _data(data),
          _height(1),
          _left(nullptr),
          _right(nullptr)
        { /* Do nothing */ }

        /**
         * @brief Destructor of the node. It recursively deletes the children of the node.
         */
        ~AVLNode() {
          if (_left != nullptr)
            delete _left;
          if (_right != nullptr)
            delete _right;
        }

        /**
         * @brief The balance factor of the node. That is, the difference between the height of the
         * left child and the height of the right child.
         * @details A negative balance factor indicates that the right child is heavier than the
         * left child.
         * A positive balance factor indicates that the left child is heavier than the right child.
         */
        int inline balance_factor() const {
          return (int) height(_left) - (int) height(_right);
        }

        /**
         * @brief Update the height of the node.
         */
        inline void update_height() {
          _height = std::max(height(_left), height(_right)) + 1;
        }

        /**
         * @brief The height of the node.
         * @param node The node to compute the height.
         * @return The height of the node. Returns 0 if the node is nullptr.
         * @details The height of a node is 1 + the maximum height of its children.
         */
        static inline unsigned height(AVLNode* node) {
          if (node == nullptr)
            return 0;
          assert(node->_height == std::max(height(node->_left), height(node->_right)) + 1);
          return node->_height;
        }

        /**
         * @brief Check if the node and its children are balanced. That is,
         * the balance factor of the node is between -1 and 1.
         * @details This is only intended for debugging purposes.
         */
        bool is_balanced();

        /**
         * @brief Check if the node and its children are a binary search tree.
         * That is left.key <= node.key <= right.key.
         * @details This is only intended for debugging purposes.
         */
        bool is_bst();

        /**
         * @brief Prints the node and its children in an inorder traversal.
         */
        void print(unsigned depth);
      } AVLNode;

      /********************************************************************************************/
      /*                                      FIELDS                                              */
      /********************************************************************************************/

      /**
       * @brief The root of the AVL tree.
       */
      AVLNode* _root;

      /**
       * @brief The path from the root to the node that is inserted or deleted. The path is used to
       * remove the recursion when balancing the tree.
       */
      std::vector<AVLNode*> _path;

      /**
       * @brief Rotate the node to the left.
       * @details
       *
       *      b                   d
       *    /   \               /   \
       *   a     d      =>     b     e
       *        / \           / \
       *       c   e         a   c
       *
       * @param node The node to rotate.
       * @return The new root of the subtree (d).
       */

      AVLNode* rotate_left(AVLNode* b);
      /**
       * @brief Rotate the node to the right.
       * @details
       *
       *      d                   b
       *    /   \               /   \
       *   b     e      =>     a     d
       *  / \                       / \
       * a   c                     c   e
       *
       * @param node The node to rotate.
       * @return The new root of the subtree (b).
       */
      AVLNode* rotate_right(AVLNode* d);

    public:
      /**
       * @brief Constructor of the AVL tree.
       */
      AVLTree() :
        _root(nullptr)
      { /* Do nothing*/ }

      /**
       * @brief Destructor of the AVL tree. It deletes the root of the tree and recursively deletes
       * the children.
       */
      ~AVLTree() {
        if (_root != nullptr)
          delete _root;
      }

      /**
       * @brief Insert a new node in the tree.
       * @param key The key of the node.
       * @param data The data associated with the key.
       * @return true if the node was inserted, false if the key already exists.
       * @par Time Complexity: (n is the number of nodes in the tree)
       * - Average case: O(log(n))
       * - Worst case: O(log(n))
       */
      bool insert(unsigned key, void* data);

      /**
       * @brief Remove a node from the tree.
       * @param key The key of the node to remove.
       * @param data The data associated with the key.
       * @return true if the node was removed, false if the key does not exist.
       * @par Time Complexity: (n is the number of nodes in the tree)
       * - Average case: O(log(n))
       * - Worst case: O(log(n))
       */
      bool remove(unsigned key, void* data);

      /**
       * @return the data associated with the smallest key that is greater than or equal to the given key.
       */
      void* find_best_fit(unsigned key);

      /**
       * @brief Check if the tree is balanced.
       * @return true if the tree is balanced, false otherwise.
       * @details This is only intended for debugging purposes.
       */
      bool is_balanced();

      /**
       * @brief Check if the tree is a binary search tree.
       * @return true if the tree is a binary search tree, false otherwise.
       * @details This is only intended for debugging purposes.
       */
      bool is_bst();

      /**
       * @brief Prints the tree in an inorder traversal.
       */
      void print();
  };



  /**
   * @brief A tree memory partition represents a partition of memory.
   * @details The tree is similar to the Buddy memory allocation algorithm. However, we use a
   * best-fit strategy to allocate memory. The best-fit is ensured by the AVL tree.
   * Another difference is that we do not allocate memory in powers of 2. Instead, we allocate
   * memory best aligned with the cache lines.
   *
   * @invariant Each node has either 0 or 2 children.
   * @invariant The start address of the partition is less than the end address.
   * @invariant The start address of a parent is identical to the start address of its left child.
   * @invariant The end address of a parent is identical to the end address of its right child.
   * @invariant The end address of the left child is identical to the start address of the right child.
   * @details The memory partition will ensure that the memory is aligned with the caches.
   */
  // class MemoryPartition
  // {
  //   /**
  //    * @invariant Nodes are always spread on the minimum number of cache lines.
  //    * @invariant A free node either fits entirely in one cache line or is right aligned with a
  //    * cache line.
  //    */
  //   typedef struct MemoryNode
  //   {
  //     private:
  //       std::byte* _start;
  //       size_t _size;

  //       /**
  //        * @brief The level of the node in the tree.
  //        * @details 0 for leaves, max(level(left), level(right)) + 1 for the other nodes.
  //        */
  //       unsigned _height;

  //       bool _free;

  //       MemoryNode* _left;
  //       MemoryNode* _right;
  //       MemoryNode* _parent;

  //     public:
  //       bool is_leaf() const;
  //       bool is_free() const;
  //       unsigned balance_factor() const;
  //       void split(size_t size);
  //       void update_height();

  //       MemoryNode(std::byte* start, size_t size) :
  //         _start(start),
  //         _size(size),
  //         _height(1),
  //         _free(true),
  //         _left(nullptr),
  //         _right(nullptr),
  //         _parent(nullptr)
  //       { /* Do nothing */ }

  //       ~MemoryNode() {
  //         if (_left != nullptr)
  //           delete _left;
  //         if (_right != nullptr)
  //           delete _right;
  //       }
  //   } MemoryNode;

  //   MemoryNode* root;

  //   utils::AVLTree free_nodes;

  //   public:
  //     MemoryPartition(void* start, void* end);
  //     ~MemoryPartition();
  //     void free(void* start);
  //     void* allocate(size_t size);
  // };
}
