/**
 * @file src/solver/SAT-options.cpp
 * @author Robin Coutelier
 *
 * @brief This file is part of the SMT Solver modulariT. It contains the option parser for the
 * standalone SAT solver.
 */
#include "SAT-options.hpp"

#include "../observer/SAT-notification.hpp"

#include <string>
#include <iostream>
#include <unordered_set>
#include <unordered_map>

using namespace std;

sat::options::options(char** tokens, unsigned n_tokens)
{
  /**
   * @brief set of options that are already set. Used to prevent setting twice the same option.
   */
  std::unordered_set<std::string> set_options;
  /**
   * @brief map of boolean options that can be set with a string.
  */
  std::unordered_map<string, bool*> bool_options = {
    {"-cb", &chronological_backtracking},
    {"--chronological-backtracking", &chronological_backtracking},
    {"-scb", &strong_chronological_backtracking},
    {"--strong-chronological-backtracking", &strong_chronological_backtracking},
    {"-o", &observing},
    {"--observing", &observing},
    {"-i", &interactive},
    {"--interactive", &interactive},
    {"-sw", &suppress_warning},
    {"--suppress-warning", &suppress_warning},
    {"-c", &check_invariants},
    {"--check-invariants", &check_invariants},
    {"-stat", &print_stats},
    {"--statistics", &print_stats},
    {"-del", &delete_clauses},
    {"--delete-clauses", &delete_clauses}
  };

  /**
   * @brief map of double options that can be set with a string.
  */
  std::unordered_map<string, double*> double_options = {
    {"--clause-elimination-multiplier", &clause_elimination_multiplier},
    {"--clause-activity-multiplier", &clause_activity_multiplier},
    {"--clause-activity-threshold-decay", &clause_activity_threshold_decay},
    {"--var-activity-decay", &var_activity_decay},
    {"--agility-decay", &agility_decay},
    {"--agility-threshold", &agility_threshold},
    {"--agility-threshold-decay", &agility_threshold_decay}
  };

  /**
   * @brief map of string options that can be set with a string.
  */
  std::unordered_map<string, string*> string_options = {
    {"-s", &save_folder},
    {"--save", &save_folder},
    {"--commands", &commands_file}
  };



  for (unsigned i = 0; i < n_tokens; i++) {
    string token = string(tokens[i]);
    string next_token = (i + 1 < n_tokens) ? string(tokens[i + 1]) : "";

    if (set_options.find(token) != set_options.end()) {
      cerr << "Warning: option " << token << " already set. The second occurrence is ignored." << endl;
      continue;
    }
    if (bool_options.find(token) != bool_options.end()) {
      *bool_options[token] = true;
      set_options.insert(token);
    }
    else if (double_options.find(token) != double_options.end()) {
      if (next_token == "" || next_token[0] == '-') {
        cerr << "Error: option " << token << " requires a value." << endl;
        exit(1);
      }
      try {
        *double_options[token] = stod(next_token);
        set_options.insert(token);
        i++;
      }
      catch (const std::invalid_argument& ia) {
        cerr << "Error: option " << token << " requires a double value." << endl;
        exit(1);
      }
    }
    else if (string_options.find(token) != string_options.end()) {
      if (next_token == "" || next_token[0] == '-') {
        cerr << "Error: option " << token << " requires a value." << endl;
        exit(1);
      }
      *string_options[token] = next_token;
      set_options.insert(token);
      i++;
    }
    else {
      cerr << "Warning: unknown option " << token << endl;
    }
  }

  /****************************************************************************/
  /**                          OPTION COMPATIBILITY                          **/
  /****************************************************************************/
  if (chronological_backtracking && strong_chronological_backtracking) {
    cerr << "Warning: strong chronological backtracking subsumes chronological backtracking. The option is ignored." << endl;
  }
  if (strong_chronological_backtracking) {
    chronological_backtracking = true;
  }
  if (suppress_warning && !observing && !interactive && !check_invariants) {
    cerr << "Warning: suppress warning requires observing. The options is ignored" << endl;
  }
  else if (suppress_warning) {
    sat::gui::notification::suppress_warning(true);
  }
  // if (print_stats && !observing && !interactive && !check_invariants) {
  //   cerr << "Warning: print stats requires observing or interactive. The option is ignored." << endl;
  // }
  if (clause_activity_threshold_decay <= 0 || clause_activity_threshold_decay >= 1) {
    cerr << "Error: clause activity threshold decay must be between 0 and 1." << endl;
    exit(1);
  }
}
