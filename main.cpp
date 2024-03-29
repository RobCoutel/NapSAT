#include <iostream>
#include <fstream>
#include <libgen.h>
#include "src/solver/NapSAT.hpp"
#include "src/environment.hpp"

using namespace std;
using namespace sat;

static const string warning_msg = "\033[0;33mWARNING\033[0m: ";

static void print_man_page(string man_file)
{
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
}

int main(int argc, char** argv)
{
  if (argc < 2) {
    cout << "Usage: " << argv[0] << " <input_file> [options]" << endl;
    return 1;
  }

  string exec_dir = string(dirname(argv[0]));
  sat::env::set_man_page_directory(exec_dir + "/../");
  sat::env::set_invariant_configuration_folder(exec_dir + "/../invariant-configurations/");

  if (string(argv[1]) == "-h" || string(argv[1]) == "--help") {
    string man_file = sat::env::get_man_page_directory() + "man.txt";
    print_man_page(man_file);
    return 0;
  }
  else if (string(argv[1]) == "-hs" || string(argv[1]) == "--help-sat-commands") {
    string man_file = sat::env::get_man_page_directory() + "man-sat.txt";
    print_man_page(man_file);
    return 0;
  }
  else if (string(argv[1]) == "-hn" || string(argv[1]) == "--help-navigation") {
    string man_file = sat::env::get_man_page_directory() + "man-nav.txt";
    print_man_page(man_file);
    return 0;
  }
  else if (string(argv[1]) == "-v" || string(argv[1]) == "--version") {
    cout << "NapSAT version " << VERSION << endl;
    return 0;
  }

  sat::env::set_input_file(argv[1]);
  sat::env::set_problem_name(string(basename(argv[1])));

  sat::options options(argv + 2, argc - 2);
  sat::NapSAT solver(0, 0, options);

  solver.parse_dimacs(argv[1]);
  solver.solve();

  if (solver.get_status() == sat::SAT)
    cout << "s SATISFIABLE" << endl;
  else if (solver.get_status() == sat::UNSAT)
    cout << "s UNSATISFIABLE" << endl;
  else
    cout << "UNKNOWN" << endl;

  if (options.print_stats) {
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
