/*
 * This file is part of the source code of the software program
 * NapSAT. It is protected by applicable copyright laws.
 *
 * This source code is protected by the terms of the MIT License.
 */
/**
 * @file main.cpp
 * @author Robin Coutelier
 * @brief This file is part of the NapSAT solver. It contains the main function of the SAT solver.
 */
#include <iostream>
#include <fstream>
#include <libgen.h>
#include <chrono>
#include "SAT-API.hpp"
#include "SAT-config.hpp"
#include "SAT-options.hpp"
#include "src/utils/printer.hpp"

using namespace std;
using namespace napsat;

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
    LOG_ERROR(": The manual page could not be loaded.");
  }
}

int main(int argc, char** argv)
{
  if (argc < 2) {
    cout << "Usage: " << argv[0] << " <input_file> [options]" << endl;
    return 1;
  }

  string exec_dir = string(dirname(argv[0]));
  napsat::env::set_man_page_folder(exec_dir + "/../");
  napsat::env::set_invariant_configuration_folder(exec_dir + "/../invariant-configurations/");

  if (string(argv[1]) == "-h" || string(argv[1]) == "--help") {
    string man_file = napsat::env::get_man_page_folder() + "man.txt";
    print_man_page(man_file);
    return 0;
  }
  else if (string(argv[1]) == "-hs" || string(argv[1]) == "--help-sat-commands") {
    string man_file = napsat::env::get_man_page_folder() + "man-sat.txt";
    print_man_page(man_file);
    return 0;
  }
  else if (string(argv[1]) == "-hn" || string(argv[1]) == "--help-navigation") {
    string man_file = napsat::env::get_man_page_folder() + "man-nav.txt";
    print_man_page(man_file);
    return 0;
  }
  else if (string(argv[1]) == "-v" || string(argv[1]) == "--version") {
    cout << "NapSAT version " << VERSION << endl;
    return 0;
  }

  vector<string> tokens(argv + 2, argv + argc);
  tokens = napsat::env::extract_environment_variables(tokens);

  napsat::options options(tokens);
  napsat::NapSAT* solver = create_solver(0, 0, options);

  if (!parse_dimacs(solver, argv[1])) {
    LOG_ERROR("The input file could not be parsed.");
    delete_solver(solver);
    return 1;
  }
  chrono::time_point<chrono::high_resolution_clock> start = chrono::high_resolution_clock::now();
  solve(solver);
  chrono::time_point<chrono::high_resolution_clock> end = chrono::high_resolution_clock::now();
  chrono::milliseconds duration = chrono::duration_cast<chrono::milliseconds>(end - start);

  cout << "c Solution found in " << pretty_time(duration) << endl;

  if (get_status(solver) == napsat::SAT) {
    cout << "s SATISFIABLE" << endl;
    cout << "v ";
    for (Tlit lit : get_partial_assignment(solver))
      cout << (lit_pol(lit) ? "" : "-") << lit_to_var(lit) << " ";
    cout << endl;
  }
  else if (get_status(solver) == napsat::UNSAT)
    cout << "s UNSATISFIABLE" << endl;
  else
    cout << "UNKNOWN" << endl;

  if (options.print_stats) {
    print_statistics(solver);
  }
  if (options.check_proof && get_status(solver) == napsat::UNSAT && !check_proof(solver)) {
    cout << WARNING_HEAD << "The proof is invalid." << endl;
  }
  if (options.print_proof && get_status(solver) == napsat::UNSAT) {
    print_proof(solver);
  }

  delete_solver(solver);

  return 0;
}
