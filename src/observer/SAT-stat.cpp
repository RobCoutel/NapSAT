#include "SAT-stat.hpp"

#include <string>

#include "../utils/printer.hpp"
#include <sys/ioctl.h>
#include <unistd.h>
#include <iomanip>

using namespace std;

namespace napsat {
  napsat::statistics::statistics(const napsat::Options* options) : _options(options) {
    _creation_time = chrono::high_resolution_clock::now();
  }

  statistics::stat* statistics::add_stat(string name, const string& category, const stat_type type) {
    stat s(move(name), type);
    auto& l = _stats[category];
    return &l.emplace_back(s);
  }

  string statistics::get_statistics() const {
    ostringstream ss;
    const auto now = chrono::high_resolution_clock::now();
    const auto duration = chrono::duration_cast<chrono::microseconds>(
      now - _creation_time);
    ss << "c Time since creation: " + pretty_time(duration) << endl;

    for (const auto& [name, stats] : _stats) {
      ss << "c " << (name.empty() ? "Statistics" : name) << ":" << endl;
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
            // print the count per second as well
            // if (duration.count() > 0) {
            //   double cps = (static_cast<double>(stat._val) * 1000000.0) / static_cast<double>(duration.count());
            //   ss << "     (" << pretty_float(cps) << " per second)";
            // }
            break;
          case TIME:
            ss << pretty_time(chrono::microseconds(stat._val));
            break;
        }
        ss << endl;
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

  void statistics::print_statistics(bool clear) const
  {
    if (last_clear) {
      for (unsigned i = 0; i < last_line_count; i++)
        cout << "\033[A";
    } else {
      for (unsigned i = 0; i < TERMINAL_WIDTH; i++)
        cout << "*";
      cout << endl;
    }
    last_clear = clear;
    update_terminal_width();
    const string s = get_statistics();
    vector<string> lines;
    unsigned last_line_end = 0;
    for (unsigned i = 0; i < s.size(); i++) {
      if (s[i] == '\n') {
        string line = s.substr(last_line_end, i - last_line_end);
        lines.push_back(line);
        last_line_end = i + 1;
      }
    }
    // print the lines and pad them with spaces
    for (unsigned i = 0; i < lines.size(); i++) {
      cout << setw (TERMINAL_WIDTH) << left << lines[i] << "\n";
    }
    cout << flush;
    // bring the cursor up to the beginning of the statistics
    last_line_count = lines.size();

  }
}
