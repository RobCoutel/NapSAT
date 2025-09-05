#include "SAT-stat.hpp"

#include <string>

#include "../utils/printer.hpp"

namespace napsat::gui {
  napsat::gui::statistics::statistics(napsat::options& options) : _options(options) {
    _creation_time = std::chrono::high_resolution_clock::now();
  }

  std::string statistics::get_statistics() const {
    std::ostringstream ss;
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::high_resolution_clock::now() - _creation_time);
    ss << "c Time: " + pretty_time(duration) << std::endl;
    ss << "c Statistics:" << std::endl;
    for (const auto& stat : _stats) {
      ss << "c  - " << stat._name << ": ";
      switch (stat._type) {
        case AVERAGE:
          ss << pretty_float((double)stat._val / stat._cnt);
          break;
        case COUNT:
          ss << pretty_integer(stat._val);
          break;
      }
      ss << std::endl;
    }
    return ss.str();
  }
}