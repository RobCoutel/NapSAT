/**
 * @file include/SAT-options.hpp
 * @author Robin Coutelier
 *
 * @brief This file is part of the NapSAT solver. It contains the runtime options of the SAT
 * solver.
 */

/**
 * HOW TO ADD NEW OPTIONS
 *
 * To add a new option, you must:
 * - Add a new field in the options class.
 * - Register it in options::build_option_parser (src/solver/SAT-options.cpp) via
 *   parser.add_bool/add_double/add_string(...), chaining .alias(...) for every CLI alias.
 * - Declare any fixed-priority incompatibility with .subsumes(...) and any AND-requirement on
 *   another option with .require(other, expected_value).
 * - Add a comment above the field in this file describing the option; the runtime -h/--help output
 *   is generated from the strings passed to build_option_parser, not from these comments, but they
 *   remain useful documentation for readers of this header.
 */
#pragma once

#include <string>
#include <vector>

#include "SAT-config.hpp"
#include "../src/utils/options.hpp"

namespace sentinel
{
  class Options;
}

namespace napsat
{

  class env
  {
    private:
    /* Note that the flags "[Start/Stop] Documentation" are used to generate the man page.*/
    /** Start Documentation **/
    /** GLOBAL ENVIRONMENT **/

    public:
    /** Stop Documentation **/
    /**
     * @brief Given a list of tokens, extract the environment variables. The function will return a the list of tokens without the environment variables.
     */
    static std::vector<std::string> extract_environment_variables(std::vector<std::string>& tokens);

    /**
     * @brief Returns the directory of the invariant configurations.
     */
    static std::string get_invariant_configuration_folder();

    /**
     * @brief Returns the directory of the obsidian template folder.
     */
    static std::string get_obsidian_template_folder();

    /**
     * @brief Returns true if the solver will not print warnings.
     */
    static bool get_suppress_warning();

    /**
     * @brief Returns true if the solver will not print information.
     */
    static bool get_suppress_info();


    /**
     * @brief Sets the directory of the invariant configurations.
     * @param dir The directory of the invariant configurations.
     */
    static void set_invariant_configuration_folder(std::string dir);

    /**
     * @brief Sets the directory of the obsidian template folder.
     * @param dir The directory of the obsidian template folder.
     */
    static void set_obsidian_template_folder(std::string dir);

    /**
     * @brief Sets the solver to suppress warnings.
     * @param sw True if the solver will not print warnings.
     */
    static void set_suppress_warning(bool sw);

    /**
     * @brief Sets the solver to suppress information.
     * @param si True if the solver will not print information.
     */
    static void set_suppress_info(bool si);
  };

  class Options {
  public:

    ::sentinel::Options* sentinel_options = nullptr;


    /** Start Documentation **/
    /** SOLVER BEHAVIOR **/
    /**
     * @brief Enables the solver to use chronological backtracking (RSCB).
     * That is, the solver will re-propagate literals that moved during backtracking.
     */
    bool chronological_backtracking = false;

    /**
     * @brief Enables the solver to use strong chronological backtracking. That is, the solver will use the lazy reimplication scheme.
     * @alias -scb
     */
    bool lazy_strong_chronological_backtracking = false;

    /**
     * @brief Enables the solver to use graph backtracking. That is, upon a conflict, the solver will only undo the literals that are reachable from the conflicting level in the implication graph. Further, the conflict level can be chosen by a heuristic, and does not necessarily have to be the highest level in the conflict clause.
     * @requires -cb, -wcb, -rscb and -lscb are false
     * @alias -gb
     */
    bool graph_backtracking = false;

    /**
     * @brief Enables the solver to log missed implications for decisions. When a decision is detected to be implied by a clause, if the chunk of that decision is backtracked, then, one of the chunks of the lazy reason clause needs to be backtracked as well.
     * @requires -gb is true
     * @alias -lcm
     */
    bool lazy_chunk_merging = false;

