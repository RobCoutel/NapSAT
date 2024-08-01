/**
 * @file src/solver/SAT-options.cpp
 * @author Robin Coutelier
 *
 * @brief This file is part of the NapSAT solver. It contains the option parser for the
 * standalone SAT solver.
 */
#include "SAT-options.hpp"

#include "../observer/SAT-notification.hpp"

#include <string>
#include <iostream>
#include <unordered_set>
#include <unordered_map>

using namespace std;

napsat::options::options(char** tokens, unsigned n_tokens)
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
    {"-wcb", &weak_chronological_backtracking},
    {"--weak-chronological-backtracking", &weak_chronological_backtracking},
    {"-lscb", &lazy_strong_chronological_backtracking},
    {"--lazy-strong-chronological-backtracking", &lazy_strong_chronological_backtracking},
    {"-rscb", &restoring_strong_chronological_backtracking},
    {"--restoring-chronological-backtracking", &restoring_strong_chronological_backtracking},
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
    {"--delete-clauses", &delete_clauses},
    {"-bp", &build_proof},
    {"--proof", &build_proof},
    {"-pp", &print_proof},
    {"--print-proof", &print_proof},
    {"-cp", &check_proof},
    {"--check-proof", &check_proof},
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
    {"-commands", &commands_file},
    {"--command_file", &commands_file}
  };



  for (unsigned i = 0; i < n_tokens; i++) {
    string token = string(tokens[i]);
    string next_token = (i + 1 < n_tokens) ? string(tokens[i + 1]) : "";

    if (set_options.find(token) != set_options.end()) {
      cerr << "\033[0;33mWARNING\033[0m: option " << token << " already set. The second occurrence is ignored." << endl;
      continue;
    }
    if (bool_options.find(token) != bool_options.end()) {
      if (next_token != "" && next_token[0] != '-') {
        if (next_token == "on") {
          *bool_options[token] = true;
        }
        else if (next_token == "off") {
          *bool_options[token] = false;
        }
        else {
          cerr << "Error: option " << token << " requires a boolean value (on/off)." << endl;
          cerr << "Default value " << (*bool_options[token] ? "on" : "off") << " is used." << endl;
          continue;
        }
      }
      else
        *bool_options[token] = true;
      set_options.insert(token);
    }
    else if (double_options.find(token) != double_options.end()) {
      if (next_token == "" || next_token[0] == '-') {
        cerr << "Error: option " << token << " requires a value (floating point number)." << endl;
        cerr << "Default value " << *double_options[token] << " is used." << endl;
        continue;
      }
      try {
        *double_options[token] = stod(next_token);
        set_options.insert(token);
        i++;
      }
      catch (const std::invalid_argument& ia) {
        cerr << "Error: option " << token << " requires a floating point number value." << endl;
        cerr << "Default value " << *double_options[token] << " is used." << endl;
        continue;
      }
    }
    else if (string_options.find(token) != string_options.end()) {
      if (next_token == "" || next_token[0] == '-') {
        cerr << "Error: option " << token << " requires a value (string of characters)." << endl;
        cerr << "Options is ignored." << endl;
        continue;
      }
      *string_options[token] = next_token;
      set_options.insert(token);
      i++;
    }
    else {
      cerr << "\033[0;33mWARNING\033[0m: unknown option " << token << endl;
    }
  }

  /****************************************************************************/
  /**                          OPTION COMPATIBILITY                          **/
  /****************************************************************************/
  if (lazy_strong_chronological_backtracking && restoring_strong_chronological_backtracking) {
    cerr << "\033[0;33mWARNING\033[0m: lazy strong chronological backtracking subsumes restoring strong chronological backtracking." << endl;
    cerr << "The solver will run with lazy strong chronological backtracking." << endl;
    restoring_strong_chronological_backtracking = false;
  }
  if (lazy_strong_chronological_backtracking && weak_chronological_backtracking) {
    cerr << "\033[0;33mWARNING\033[0m: lazy strong chronological backtracking subsumes weak chronological backtracking." << endl;
    cerr << "The solver will run with lazy strong chronological backtracking." << endl;
    weak_chronological_backtracking = false;
  }
  if (restoring_strong_chronological_backtracking && weak_chronological_backtracking) {
    cerr << "\033[0;33mWARNING\033[0m: restoring strong chronological backtracking subsumes weak chronological backtracking." << endl;
    cerr << "The solver will run with restoring strong chronological backtracking." << endl;
    weak_chronological_backtracking = false;
  }
  chronological_backtracking = weak_chronological_backtracking || restoring_strong_chronological_backtracking || lazy_strong_chronological_backtracking;

  interactive |= commands_file != "";

  if (suppress_warning && !observing && !interactive && !check_invariants) {
    cerr << "\033[0;33mWARNING\033[0m: suppress warning requires observing. The options is ignored" << endl;
  }
  else if (suppress_warning) {
    napsat::gui::notification::suppress_warning(true);
  }
  // if (print_stats && !observing && !interactive && !check_invariants) {
  //   cerr << "\033[0;33mWARNING\033[0m: print stats requires observing or interactive. The option is ignored." << endl;
  // }
  if (clause_activity_threshold_decay <= 0 || clause_activity_threshold_decay >= 1) {
    cerr << "Error: clause activity threshold decay must be between 0 and 1." << endl;
    exit(1);
  }

  build_proof = build_proof || print_proof || check_proof;
}
