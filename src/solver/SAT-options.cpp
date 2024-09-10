/**
 * @file src/solver/SAT-options.cpp
 * @author Robin Coutelier
 *
 * @brief This file is part of the NapSAT solver. It contains the option parser for the
 * standalone SAT solver.
 */
#include "SAT-options.hpp"

#include "../utils/printer.hpp"
#include "../observer/SAT-notification.hpp"

#include <string>
#include <iostream>
#include <unordered_set>
#include <unordered_map>

using namespace std;
using namespace napsat::env;

/**************************************************************************************************/
/*                                  GLOBAL ENVIRONMENT                                            */
/**************************************************************************************************/
static unordered_map<string, bool*> bool_options = {
  {"--suppress-warning", &suppress_warning},
  {"-sw", &suppress_warning},
  {"--suppress-info", &suppress_info},
  {"-si", &suppress_info}
};

static unordered_map<string, string*> string_options = {
  {"--man-page-folder", &man_page_folder},
  {"-m", &man_page_folder},
  {"--invariant-configuration-folder", &invariant_configuration_folder},
  {"-icf", &invariant_configuration_folder}};


vector<string> napsat::env::extract_environment_variables(vector<string>& tokens) {
  vector<string> to_return;

  unsigned n_tokens = tokens.size();
  for (unsigned i = 0; i < n_tokens; i++) {
    string token = string(tokens[i]);
    string next_token = (i + 1 < n_tokens) ? string(tokens[i + 1]) : "";

    if(bool_options.find(token) != bool_options.end()) {
      if (next_token != "" && next_token[0] != '-') {
        if (next_token == "on") {
          *bool_options[token] = true;
        }
        else if (next_token == "off") {
          *bool_options[token] = false;
        }
        else {
          LOG_WARNING("option " << token << " requires a boolean value (on/off).");
          LOG_WARNING("Default value " << (*bool_options[token] ? "on" : "off") << " is used.");
          continue;
        }
      }
      else
        *bool_options[token] = true;
    }
    else if (string_options.find(token) != string_options.end()) {
      if (next_token == "" || next_token[0] == '-') {
        LOG_WARNING("option " << token << " requires a string value.");
        LOG_WARNING("The option is ignored.");
        continue;
      }
      *string_options[token] = next_token;
      i++;
    }
    else {
      to_return.push_back(token);
    }
  }
  return to_return;
}

string napsat::env::get_man_page_directory() {
  return man_page_folder;
}

string napsat::env::get_invariant_configuration_folder() {
  return invariant_configuration_folder;
}

void napsat::env::set_man_page_directory(string dir) {
  man_page_folder = dir;
}

void napsat::env::set_invariant_configuration_folder(string dir) {
  invariant_configuration_folder = dir;
}

bool napsat::env::get_suppress_warning() {
  return suppress_warning;
}

void napsat::env::set_suppress_warning(bool sw) {
  suppress_warning = sw;
}

bool napsat::env::get_suppress_info() {
  return suppress_info;
}

void napsat::env::set_suppress_info(bool si) {
  suppress_info = si;
}


/**************************************************************************************************/
/*                                    LOCAL OPTIONS                                               */
/**************************************************************************************************/

napsat::options::options(vector<string>& tokens)
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

  unsigned n_tokens = tokens.size();
  for (unsigned i = 0; i < n_tokens; i++) {
    string token = string(tokens[i]);
    string next_token = (i + 1 < n_tokens) ? string(tokens[i + 1]) : "";

    if (set_options.find(token) != set_options.end()) {
      LOG_WARNING("option " << token << " already set. The second occurrence is ignored.");
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
          LOG_WARNING("option " << token << " requires a boolean value (on/off).");
          LOG_WARNING("Default value " << (*bool_options[token] ? "on" : "off") << " is used.");
          continue;
        }
      }
      else
        *bool_options[token] = true;
      set_options.insert(token);
    }
    else if (double_options.find(token) != double_options.end()) {
      if (next_token == "" || next_token[0] == '-') {
        LOG_WARNING("option " << token << " requires a value (floating point number).");
        LOG_WARNING("Default value " << *double_options[token] << " is used.");
        continue;
      }
      try {
        *double_options[token] = stod(next_token);
        set_options.insert(token);
        i++;
      }
      catch (const std::invalid_argument& ia) {
        LOG_WARNING("option " << token << " requires a floating point number value.");
        LOG_WARNING("Default value " << *double_options[token] << " is used.");
        continue;
      }
    }
    else if (string_options.find(token) != string_options.end()) {
      if (next_token == "" || next_token[0] == '-') {
        LOG_WARNING("option " << token << " requires a value (string of characters).");
        LOG_WARNING("Options is ignored.");
        continue;
      }
      *string_options[token] = next_token;
      set_options.insert(token);
      i++;
    }
    else {
      LOG_WARNING("Unknown option " << token);
    }
  }

  /****************************************************************************/
  /**                          OPTION COMPATIBILITY                          **/
  /****************************************************************************/
  if (lazy_strong_chronological_backtracking && restoring_strong_chronological_backtracking) {
    LOG_WARNING("lazy strong chronological backtracking subsumes restoring strong chronological backtracking.");
    LOG_WARNING("The solver will run with lazy strong chronological backtracking.");
    restoring_strong_chronological_backtracking = false;
  }
  if (lazy_strong_chronological_backtracking && weak_chronological_backtracking) {
    LOG_WARNING("lazy strong chronological backtracking subsumes weak chronological backtracking.");
    LOG_WARNING("The solver will run with lazy strong chronological backtracking.");
    weak_chronological_backtracking = false;
  }
  if (restoring_strong_chronological_backtracking && weak_chronological_backtracking) {
    LOG_WARNING("restoring strong chronological backtracking subsumes weak chronological backtracking.");
    LOG_WARNING("The solver will run with restoring strong chronological backtracking.");
    weak_chronological_backtracking = false;
  }
  chronological_backtracking = weak_chronological_backtracking || restoring_strong_chronological_backtracking || lazy_strong_chronological_backtracking;

  interactive |= commands_file != "";

  if (clause_activity_threshold_decay <= 0 || clause_activity_threshold_decay >= 1) {
    LOG_ERROR("clause activity threshold decay must be between 0 and 1.");
    exit(1);
  }

  build_proof = build_proof || print_proof || check_proof;
}
