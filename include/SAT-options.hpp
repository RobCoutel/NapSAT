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
 * - Add an entry in the SAT-options.cpp file "bool_options", "string_options" or "double_options" unordered_map.
 * - Add a comment above the field in this file with the following information:
 *   - @brief A description of the option.
 *   - @default The default value of the option for static (environment) options.
 * You also may:
 * - Add an alias to the option with @alias. The alias should also be entered in the unordered_maps in the SAT-options.cpp file.
 * - mention if an option is subsumed by another option with the tag @subsumed and the option that subsumes it.
 * - mention a warning that the user may need to know with the tag @warning.
 * - specific ranges of values or other constraints with the tag @requires.
 *
 * Finally, you should run the script/generate-option-documentation.py script from the root of the directory.
 */
#pragma once

#include <string>
#include <vector>

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
     * @brief Returns the directory of the manual pages.
     */
    static std::string get_man_page_folder();

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
     * @brief Sets the directory of the manual pages
     * @param dir The directory of the manual pages.
     */
    static void set_man_page_folder(std::string dir);

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

  class options {
  public:
    /** Start Documentation **/
    /** SOLVER BEHAVIOR **/
    /**
     * @brief Enables chronological backtracking. If enabled directly, the solver will use weak chronological backtracking. This option is not meant to be used standalone, but can be, in which case, wcb is used. This option is enabled by any chronological backtracking variant.
     * wcb => cb, rscb => cb, lscb => cb
     * There exists a hierarchy between the options. If several are enabled, the highest in the hierarchy will be used.
     * cb <= wcb < rscb < lscb
     * @alias -cb
     * @subsumed -wcb, -lscb and -rscb
     */
    bool chronological_backtracking = false;

    /**
     * @brief Enables the solver to use chronological backtracking.
     * @alias -wcb
     * @subsumed lscb and rscb
     * @warning This option is deprecated and will be removed in a future version.
     */
    bool weak_chronological_backtracking = false;

    /**
     * @brief Enables the solver to use restoring chronological backtracking.
     * That is, the solver will re-propagate literals that moved during backtracking.
     * @alias -rscb
     * @subsumed lscb
     */
    bool restoring_strong_chronological_backtracking = false;

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

    /**
     * @brief File containing the commands to be executed by the solver.
     * @requires interactive is on
     * @alias -commands
    */
    std::string commands_file = "";

    /**
     * @brief Folder where the solver will save the LaTeX files. (see $ NapSAT --help-navigation for more information)
     * @alias -s
    */
    std::string save_folder = "";

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
     * @brief Search all conflicts until the end of propagation before triggering conflict analysis.
     * @alias -ecr
     */
    bool exhaustive_conflict_repair = false;

    /**
     * @brief Enables partial conflict repair.
     * @alias -pcr
     */
    bool partial_conflict_repair = false;

    /**
     * @brief Limit on the number of conflicts before stopping the solve.
     * If set to a negative value, there is no limit.
     */
    double conflict_limit = -1.0;

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

    /** Stop Documentation **/
    // The tag above is used to generate the documentation of the options.

    /**
     * @brief Constructor
    */
    options() = default;

    /**
     * @brief Constructor
    */
    explicit options(std::vector<std::string>& tokens);
  };

} // namespace napsat
