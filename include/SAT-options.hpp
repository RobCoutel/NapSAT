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
    /**
     * @brief The directory of the manual pages. This option in general should not be set by the user, unless NapSAT is used as a library and the main program is not the NapSAT executable. NapSAT will find the manual pages folder if the option is not set. This option is only meant to be used by the user of the library.
     * @default [exec_dir]/../
     * @alias -m
     */
    static std::string man_page_folder;

    /**
     * @brief The directory of the invariant configurations. This option in general should not be set by the user, unless NapSAT is used as a library and the main program is not the NapSAT executable.
     * NapSAT will find the invariant configurations folder if the option is not set. However, the user can use this option to set their own configurations.
     * The invariants will only be used if the observer is active (-i, -o or -c).
     * @default [exec_dir]/../invariant-configurations/
     * @alias -icf
     */
    static std::string invariant_configuration_folder;

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
     * @brief Enables the observer to print statistics during, and at the end of the execution.
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
    options(std::vector<std::string>& tokens);
  };

} // namespace napsat