    /**
     * @brief Enables the solver to eagerly merge chunks when a missed implication is detected for a decision. That is, when a decision is detected to be implied by a clause, the chunks of the lazy reason clause are merged with the chunk of the decision right away.
     * @requires -gb is true and -lcm is false
     * @alias -ecm
     */
    bool eager_chunk_merging = false;

    /**
     * @brief Enables the solver to search the smallest UIP and choose the backtracked chunk accordingly.
     * @requires -gb is true, -bfc is false
     * @alias -bsc
     */
    bool backtrack_smallest_chunk = false;

    /**
     * @brief Enables the solver to backtrack the first chunk in the conflict clause.
     * @requires -gb is true, -bsc is false
     * @alias -bfc
     */
    bool backtrack_first_chunk = false;

    /**
     * @brief Enables the solver to delete learned clauses.
     * @alias -del
    */
    bool delete_clauses = true;

    /**
     * @brief If true, unused variables not be assigned a value.
     * @alias -iuv
     */
    bool ignore_unused_variables = false;

    /** OBSERVER **/
    /**
     * @brief Sets the solver to interactive mode. Before each decision, the solver will wait for the user to enter a command before continuing.
     * @warning The interactive mode will slow down the solver significantly.
     * @alias -i
    */
    bool interactive = false;

    /**
     * @brief Sets an observer to the solver. The observer will print information about the solver.
     * @subsumed -i
     * @warning The observer will slow down the solver significantly.
     * @alias -o
    */
    bool observing = false;

    /**
     * @brief Enables the solver to check some invariants through the observers.
     * @subsumed -o and -i
     * @warning Checking the invariants will slow down the solver significantly.
     * @alias -c
    */
    bool check_invariants = false;

    /**
     * @brief Enables the observer to print statistics at the end of the execution.
     * @requires observing or interactive is on
     * @alias -stat
     * @warning This option has a significant impact on the performance of the solver.
     * It should not be used when measuring the compute time of the solver. It is recommended
     * to run the solver twice, once with this option and once without it.
     */

    bool print_stats = false;

    /**
     * @brief Enables the observer to print the statistics during the execution.
     * @requires observing or interactive is on
     * @alias -live-stat
     * @warning This option has a significant impact on the performance of the solver.
     * It should not be used when measuring the compute time of the solver. It is recommended
     * to run the solver twice, once with this option and once without it.
     */
    bool print_live_stats = false;

    /**
     * @brief If true, the solver will save its state to an obsidian vault when failing an assertion.
      * @alias -ssi
     */
    bool save_state_on_interrupt = true;

    /**
     * @brief Enables resolution proof system to build a proof during the execution.
     * @alias -bp
    */
    bool build_proof = false;

    /**
     * @brief Enables the resolution proof system to check the proof during the execution.
     * @requires build_proof is on
     * @alias -cp
    */
    bool check_proof = false;

    /**
     * @brief Enables the observer to print the proof during the execution.
     * @requires build_proof is on
     * @alias -pp
    */
    bool print_proof = false;

    /**
     * @brief If true, for each learned clause, the solver will record on which input clauses it depends.
     * @details Note that if this option is enabled, deletion of input clauses is disabled (even when satisfied at level 0).
     * @warning This option must be enabled to produce clause UNSAT cores.
     */
    bool record_dependencies = false;


    /** VARIABLE ACTIVITY **/
    /**
     * @brief Decay factor of the _var_activity_increment.
     * @requires 0 < decay < 1
     */
    double var_activity_decay = 0.95;

    /** CLAUSE DELETION **/
    /**
     * @brief Multiplier of the number of clauses before elimination. When the simplifying procedure is called to eliminate clauses, the new threshold for deletion is multiplied by this multiplier.
     * @requires multiplier > 1
     */
    double clause_elimination_multiplier = 2.0;

    /**
     * @brief Multiplier for the activity increment of clauses. The higher the multiplier, faster the clauses are considered irrelevant.
     * @requires The multiplier must be greater than 1.
     */
    double clause_activity_multiplier = 1.001;
    /**
     * @brief Decay factor of the clause activity threshold. If the solver deletes too many clauses, the solver will not make progress. Therefore, the threshold is multiplied by the threshold decay factor upon each clause deletion. This hyper parameter must be set to a value lower than 1.
     * @requires 0 < decay < 1
     */
    double clause_activity_threshold_decay = 0.85;

