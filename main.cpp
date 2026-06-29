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
  env::set_man_page_folder(exec_dir + "/../");
  env::set_invariant_configuration_folder(exec_dir + "/../invariant-configurations/");
  env::set_obsidian_template_folder(exec_dir + "/../obsidian_template/");

  bool all_models = false;
  if (string(argv[1]) == "-h" || string(argv[1]) == "--help") {
    string man_file = env::get_man_page_folder() + "man.txt";
    print_man_page(man_file);
    return 0;
  }
  else if (string(argv[1]) == "-hs" || string(argv[1]) == "--help-sat-commands") {
    string man_file = env::get_man_page_folder() + "man-sat.txt";
    print_man_page(man_file);
    return 0;
  }
  else if (string(argv[1]) == "-hn" || string(argv[1]) == "--help-navigation") {
    string man_file = env::get_man_page_folder() + "man-nav.txt";
    print_man_page(man_file);
    return 0;
  }
  else if (string(argv[1]) == "-v" || string(argv[1]) == "--version") {
    cout << "NapSAT version " << VERSION << endl;
    return 0;
  } else if (string(argv[1]) == "--all-models") {
    all_models = true;
    for (unsigned i = 1; i < (unsigned) argc - 1; i++) {
      argv[i] = argv[i + 1];
    }
    argc--;
  }

  vector<string> tokens(argv + 2, argv + argc);
  tokens = env::extract_environment_variables(tokens);

  options options(tokens);
  NapSAT* solver = create_solver(0, 0, options);

  chrono::time_point<chrono::high_resolution_clock> start = chrono::high_resolution_clock::now();
  if (!parse_dimacs(solver, argv[1])) {
    LOG_ERROR("The input file could not be parsed.");
    delete_solver(solver);
    return 1;
  }
  chrono::microseconds parse_duration = chrono::duration_cast<chrono::microseconds>(chrono::high_resolution_clock::now() - start);
  cout << "c Input file parsed in " << pretty_time(parse_duration) << endl;
  start = chrono::high_resolution_clock::now();

  if (all_models) {
    unsigned n_models = 0;
    while (solve(solver) == status::SAT) {
      synchronize(solver);
      cout << "Model found: " << n_models << "\r " << std::flush;
      vector<Tlit> blocking_clause;
      const vector<Tlit>& trail = get_partial_assignment(solver);
      for (Tlit lit : trail) {
        blocking_clause.push_back(~lit);
      }
      add_clause(solver, blocking_clause.data(), blocking_clause.size());
      n_models++;
    }
  } else {
    solve(solver);
  }
  chrono::time_point<chrono::high_resolution_clock> end = chrono::high_resolution_clock::now();
  chrono::microseconds duration = chrono::duration_cast<chrono::microseconds>(end - start);

  cout << "c Solution found in " << pretty_time(duration) << endl;
  cout << "c  - Time (ms): " << duration.count() << endl;

  if (get_status(solver) == status::SAT) {
    cout << "s SATISFIABLE" << endl;
  }
  else if (get_status(solver) == status::UNSAT)
    cout << "s UNSATISFIABLE" << endl;
  else if (get_status(solver) == status::TIMEOUT)
    cout << "s TIMEOUT" << endl;
  else
    cout << "UNKNOWN" << endl;

  if (options.print_stats) {
    print_statistics(solver);
  }
  if (options.check_proof && get_status(solver) == status::UNSAT && !check_proof(solver)) {
    LOG_ERROR("The proof is invalid.");
  }
  if (options.print_proof && get_status(solver) == status::UNSAT) {
    print_proof(solver);
  }

  delete_solver(solver);

  return 0;
}
