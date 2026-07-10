/**
 * @file hitting-set.hpp
 * @author Robin Coutelier
 */

#pragma once

#include "bitset.hpp"

#include <vector>

namespace napsat
{
  void compute_hitting_sets(const std::vector<bitset>& to_hit,
                           std::vector<bitset>& hitting_set,
                           unsigned limit = 0);
}
