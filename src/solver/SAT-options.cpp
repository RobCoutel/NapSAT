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

#include "../utils/options.hpp"
#include "../utils/printer.hpp"

#include <string>
#include <iostream>
#include <sstream>
#include <unordered_map>

using namespace std;



/**************************************************************************************************/
/*                                  GLOBAL ENVIRONMENT                                            */
/**************************************************************************************************/
namespace napsat
{
/**
 * @brief The directory of the invariant configurations. This option in general should not be set by the user, unless NapSAT is used as a library and the main program is not the NapSAT executable.
 * NapSAT will find the invariant configurations folder if the option is not set. However, the user can use this option to set their own configurations.
 * The invariants will only be used if the observer is active (-i, -o or -c).
 * @default [exec_dir]/../invariant-configurations/
 * @alias -icf
 */
static std::string invariant_configuration_folder = "../invariant-configurations/";

/**
 * @brief The directory of the obsidian template folder. This folder should contain a subfolder named ".obsidian" with the necessary configuration files for Obsidian to recognize the exported graph. This option is used when exporting the implication graph to an obsidian vault. If the template folder is not found, a warning will be printed and the exported graph may not be properly recognized by Obsidian.
 * @default [exec_dir]/../obsidian_template/
 */
static std::string obsidian_template_folder = "../obsidian_template/";

/**
 * @brief If true, the solver will not print warnings to the standard output.
 * @default off
 * @alias -sw
*/
static bool suppress_warning;

/**
 * @brief If true, the solver will not print information to the standard output.
 * @default off
 * @alias -si
*/
static bool suppress_info;

vector<string> env::extract_environment_variables(vector<string>& tokens) {
  static unordered_map<string, bool*> bool_options = {
    {"--suppress-warning", &suppress_warning},
    {"-sw",                &suppress_warning},
    {"--suppress-info",    &suppress_info},
    {"-si",                &suppress_info}
  };

  static unordered_map<string, string*> string_options = {
    {"--obsidian-template-folder",       &obsidian_template_folder},
    {"--invariant-configuration-folder", &invariant_configuration_folder},
    {"-icf",                             &invariant_configuration_folder}};

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

string env::get_invariant_configuration_folder() {
  return invariant_configuration_folder;
}

string env::get_obsidian_template_folder()
{
  return obsidian_template_folder;
}

void env::set_invariant_configuration_folder(string dir) {
  invariant_configuration_folder = dir;
}

void env::set_obsidian_template_folder(string dir) {
  obsidian_template_folder = dir;
}

bool env::get_suppress_warning() {
  return suppress_warning;
}

void env::set_suppress_warning(bool sw) {
  suppress_warning = sw;
}

bool env::get_suppress_info() {
  return suppress_info;
}

void env::set_suppress_info(bool si) {
  suppress_info = si;
}
}

/**************************************************************************************************/
/*                                    LOCAL OPTIONS                                               */
/**************************************************************************************************/

vector<string> napsat::options::extract_sentinel_tokens(vector<string>& tokens)
{
  vector<string> sentinel_tokens;
  for (unsigned i = 0; i < tokens.size(); i++) {
    if (tokens[i] != "-o" && tokens[i] != "--observing")
      continue;
    if (i + 1 >= tokens.size() || tokens[i + 1].empty() || tokens[i + 1][0] != '{')
      break;

    unsigned end = i + 1;
    bool closed = false;
    for (; end < tokens.size(); end++) {
      if (!tokens[end].empty() && tokens[end].back() == '}') {
        closed = true;
        break;
      }
    }
    if (!closed) {
      LOG_WARNING("unterminated sentinel option group after " << tokens[i] << "; the group is ignored.");
      break;
    }

    // Re-join the captured span with spaces and re-split on whitespace, rather than trusting the
    // shell's own word-splitting: an unquoted "{--gui -commands file}" is a parse error in zsh (it
    // reads {} as command-grouping syntax), so the documented/expected usage is to quote the whole
    // group (e.g. -o "{--gui -commands file}"), which the shell then hands us as a single token
    // containing embedded spaces. Escaping only the braces (\{--gui -commands file\}) still yields
    // the historical one-word-per-token layout; both are normalized here.
    string joined;
    for (unsigned j = i + 1; j <= end; j++) {
      if (j != i + 1)
        joined += ' ';
      joined += tokens[j];
    }
    joined = joined.substr(1, joined.size() - 2); // strip outer '{' and '}'

    istringstream iss(joined);
    string piece;
    while (iss >> piece)
      sentinel_tokens.push_back(piece);

    tokens.erase(tokens.begin() + i + 1, tokens.begin() + end + 1);
    break;
  }
  return sentinel_tokens;
}

void napsat::options::build_option_parser(napsat::options& t, napsat::OptionParser& p)
{
  p.set_category("SOLVER BEHAVIOR");
  auto& cb = p.add_bool("--chronological-backtracking", t.chronological_backtracking,
    "Enables chronological backtracking as described in \n  [2018 - Chronological Backtracking - Nadel and Ryvchin]").alias("-cb");
  auto& lscb = p.add_bool("--lazy-strong-chronological-backtracking", t.lazy_strong_chronological_backtracking,
    "Enables strong chronological backtracking with the lazy reimplication scheme as described in \n  [2024 - Lazy Reimplication in Chronological Backtracking - Coutelier et al.].")
    .alias("-lscb").alias("-scb");
  auto& gb = p.add_bool("--graph-backtracking", t.graph_backtracking,
    "Enables graph backtracking: upon a conflict, selects the lightest set of literals to be unassigned. As described in \n  [2026 - Generalizing CDCL with Graph Backtracking - Coutelier et al.]").alias("-gb");
  auto& lcm = p.add_bool("--lazy-chunk-merging", t.lazy_chunk_merging,
    "Logs missed implications for decisions so that chunks can be merged lazily when needed.")
    .alias("-lcm");
  auto& ecm = p.add_bool("--eager-chunk-merging", t.eager_chunk_merging,
    "Eagerly merges chunks as soon as a missed implication is detected for a decision.")
    .alias("-ecm");
  auto& bsc = p.add_bool("--backtrack-smallest-chunk", t.backtrack_smallest_chunk,
    "Searches the smallest UIP and chooses the backtracked chunk accordingly.").alias("-bsc");
  auto& bfc = p.add_bool("--backtrack-first-chunk", t.backtrack_first_chunk,
    "Backtracks the first chunk in the conflict clause.").alias("-bfc");
  p.add_bool("--delete-clauses", t.delete_clauses, "Enables deletion of learned clauses.")
    .alias("-del");
  p.add_bool("--ignore-unused-variables", t.ignore_unused_variables,
    "If true, unused variables are not assigned a value.")
    .alias("-iuv").alias("--ignore-unused_variables");

  p.set_category("OBSERVER");
  p.add_bool("--interactive", t.interactive,
    "Before each decision, the solver waits for a user command before continuing.").alias("-i");
  p.add_bool("--observing", t.observing,
    "Attaches an observer to the solver, which prints information about its execution. Sentinel "
    "options can be passed grouped in braces right after this flag, e.g. -o \"{--gui -commands "
    "file.txt}\" (quote the group, since an unquoted '{' is a syntax error in zsh).")
    .alias("-o");
  p.add_bool("--check-invariants", t.check_invariants,
    "Checks solver invariants through the observer.").alias("-c");
  p.add_bool("--save-state-on-interrupt", t.save_state_on_interrupt,
    "Saves the solver state to an obsidian vault when an assertion fails.").alias("-ssi");
  p.add_bool("--statistics", t.print_stats,
    "Prints statistics at the end of the execution.").alias("-stat");
  p.add_bool("--live-statistics", t.print_live_stats,
    "Prints statistics live during the execution.").alias("-live-stat");
  p.add_bool("--proof", t.build_proof, "Builds a resolution proof during the execution.")
    .alias("-bp");
  p.add_bool("--check-proof", t.check_proof, "Checks the resolution proof during the execution.")
    .alias("-cp");
  p.add_bool("--print-proof", t.print_proof, "Prints the resolution proof during the execution.")
    .alias("-pp");
  p.add_bool("--record-dependencies", t.record_dependencies,
    "For each learned clause, records which input clauses it depends on. Required to produce "
    "clause UNSAT cores; disables deletion of input clauses.");

  p.set_category("VARIABLE ACTIVITY");
  p.add_double("--var-activity-decay", t.var_activity_decay,
    "Decay factor of the variable activity increment.");

  p.set_category("CLAUSE DELETION");
  p.add_double("--clause-elimination-multiplier", t.clause_elimination_multiplier,
    "Multiplier of the clause-count threshold before elimination is triggered again.");
  p.add_double("--clause-activity-multiplier", t.clause_activity_multiplier,
    "Multiplier for the activity increment of clauses.");
  p.add_double("--clause-activity-threshold-decay", t.clause_activity_threshold_decay,
    "Decay factor of the clause activity threshold.").range(0.0, 1.0, /*fatal=*/true);
  p.add_bool("--restarts", t.restarts, "Enables Luby restarts.");
  auto& bl = p.add_bool("--backtrack-learned", t.backtrack_learned,
    "Backtracks the chunks that were analyzed to learn the clause, instead of always backtracking "
    "the smallest possible set of chunks.").alias("-bl");
  auto& max_c = p.add_bool("--use-max-approximate-cost-estimation", t.use_max_approximate_cost_estimation,
    "Uses an approximate max-based cost estimation when weighting bitsets during conflict analysis.")
    .alias("-max-approx-cost");
  auto& sum_c = p.add_bool("--use-sum-approximate-cost-estimation", t.use_sum_approximate_cost_estimation,
    "Uses an approximate sum-based cost estimation when weighting bitsets during conflict analysis.")
    .alias("-sum-approx-cost");
  auto& vsids_c = p.add_bool("--use-vsids-approximate-cost-estimation", t.use_vsids_approximate_cost_estimation,
    "Uses a VSIDS-activity-based approximate cost estimation when weighting bitsets during conflict "
    "analysis.").alias("-vsids-approx-cost");
  p.add_double("--chunk-level-penalty", t.chunk_level_penalty,
    "Penalty for the level of chunks in the cost heuristic used by graph backtracking.");
  p.add_double("--backtrack-possibilities-limit", t.backtrack_possibilities_limit,
    "Limit on the number of backtrack possibilities considered by graph backtracking before "
    "heuristically cutting off.");
  p.add_double("--sync-weight", t.sync_weight,
    "Weight of synced variables in the graph-backtracking utility heuristic.");
  p.add_double("--timeout", t.timeout,
    "Timeout of the solver in milliseconds.").alias("-t");
  p.add_double("--conflict-limit", t.conflict_limit,
    "Number of conflicts before the solver exits with UNKNOWN. -1 disables the limit.")
    .alias("-cl");

  /****************************************************************************/
  /**                          OPTION COMPATIBILITY                          **/
  /****************************************************************************/
  lscb.subsumes(cb);

  bl.require(gb, true);
  lcm.require(gb, true);
  ecm.require(gb, true);
  ecm.require(lcm, false);
  bsc.require(gb, true);
  bfc.require(gb, true);
  bfc.subsumes(bsc);

  max_c.require(gb, true);
  sum_c.require(gb, true);
  sum_c.require(max_c, false);
  vsids_c.require(gb, true);
  vsids_c.require(max_c, false);
  vsids_c.require(sum_c, false);
}

napsat::options::options(vector<string>& tokens)
{
  vector<string> sentinel_tokens = extract_sentinel_tokens(tokens);
  sentinel_options = sentinel::Options(sentinel_tokens);

  OptionParser parser;
  build_option_parser(*this, parser);
  parser.parse(tokens);
  parser.resolve();

  /****************************************************************************/
  /**                    DERIVED VALUES / MANUAL SPECIAL CASES               **/
  /****************************************************************************/
  chronological_backtracking |= lazy_strong_chronological_backtracking;

  if (graph_backtracking && chronological_backtracking) {
    LOG_WARNING("graph backtracking subsumes chronological backtracking.");
    LOG_WARNING("The solver will run with graph backtracking.");
    chronological_backtracking = false;
  }

  interactive |= !sentinel_options.commands_file.empty();

  build_proof = build_proof || print_proof || check_proof;

  if ((print_stats || print_live_stats) && !observing && !interactive) {
    LOG_WARNING("print-stats/print-live-stats requires observing or interactive mode to be enabled.");
    LOG_WARNING("The option is ignored.");
    print_stats = false;
    print_live_stats = false;
  }
}

string napsat::options::get_help_text()
{
  static const string title =
    "################################################################################\n"
    "#                                    NapSAT                                    #\n"
    "################################################################################\n";
  static const string usage =
    "NapSAT is a CDCL SAT solver supporting both chronological and non-chronological "
    "backtracking. The solver will print \"s SATISFIABLE\" if the problem has a solution, and "
    "\"s UNSATISFIABLE\" otherwise.\n\n"
    "By default, boolean options [on/off] are set [on] if the specification is omitted.\n\n"
    "Usage: NapSAT <input_file/-h/-hs/-hn> [options]\n\n"
    "  -h  or --help                 Print this helper\n";

  options dummy;
  OptionParser parser;
  build_option_parser(dummy, parser);
  return title
       + justify_string(usage, 80)
       + parser.help_text() + "\n"
       + sentinel::Options::get_help_text();
}
