#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>
#include "../src/utils/luby-counter.hpp"
#include <vector>
#include <iostream>

using namespace napsat;

TEST_CASE("Luby Counter basic operations", "[luby_counter]") {
  luby_counter lc;

  // for (unsigned i = 0; i < 100; i++) {
  //   std::cout << "Increment: " << i << ": " << lc.increment() << std::endl;
  // }
  // lc.reset();

  std::vector<unsigned> sequence = {1, 2, 1, 1, 2, 4, 1, 1, 2, 4, 8};
  // reverse the sequence
  std::reverse(sequence.begin(), sequence.end());
  unsigned value = 0;
  unsigned next_value = 1;
  while (true) {
    bool expected = false;\
    value++;
    if (value >= next_value) {
      expected = true;
      if (sequence.empty()) {
        break;
      }
      next_value += sequence.back();
      sequence.pop_back();
    }
    REQUIRE(lc.increment() == expected);
  }
}
