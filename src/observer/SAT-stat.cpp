#include "SAT-stat.hpp"

#include <string>

#include "../utils/printer.hpp"

namespace napsat {
  napsat::statistics::statistics(napsat::options& options) : _options(options) {
    _creation_time = std::chrono::high_resolution_clock::now();
  }

  statistics::stat* statistics::add_stat(std::string name, const std::string& category, const stat_type type) {
    stat s(std::move(name), type);
    auto& l = _stats[category];
    return &l.emplace_back(s);
  }

  std::string statistics::get_statistics() const {
    std::ostringstream ss;
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::high_resolution_clock::now() - _creation_time);
    ss << "c Time: " + pretty_time(duration) << std::endl;
    // print the core stats first if available
    auto it = _stats.find("Core statistics");

    if (it != _stats.end()) {
      const auto& name = it->first;
      const auto& stats = it->second;
      ss << "c " << (name.empty() ? "Statistics" : name) << ":" << std::endl;
      for (const auto& stat : stats) {
        if (stat._cnt == 0 && stat._val == 0)
          continue;
        ss << "c  - " << stat._name << ": ";
        switch (stat._type) {
          case AVERAGE:
            if (stat._cnt > 0)
              ss << pretty_float(static_cast<double>(stat._val) / static_cast<double>(stat._cnt));
            else
              ss << "---";
            break;
          case COUNT:
            ss << pretty_integer(stat._val);
            break;
        }
        ss << std::endl;
      }
    }

    for (const auto& [name, stats] : _stats) {
      if (name == "Core statistics")
        continue;
      ss << "c " << (name.empty() ? "Statistics" : name) << ":" << std::endl;
      for (const auto& stat : stats) {
        if (stat._cnt == 0 && stat._val == 0)
          continue;
        ss << "c  - " << stat._name << ": ";
        switch (stat._type) {
          case AVERAGE:
            if (stat._cnt > 0)
              ss << pretty_float(static_cast<double>(stat._val) / static_cast<double>(stat._cnt));
            else
              ss << "---";
            break;
          case COUNT:
            ss << pretty_integer(stat._val);
            break;
        }
        ss << std::endl;
      }
    }
    return ss.str();
  }
}
