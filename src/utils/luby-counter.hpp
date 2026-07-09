/**
 * Luby Counter Header
 */

#pragma once


#include <iostream>

namespace napsat
{
  class luby_counter
  {
    private:
      unsigned _count = 0;
      unsigned _order = 1;
      unsigned _next_power_of_two = 1;
      bool skip = true;

    public:
      luby_counter() = default;

      void reset() {
        _count = 0;
        _order = 1;
        _next_power_of_two = 1;
        skip = true;
      }

      bool increment()
      {
        _count++;
        if (_count == _next_power_of_two) {
          _count = 0;
          if (_next_power_of_two >> _order == 0) {
            _next_power_of_two <<= skip ? 0 : 1;
            skip = false;
          } else {
            _order++;
            _next_power_of_two = 1;
            skip = true;
          }
          return true;
        }
        return false;
      }
  };
} // namespace napsat