    /**
     * @brief Enables luby restarts of the solver.
     */
    bool restarts = true;

    /**
     * @brief If this option is true, the solver will backtrack the chunks that were analyzed to learn clauses. Otherwise, the solver will always backtrack the smallest set of chunks, if possible.
     * @alias -bl
     * @requires -gb
     */
    bool backtrack_learned = false;

    /**
     * @brief If true, the solver will use an approximate cost estimation when calculating the weights of bitsets during conflict analysis.
     * @alias -max-approx-cost
     * @requires -gb and not use_sum_approximate_cost_estimation and not use_vsids_approximate_cost_estimation
     */
    bool use_max_approximate_cost_estimation = false;

    /**
     * @brief If true, the solver will use an alternative approximate cost estimation when calculating the weights of bitsets during conflict analysis.
     * @alias -sum-approx-cost
     * @requires -gb and not use_max_approximate_cost_estimation and not use_vsids_approximate_cost_estimation
     */
    bool use_sum_approximate_cost_estimation = false;

    /**
     * @brief If true, the solver will use an approximate cost estimation based on VSIDS activity when calculating the weights of bitsets during conflict analysis.
     * @alias -vsids-approx-cost
     * @requires -gb and not use_max_approximate_cost_estimation and not use_sum_approximate_cost_estimation
     */
    bool use_vsids_approximate_cost_estimation = false;

    /**
     * @brief Penalty for the level of chunks in the cost heuristic. The higher the penalty, the more the heuristic
     * will prefer to backtrack chunks at higher level.
     * @requires -gb
     */
    double chunk_level_penalty = 0.01;

    /**
     * @brief Limit on the number of backtrack possibilities to consider when using graph backtracking. If the number of possibilities is greater than the limit, the solver will heuristically cutoff.
     */
    double backtrack_possibilities_limit = 1000;

    /**
     * @brief Weight of synced variables in the utility heuristic.
     * @requires -gb
     * @details The utility heuristic is used to choose which chunk to backtrack when using graph backtracking.
     * The utility of a literal is defined as follows:
     * - If the variable is synced, the utility is sync_weight.
     * - Otherwise, the utility is 1.
     */
    double sync_weight = 8;

    /**
     * @brief Timeout of the solver in milliseconds. If the solver does not finish within this time, it will stop and return UNKNOWN.
     * @requires timeout > 0
     * @alias -t
     */
    double timeout = 5000;

    /**
     * @brief Number of conflicts before the solver exists with UNKNOWN.
     * If conflict-limit is set to -1, the option is ignored.
     * @alias -cl
     */
    double conflict_limit = -1;

    /** Stop Documentation **/
    // The tag above is used to generate the documentation of the options.

    /**
     * @brief Constructor
    */
    Options() = default;

    Options(const Options&) = delete;

    /**
     * @brief Constructor
    */
    explicit Options(std::vector<std::string>& tokens);

    /**
     * @brief Runtime-generated help text describing every solver option (and, nested within it,
     * every sentinel/observer option). Replaces the old scripts/generate-option-documentation.py
     * + man.txt pipeline.
     */
    static std::string get_help_text();

    /**
     * @brief Returns a string representation of the options, suitable for logging or debugging.
     */
    std::string get_options_string() const;

  private:
    /**
     * @brief Parser for the options.
     */
    OptionParser _parser;

       /**
     * @brief Registers every option of `target` (aliases, defaults, incompatibilities,
     * requirements) into `parser`. Shared by the constructor and get_help_text().
     */
    void build_option_parser();

    /**
     * @brief Extracts the brace-delimited group of tokens following -o/--observing (if any), e.g.
     * `-o {--gui -commands file.txt}`, stripping the group out of `tokens` in place and returning
     * its contents (braces stripped) for the sentinel option parser to consume.
     */
    static std::vector<std::string> extract_sentinel_tokens(std::vector<std::string>& tokens);
  };

} // namespace napsat
