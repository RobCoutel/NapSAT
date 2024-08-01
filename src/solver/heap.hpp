/**
 * @file src/solver/heap.hpp
 * @author Robin Coutelier
 *
 * @brief This file is part of the SAT solver NapSAT. If defines a heap data structure for integer keys with double activities.
*/
#pragma once

#include <vector>

#define LOCATION_UNDEF 0xffffffff

namespace napsat::utils
{
  /**
   * @warning The heap should not be used for arbitrary keys. The memory allocated is proportional to the largest key. This structure is meant for densely packed keys.
  */
  class heap
  {
  private:
    /**
     * @brief The heap.
    */
    std::vector<unsigned> _heap;
    /**
     * @brief The index of each element in the heap.
    */
    std::vector<unsigned> _index;
    /**
     * @brief The activity of each element in the heap.
    */
    std::vector<double> _activity;

    inline unsigned parent(unsigned i)
    {
      return (i - 1) / 2;
    }
    inline unsigned left(unsigned i)
    {
      return 2 * i + 1;
    }
    inline unsigned right(unsigned i)
    {
      return 2 * i + 2;
    }

    /**
     * @brief Swaps two elements in the heap.
     * @param i The index of the first element.
     * @param j The index of the second element.
     * @details This function swaps the elements in the heap, the index and the activities.
    */
    void swap(unsigned i, unsigned j);

    /**
     * @brief Heapifies downward the heap from the given index down.
     * @param i The index from which to heapify down.
     * @pre The activity of the element at index i is lower than or equal to the activity of its children.
    */
    void heapify_down(unsigned i);

    /**
     * @brief Heapifies upward the heap from the given index up.
     * @param i The index from which to heapify up.
     * @pre The activity of the element at index i is greater than or equal to the activity of its parent.
    */
    void heapify_up(unsigned i);

  public:
    /**
     * @brief Inserts a new element in the heap.
     * @param key The key of the element to insert.
     * @param activity The activity of the element to insert.
     * @pre The heap does not contain the given key.
     * @details Complexity: O(log n) where n is the size of the heap.
    */
    void insert(unsigned key, double activity);

    /**
     * @brief Removes an element from the heap.
     * @param key The key of the element to remove.
     * @pre The heap contains the given key.
     * @details Complexity: O(log n) where n is the size of the heap.
    */
    void remove(unsigned key);

    /**
     * @brief Increases the activity of an element in the heap.
     * @param key The key of the element to increase.
     * @param activity The new activity of the element.
     * @pre The heap contains the given key.
     * @pre The activity of the element is lower than the given activity.
     * @details Complexity: O(log n) where n is the size of the heap.
     * @details This function is slightly faster than calling update if the activity is known to increase.
    */
    void increase_activity(unsigned key, double activity);

    /**
     * @brief Updates the activity of an element in the heap.
     * @param key The key of the element to update.
     * @param activity The new activity of the element.
     * @pre The heap contains the given key.
     * @details Complexity: O(log n) where n is the size of the heap.
    */
    void update(unsigned key, double activity);

    /**
     * @brief Down scales the activity of all elements in the heap.
     * @param factor The factor by which to down scale the activity of all elements.
     * @details Complexity: O(n) where n is the size of the heap.
    */
    void normalize(double factor);

    /**
     * @brief Checks if the heap contains the given key.
     * @param key The key to check.
     * @return True if the heap contains the given key, false otherwise.
     * @details Complexity: O(1).
    */
    bool contains(unsigned key);

    /**
     * @brief Checks if the heap is empty.
     * @return True if the heap is empty, false otherwise.
     * @details Complexity: O(1).
    */
    bool empty();

    /**
     * @brief Returns the key of the element at the top of the heap and removes it.
     * @return The key of the element at the top of the heap.
     * @pre The heap is not empty.
     * @details Complexity: O(log n) where n is the size of the heap.
    */
    unsigned pop();

    /**
     * @brief Returns the key of the element at the top of the heap.
     * @return The key of the element at the top of the heap.
     * @pre The heap is not empty.
     * @details Complexity: O(1).
    */
    unsigned top();

    /**
     * @brief Returns the size of the heap.
     * @return The size of the heap.
     * @details Complexity: O(1).
    */
    unsigned size();

    /**
     * @brief Prints the heap.
     * @details This function is meant for debugging purposes.
     * @details Complexity: O(n) where n is the size of the heap.
     * @todo This function is not clean and should be removed.
    */
    void print();
  };
} // namespace napsat::utils
