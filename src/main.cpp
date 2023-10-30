#include <iostream>
#include <fstream>
#include <libgen.h>
#include "modulariT-SAT.hpp"

using namespace std;
using namespace sat;

int main(int argc, char **argv)
{
  if (argc < 2)
  {
    cout << "Usage: " << argv[0] << " <input_file> [options]" << endl;
    return 1;
  }

  if (string(argv[1]) == "-h" || string(argv[1]) == "--help")
  {
    // print the content of the file man.txt
    string man_file = string(dirname(argv[0])) + "/../man.txt";
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

  bool interactive = false;
  sat::modulariT_SAT solver(20, 91);
  cout << "argc = " << argc << endl;
  for (int i = 2; i < argc; i++)
  {
    if (string(argv[i]) == "-cb" || string(argv[i]) == "--chronological-backtracking")
    {
      cout << "Using chronological backtracking" << endl;
      solver.toggle_chronological_backtracking(true);
    }

    else if (string(argv[i]) == "-i" || string(argv[i]) == "--interactive")
    {
      cout << "Interactive run enabled" << endl;
      interactive = true;
    }
  }

  solver.parse_dimacs(argv[1]);
  if (!interactive)
    solver.solve();
  else
    solver.solve_interactive();

  if (solver.get_status() == sat::SAT)
    cout << "s SATISFIABLE" << endl;
  else if (solver.get_status() == sat::UNSAT)
    cout << "s UNSATISFIABLE" << endl;
  else
    cout << "UNKNOWN" << endl;

  // solver.print_trail();
  return 0;
}
