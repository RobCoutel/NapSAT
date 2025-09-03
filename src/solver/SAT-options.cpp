/*
 * This file is part of the source code of the software program
 * NapSAT. It is protected by applicable copyright laws.
 *
 * This source code is protected by the terms of the MIT License.
 */
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

std::string napsat::env::man_page_folder = "../";
std::string napsat::env::invariant_configuration_folder = "../invariant-configurations/";
bool napsat::env::suppress_warning = false;
bool napsat::env::suppress_info = false;


/**************************************************************************************************/
/*                                  GLOBAL ENVIRONMENT                                            */
/**************************************************************************************************/
vector<string> napsat::env::extract_environment_variables(vector<string>& tokens) {
  static unordered_map<string, bool*> bool_options = {
    {"--suppress-warning", &napsat::env::suppress_warning},
    {"-sw",                &napsat::env::suppress_warning},
    {"--suppress-info",    &napsat::env::suppress_info},
    {"-si",                &napsat::env::suppress_info}
  };

  static unordered_map<string, string*> string_options = {
    {"--man-page-folder",                &napsat::env::man_page_folder},
    {"-m",                               &napsat::env::man_page_folder},
    {"--invariant-configuration-folder", &napsat::env::invariant_configuration_folder},
    {"-icf",                             &napsat::env::invariant_configuration_folder}};

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

string napsat::env::get_man_page_folder() {
  return man_page_folder;
}

string napsat::env::get_invariant_configuration_folder() {
  return invariant_configuration_folder;
}

void napsat::env::set_man_page_folder(string dir) {
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
    {"-cb",                                      &chronological_backtracking},
    {"--chronological-backtracking",             &chronological_backtracking},
    {"-wcb",                                     &weak_chronological_backtracking},
    {"--weak-chronological-backtracking",        &weak_chronological_backtracking},
    {"-rscb",                                    &restoring_strong_chronological_backtracking},
    {"--restoring-chronological-backtracking",   &restoring_strong_chronological_backtracking},
    {"-lscb",                                    &lazy_strong_chronological_backtracking},
    {"--lazy-strong-chronological-backtracking", &lazy_strong_chronological_backtracking},
    {"-gb",                                      &graph_backtracking},
    {"--graph-backtracking",                     &graph_backtracking},
    {"-bsc",                                     &backtrack_smallest_chunk},
    {"-lcm",                                     &lazy_chunk_merging},
    {"--lazy-chunk-merging",                     &lazy_chunk_merging},
    {"--backtrack-smallest-chunk",               &backtrack_smallest_chunk},
    {"-bfc",                                     &backtrack_first_chunk},
    {"--backtrack-first-chunk",                  &backtrack_first_chunk},
    {"-o",                                       &observing},
    {"--observing",                              &observing},
    {"-i",                                       &interactive},
    {"--interactive",                            &interactive},
    {"-c",                                       &check_invariants},
    {"--check-invariants",                       &check_invariants},
    {"-stat",                                    &print_stats},
    {"--statistics",                             &print_stats},
    {"-live-stat",                               &print_live_stats},
    {"--live-statistics",                        &print_live_stats},
    {"-del",                                     &delete_clauses},
    {"--delete-clauses",                         &delete_clauses},
    {"-bp",                                      &build_proof},
    {"--proof",                                  &build_proof},
    {"-pp",                                      &print_proof},
    {"--print-proof",                            &print_proof},
    {"-cp",                                      &check_proof},
    {"--check-proof",                            &check_proof},
    {"--ignore-unused_variables",                &ignore_unused_variables},
    {"-iuv",                                     &ignore_unused_variables},
    {"--no-restart",                             &no_restart}
  };

  /**
   * @brief map of double options that can be set with a string.
  */
  std::unordered_map<string, double*> double_options = {
    {"--clause-elimination-multiplier",   &clause_elimination_multiplier},
    {"--clause-activity-multiplier",      &clause_activity_multiplier},
    {"--clause-activity-threshold-decay", &clause_activity_threshold_decay},
    {"--var-activity-decay",              &var_activity_decay},
    {"--conflict-penalty",                &conflict_penalty},
    {"--decision-activity-decay",         &decision_activity_decay}
  };

  /**
   * @brief map of string options that can be set with a string.
  */
  std::unordered_map<string, string*> string_options = {
    {"-s",             &save_folder},
    {"--save",         &save_folder},
    {"-commands",      &commands_file},
    {"--command-file", &commands_file}
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
        i++;
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
  if (weak_chronological_backtracking) {
    LOG_WARNING("weak chronological backtracking is deprecated and will be removed in a future version.");
    LOG_WARNING("Please use restoring strong chronological backtracking instead.");
    LOG_INFO("Switching to restoring strong chronological backtracking.");
    restoring_strong_chronological_backtracking = true;
    weak_chronological_backtracking = false;
  }

  chronological_backtracking = weak_chronological_backtracking || restoring_strong_chronological_backtracking || lazy_strong_chronological_backtracking;

  interactive |= commands_file != "";

  if (graph_backtracking && chronological_backtracking) {
    LOG_WARNING("graph backtracking subsumes chronological backtracking.");
    LOG_WARNING("The solver will run with graph backtracking.");
    chronological_backtracking = false;
  }

  if (backtrack_smallest_chunk && !graph_backtracking) {
    LOG_WARNING("backtrack smallest chunk requires graph backtracking.");
    LOG_WARNING("The solver will ignore this option.");
    backtrack_smallest_chunk = false;
  }

  if (backtrack_first_chunk && !graph_backtracking) {
    LOG_WARNING("backtrack first chunk requires graph backtracking.");
    LOG_WARNING("The solver will ignore this option.");
    backtrack_first_chunk = false;
  }

  if (backtrack_first_chunk && backtrack_smallest_chunk) {
    LOG_WARNING("backtrack first chunk subsumes backtrack smallest chunk.");
    LOG_WARNING("The solver will run with backtrack first chunk.");
    backtrack_smallest_chunk = false;
  }

  if (clause_activity_threshold_decay <= 0 || clause_activity_threshold_decay >= 1) {
    LOG_ERROR("clause activity threshold decay must be between 0 and 1.");
    exit(1);
  }

  build_proof = build_proof || print_proof || check_proof;

  exhaustive_conflict_search |= graph_backtracking;
}
