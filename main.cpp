#include <iostream>
#include <fstream>
#include <libgen.h>
#include "src/solver/modulariT-SAT.hpp"

using namespace std;
using namespace sat;

static const string warning_msg = "\033[0;33mWARNING\033[0m: ";

int main(int argc, char** argv)
{
  if (argc < 2) {
    cout << "Usage: " << argv[0] << " <input_file> [options]" << endl;
    return 1;
  }

  if (string(argv[1]) == "-h" || string(argv[1]) == "--help") {
    // print the content of the file man.txt
    string man_file = string(dirname(argv[0])) + "../man.txt";
    ifstream file(man_file);
    if (file.is_open()) {
      string line;
      while (getline(file, line))
        cout << line << endl;
      file.close();
    }
    else {
      cerr << "Error: could not load the manual page." << endl;
    }
    return 0;
  }

  sat::modulariT_SAT solver(0, 0);
  bool non_chonological_backtracking = false;
  bool chronological_backtracking = false;
  bool strong_chronological_backtracking = false;
  bool observe = false;
  string save_file = "";
  bool interactive = false;
  string commands_file = "";
  bool check_invariants = false;
  bool print_stats = false;
  bool suppress_warning = false;

  for (int i = 2; i < argc; i++) {
    if (string(argv[i]) == "-ncb" || string(argv[i]) == "--non-chronological-backtracking")
      non_chonological_backtracking = true;
    else if (string(argv[i]) == "-cb" || string(argv[i]) == "--chronological-backtracking")
      chronological_backtracking = true;
    else if (string(argv[i]) == "-scb" || string(argv[i]) == "--strong-chronological-backtracking")
      strong_chronological_backtracking = true;
    else if (string(argv[i]) == "-o" || string(argv[i]) == "--observer")
      observe = true;
    else if (string(argv[i]) == "-stats" || string(argv[i]) == "--print-statistics")
      print_stats = true;
    else if (string(argv[i]) == "--suppress-warning")
      suppress_warning = true;
    else if (string(argv[i]) == "-i" || string(argv[i]) == "--interactive") {
      interactive = true;
      if (argc > i + 1 && string(argv[i + 1])[0] != '-') {
        commands_file = argv[i + 1];
        i++;
      }
    }
    else if (string(argv[i]) == "-c" || string(argv[i]) == "--check-invariants")
      check_invariants = true;
    else if (string(argv[i]) == "-s" || string(argv[i]) == "--save") {
      if (argc > i + 1 && string(argv[i + 1])[0] != '-') {
        save_file = argv[i + 1];
        i++;
      }
      else
        save_file = "save.txt";
    }
    else
      cout << "Unknown option: " << argv[i] << endl;
  }

  /***************************************************************************/
  /*                           OPTIONS PROCESSING                            */
  /***************************************************************************/
  if (non_chonological_backtracking) {
    if (chronological_backtracking || strong_chronological_backtracking)
      cerr << warning_msg + "non-chronological backtracking overrides chronological backtracking" << endl;
    else
      solver.toggle_chronological_backtracking(false);
  }
  if (strong_chronological_backtracking) {
    cout << "Strong Chronological Backtracking enabled" << endl;
    solver.toggle_strong_chronological_backtracking(true);
  }
  if (chronological_backtracking) {
    cout << "Chronological Backtracking enabled" << endl;
    if (strong_chronological_backtracking)
      cerr << warning_msg + "strong chronological backtracking overrides chronological backtracking" << endl;
    else
      solver.toggle_chronological_backtracking(true);
  }
  if (interactive) {
    solver.toggle_interactive(true);
    sat::gui::observer* obs = solver.get_observer();
    assert(obs != nullptr);
    if (commands_file != "")
      obs->load_commands(commands_file);
    obs->notify(new sat::gui::marker("start"));
  }
  if (observe) {
    if (interactive)
      cerr << warning_msg + "interactive mode overrides observer" << endl;
    else {
      solver.toggle_observing(true);
      sat::gui::observer* obs = solver.get_observer();
      assert(obs != nullptr);
      obs->notify(new sat::gui::marker("start"));
    }
  }
  if (check_invariants) {
    if (interactive)
      cerr << warning_msg + "interactive mode overrides check invariants" << endl;
    else if (observe)
      cerr << warning_msg + "observe mode overrides check invariants" << endl;
    else {
      solver.toggle_observing(true);
      sat::gui::observer* obs = solver.get_observer();
      assert(obs != nullptr);
      obs->toggle_checking_only(true);
    }
  }
  if (print_stats) {
    if (!solver.is_observing()) {
      solver.toggle_observing(true);
      sat::gui::observer* obs = solver.get_observer();
      assert(obs != nullptr);
      obs->toggle_stats_only(true);
    }
  }
  if (suppress_warning) {
    if (!solver.is_observing())
      cerr << warning_msg + "suppress warning option only works with observer" << endl;
    else {
      sat::gui::notification::suppress_warning(true);
    }
  }

  solver.parse_dimacs(argv[1]);
  solver.solve();

  if (solver.get_status() == sat::SAT)
    cout << "s SATISFIABLE" << endl;
  else if (solver.get_status() == sat::UNSAT)
    cout << "s UNSATISFIABLE" << endl;
  else
    cout << "UNKNOWN" << endl;

  if (print_stats) {
    sat::gui::observer* obs = solver.get_observer();
    if (obs != nullptr)
      cout << obs->get_statistics() << endl;
    else {
      cout << "The solver was not run with the observer enabled" << endl;
      cout << "No statistics available" << endl;
    }
  }

  return 0;
}
