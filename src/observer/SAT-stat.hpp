#pragma once

#include <string>
#include <map>
#include <list>
#include <chrono>
#include <utility>

#include "SAT-options.hpp"

namespace napsat {
  class statistics {
  public:
    enum stat_type {
      AVERAGE,
      COUNT,
      TIME
    };

    struct stat {
      friend class statistics;

      const std::string _name;
      const stat_type _type;

      inline void inc() noexcept { ++_cnt; ++_val; }
      inline void inc(unsigned v) noexcept  { ++_cnt; _val += v; }

      inline void dec() noexcept { --_cnt; --_val; }
      inline void dec(unsigned v) noexcept { --_cnt; _val -= v; }

      inline void operator++() noexcept { inc(); }
      inline void operator--() noexcept { dec(); }
      inline void operator+=(unsigned v) noexcept { inc(v); }
      inline void operator-=(unsigned v) noexcept { dec(v); }

    private:
      long long _cnt = 0;
      long long _val = 0;

      stat(std::string name, stat_type type) : _name(std::move(name)), _type(type) {}
    };

    explicit statistics(napsat::options &options);

    stat* add_stat(std::string name, const std::string& category = "", stat_type type = COUNT);

    [[nodiscard]] std::string get_statistics() const;

    void print_statistics(bool clear) const;

  private:
    napsat::options _options;

    /**
     * @brief The time when the observer was created.
     */
    std::chrono::time_point<std::chrono::high_resolution_clock> _creation_time;

    /**
     * @brief List of all registered stats
     */
    std::map<std::string, std::list<stat>> _stats;
  };
}
