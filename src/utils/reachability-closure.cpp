/**
 * @file reachability-closure.cpp
 * @author Robin Coutelier
 *
 * @brief See reachability-closure.hpp.
 */
#include "reachability-closure.hpp"

#include <iostream>
#include <queue>
#include <cassert>

using namespace std;

namespace napsat
{

namespace
{
  bool subsumed(const vector<bitset>& completions, const bitset& b)
  {
    for (const bitset& c : completions) {
      if (c <= b) {
        return true;
      }
    }
    return false;
  }

  struct closure_node {
    bitset current;
    bitset processed;
  };

  struct compare_closure_nodes {
    bool operator()(const closure_node& a, const closure_node& b) const
    {
      return a.current.count() > b.current.count();
    }
  };
}


void compute_reachability_closures(const vector<bitset>& parents,
                                   const bitset& mergeable,
                                   const bitset& forbidden,
                                   const bitset& start,
                                   vector<bitset>& completions,
                                   unsigned limit)
{
  unsigned allocated_size = start.capacity();
  assert (parents.size() == allocated_size);
  assert (forbidden.capacity() == allocated_size);
  assert (mergeable.capacity() == allocated_size);

  cout << "Computing reachability closures for start = " << start.to_string() << ", forbidden = " << forbidden.to_string() << endl;
  cout << "Completions before: " << completions.size() << endl;
  for (size_t i = 0; i < completions.size(); i++) {
    cout << "  " << i << ": " << completions[i].to_string() << endl;
  }

  priority_queue<closure_node, vector<closure_node>, compare_closure_nodes> stack;
  stack.push({start, bitset(allocated_size)});

  while(!stack.empty()) {
    closure_node node = stack.top();
    stack.pop();

    if (subsumed(completions, node.current)) {
      continue;
    }

    bitset remaining = (node.current & mergeable);
    remaining -= node.processed;

    if (remaining.empty()) {
      completions.push_back(node.current);
      if (limit > 0 && completions.size() >= limit) {
        return;
      }
      continue;
    }

    for (auto it = remaining.cbegin(); it != remaining.cend(); ++it) {
      unsigned c = *it;
      if (forbidden[c])
        continue;
      assert(c < parents.size());
      assert(!node.processed[c]);
      assert(!parents[c].empty());

      bitset new_processed = node.processed;
      new_processed.set(c, true);
      for (auto p_it = parents[c].cbegin(); p_it != parents[c].cend(); ++p_it) {
        unsigned p = *p_it;
        if (forbidden[p]) {
          continue;
        }
        assert(p < allocated_size);
        bitset new_current = node.current;
        new_current.set(p, true);
        stack.push({new_current, new_processed});
      }
    }
  }
}

} // namespace napsat
