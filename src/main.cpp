#include <iostream>
#include <fstream>
#include "modulariT-SAT.hpp"

using namespace std;
using namespace sat;

int main(int argc, char** argv)
{
  if (argc < 2) {
      cout << "Usage: " << argv[0] << " <input_file>" << endl;
      return 1;
  }

  sat::modulariT_SAT solver(20, 91);
  solver.toggle_chronological_backtracking(true);
  solver.parse_dimacs(argv[1]);
  solver.solve();

  if (solver.get_status() == sat::modulariT_SAT::SAT) {
    cout << "s SATISFIABLE" << endl;
  } else if (solver.get_status() == sat::modulariT_SAT::UNSAT) {
    cout << "s UNSATISFIABLE" << endl;
  } else {
    cout << "UNKNOWN" << endl;
  }

  // solver.print_trail();
  return 0;
}
