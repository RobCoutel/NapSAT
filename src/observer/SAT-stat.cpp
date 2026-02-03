#include "SAT-stat.hpp"

#include <string>

#include "../utils/printer.hpp"
#include <sys/ioctl.h>
#include <unistd.h>
#include <iomanip>

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
    ss << "c Time since creation: " + pretty_time(duration) << std::endl;
    // print the core stats first if available

    for (const auto& [name, stats] : _stats) {
      bool time = name == "1. Runtime";
      ss << "c " << (name.empty() ? "Statistics" : name) << ":" << std::endl;
      for (const auto& stat : stats) {
        if (stat._cnt == 0 && stat._val == 0)
          continue;
        ss << "c  - " << stat._name << ": ";
        if (time) {
          ss << pretty_time(std::chrono::milliseconds(stat._val));
        } else
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

  static unsigned last_line_count = 0;
  static unsigned TERMINAL_WIDTH = 80;
  static bool last_clear = false;

  static void update_terminal_width() {
  #ifdef __unix__
    struct winsize size;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);
    short int width = size.ws_col;
    if (width > 0)
      TERMINAL_WIDTH = size.ws_col;
  #endif
  #ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
      GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
      TERMINAL_WIDTH = csbi.srWindow.Right - csbi.srWindow.Left + 1;
  #endif
  }

  void statistics::print_statistics(bool clear)const
  {

    if (last_clear) {
      for (unsigned i = 0; i < last_line_count; i++)
        std::cout << "\033[A";
    } else {
      for (unsigned i = 0; i < TERMINAL_WIDTH; i++)
        std::cout << "*";
      std::cout << std::endl;
    }
    last_clear = clear;
    update_terminal_width();
    const std::string s = get_statistics();
    std::vector<std::string> lines;
    unsigned last_line_end = 0;
    for (unsigned i = 0; i < s.size(); i++) {
      if (s[i] == '\n') {
        std::string line = s.substr(last_line_end, i - last_line_end);
        lines.push_back(line);
        last_line_end = i + 1;
      }
    }
    // print the lines and pad them with spaces
    for (unsigned i = 0; i < lines.size(); i++) {
      std::cout << std::setw (TERMINAL_WIDTH) << std::left << lines[i] << std::endl;
    }
    // bring the cursor up to the beginning of the statistics
    last_line_count = lines.size();

  }
}
