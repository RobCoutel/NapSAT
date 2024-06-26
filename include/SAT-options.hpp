/**
 * @file include/SAT-options.hpp
 * @author Robin Coutelier
 *
 * @brief This file is part of the NapSAT solver. It contains the runtime options of the SAT
 * solver.
 */
#pragma once

#include <string>

namespace napsat
{
  class options {
  public:
    /** SOLVER BEHAVIOR **/
    /**
     * @brief Enables chronological backtracking. If enabled directly, the solver will use weak chronological backtracking. This option is not meant to be used standalone, but can be, in which case, wcb is used. This option is enabled by any chronological backtracking variant.
     * wcb => cb, rscb => cb, lscb => cb
     * There exists a hierarchy between the options. If several are enabled, the highest in the hierarchy will be used.
     * cb <= wcb < rscb < lscb
     * @alias -cb
     * @subsumedby weak-chronological-backtracking, lazy-strong-chronological-backtracking and restoring-strong-chronological-backtracking
     */
    bool chronological_backtracking = false;

    /**
     * @brief Enables the solver to use chronological backtracking.
     * @alias -cb
     * @subsumedby lazy-strong-chronological-backtracking and restoring-strong-chronological-backtracking
     */
    bool weak_chronological_backtracking = false;
    /**
     * @brief Enables the solver to use restoring chronological backtracking.
     * That is, the solver will re-propagate literals that moved during backtracking.
     * @alias -rscb
     * @subsumedby lazy-strong-chronological-backtracking
     */
    bool restoring_strong_chronological_backtracking = false;
    /**
     * @brief Enables the solver to use strong chronological backtracking. That is, the solver will use the lazy reimplication scheme.
     * @alias -scb
     */
    bool lazy_strong_chronological_backtracking = false;

    /**
     * @brief Enables the solver to delete learned clauses.
     * @alias -del
    */
    bool delete_clauses = true;

    /** OBSERVER **/
    /**
     * @brief If true, the observer will not print warnings.
     * @requires observing, interactive, check_invariants or print_stats is on
     * @alias -sw
    */
    bool suppress_warning = false;
    /**
     * @brief Sets the solver to interactive mode. Before each decision, the solver will wait for the user to enter a command before continuing.
     * @alias -i
    */
    bool interactive = false;
    /**
     * @brief Sets an observer to the solver. The observer will print information about the solver.
     * @warning The observer will slow down the solver significantly.
     * @alias -o
    */
    bool observing = false;
    /**
     * @brief Enables the solver to check some invariants through the observers.
     * @subsumedby observing and interactive
     * @alias -c
    */
    bool check_invariants = false;
    /**
     * @brief Enables the observer to print statistics at the end of the execution.
     * @requires observing or interactive is on
     * @alias -stat
    */
    bool print_stats = false;
    /**
     * @brief Enables the observer to build a proof during the execution.
     * @alias -bp
    */
    bool build_proof = false;
    /**
     * @brief Enables the observer to check the proof during the execution.
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
    double clause_elimination_multiplier = 1.5;

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

    /** RESTARTS **/
    /**
     * @brief Decay factor the of moving average of the agility.
     * @requires 0 < decay < 1
     */
    double agility_decay = 0.9999;

    /**
     * @brief Threshold of the agility. If the agility is lower than the threshold, the solver restarts and the the threshold is multiplied by agility_threshold_decay.
     * @requires 0 < threshold < 1
     */
    double agility_threshold = 0.4;

    /**
      * @brief Multiplier for the agility threshold. Since formulas can be very different, the agility for one problem may not be the same as the agility for another problem. Therefore, the threshold is multiplied by the threshold multiplier upon each implication. This hyper parameter must be set to a value greater than 1.
      * @requires multiplier >= 1
      */
    double threshold_multiplier = 1;
    /**
     * @brief Decay factor of the threshold. If the solver restarts too often, the solver will not make progress. Therefore, the threshold is multiplied by the threshold decay factor upon each restart. This hyper parameter must be set to a value lower than 1 and lower than 2 - threshold_multiplier.
     * @requires 0 < decay < 1
     */
    double agility_threshold_decay = 1;

    /** Stop Documentation **/
    // The tag above is used to generate the documentation of the options.

    /**
     * @brief Constructor
    */
    options() = default;

    /**
     * @brief Constructor
    */
    options(char** tokens, unsigned n_tokens);
  };

} // namespace napsat
