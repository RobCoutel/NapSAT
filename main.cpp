#include <iostream>
#include <fstream>
#include <libgen.h>
#include <chrono>
#include "SAT-API.hpp"
#include "SAT-config.hpp"
#include "src/environment.hpp"

using namespace std;
using namespace napsat;

static string time_to_string(chrono::milliseconds time)
{
  string str = "";
  const long long ms = time.count();
  const long long hours = ms / 3600000;
  const long long minutes = (ms % 3600000) / 60000;
  const long long seconds = (ms % 60000) / 1000;
  const long long milliseconds = ms % 1000;
  if (hours > 0)
    str += to_string(hours) + "h ";
  if (minutes > 0)
    str += to_string(minutes) + "m ";
  if (seconds > 0)
    str += to_string(seconds) + "s ";
  str += to_string(milliseconds) + "ms";
  return str;
}

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
  napsat::env::set_man_page_directory(exec_dir + "/../");
  napsat::env::set_invariant_configuration_folder(exec_dir + "/../invariant-configurations/");

  if (string(argv[1]) == "-h" || string(argv[1]) == "--help") {
    string man_file = napsat::env::get_man_page_directory() + "man.txt";
    print_man_page(man_file);
    return 0;
  }
  else if (string(argv[1]) == "-hs" || string(argv[1]) == "--help-sat-commands") {
    string man_file = napsat::env::get_man_page_directory() + "man-sat.txt";
    print_man_page(man_file);
    return 0;
  }
  else if (string(argv[1]) == "-hn" || string(argv[1]) == "--help-navigation") {
    string man_file = napsat::env::get_man_page_directory() + "man-nav.txt";
    print_man_page(man_file);
    return 0;
  }
  else if (string(argv[1]) == "-v" || string(argv[1]) == "--version") {
    cout << "NapSAT version " << VERSION << endl;
    return 0;
  }

  napsat::env::set_input_file(argv[1]);
  napsat::env::set_problem_name(string(basename(argv[1])));

  napsat::options options(argv + 2, argc - 2);
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

  cout << "Solution found in " << time_to_string(duration) << endl;

  if (get_status(solver) == napsat::SAT)
    cout << "s SATISFIABLE" << endl;
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
