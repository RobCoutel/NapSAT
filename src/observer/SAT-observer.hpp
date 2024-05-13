/**
 * @file src/observer/SAT-observer.hpp
 * @author Robin Coutelier
 *
 * @brief This file is part of the SAT solver NapSAT. It defines an
 * observer for the SAT solver. That is, a duplicate of the state of the solver.
 * It does not do any reasoning, but is used to check invariants and display the
 * state of the solver, while being able to navigate through the historyof the execution.
 */
#pragma once

#include "SAT-types.hpp"
#include "SAT-options.hpp"
#include "../display/SAT-display.hpp"
#include "SAT-notification.hpp"

#include <vector>
#include <functional>
#include <string>
#include <map>
#include <set>
#include <unordered_map>

namespace napsat::gui
{
  class notification;
  class display;
  class observer;
  class observer
  {
    // The notification have to be able to access private members of the observer to make the code more readable
    friend class notification;
    friend class new_variable;
    friend class delete_variable;
    friend class decision;
    friend class implication;
    friend class propagation;
    friend class remove_propagation;
    friend class conflict;
    friend class unassignment;
    friend class new_clause;
    friend class delete_clause;
    friend class checkpoint;
    friend class marker;
    friend class done;
    friend class watch;
    friend class unwatch;
    friend class block;
    friend class missed_lower_implication;
    friend class remove_lower_implication;

  public:
    /**
     * @brief Function type for the command parser. The command parser is called when the observer reaches a checkpoint and the user enters a command.
     * @param command The command to parse.
     * @return true if the command was successfully parsed, and false otherwise.
    */
    using command_parser = std::function<bool(std::string)>;

  private:
    /**
     * @brief Structure to represent a variable in the observer. It contains only the information that is necessary to display the state of the solver.
    */
    struct variable
    {
      variable() = default;
      variable(napsat::Tval value, napsat::Tlevel level, napsat::Tclause reason, bool active) : value(value), level(level), reason(reason), active(active) {}
      variable(const variable& other) : value(other.value), level(other.level), reason(other.reason), active(other.active) {}

      /**
       * @brief The truth value of the variable. It can be either true, false, or undefined.
       */
      napsat::Tval value = napsat::VAR_UNDEF;
      /**
       * @brief The decision level of the variable. /if the variable is not assigned, the decision level is \infty.
       */
      napsat::Tlevel level = napsat::LEVEL_UNDEF;
      napsat::Tclause reason = napsat::CLAUSE_UNDEF;
      napsat::Tclause lazy_reason = napsat::CLAUSE_UNDEF;
      bool active = false;
      bool propagated = false;
    };

    struct clause
    {
      clause() = default;
      clause(const std::vector<napsat::Tlit>& literals, napsat::Tclause cl, bool learnt, bool external) : literals(literals), cl(cl), learnt(learnt), external(external) {}
      clause(const clause& other) : literals(std::move(other.literals)), cl(other.cl), active(other.active), learnt(other.learnt), external(other.external) {}
      std::vector<napsat::Tlit> literals;
      napsat::Tclause cl;
      std::set<napsat::Tlit> watched;
      Tlit blocker = LIT_UNDEF;
      bool active = false;
      bool learnt = false;
      bool external = false;
    };

    napsat::options _options;

    std::vector<notification*> _notifications;

    unsigned _location = 0;

    bool _stopped = true;

    /**
     * Set of clauses that were added to the solver from the beginning.
     */
    std::map<long unsigned, clause*> _clauses_dict;

    /**
     * Set of clauses that are currently active.
     */
    std::vector<clause*> _active_clauses;

    /**
     * @brief Set of variables that were added to the solver from the beginning.
     * The index of the variable in the vector is its id.
     */
    std::vector<variable> _variables;

    /**
     * @brief Stack of assignments. This is the trail of the solver.
     */
    std::vector<napsat::Tlit> _assignment_stack;

    /**
     * @brief Number of propagated literals.
     * @pre We assume that the solver does not reorder the literals in the trail.
     */
    unsigned _n_propagated = 0;

    /**
     * @brief Highest decision level in the trail.
     */
    napsat::Tlevel _decision_level = 0;

    /**
     * @brief The display interface used to view the state of the solver.
     */
    napsat::gui::display* _display = nullptr;

    /**
     * @brief Hash function for clauses.
     */
    long unsigned hash_clause(const std::vector<napsat::Tlit>& literals);

    /**
     * @brief List of commands to be executed when the observer receives a checkpoint.
     */
    std::vector<std::string> _commands;

    /**
     * @brief Function of the solver used to parse commands
     */
    command_parser _command_parser = nullptr;

    /**
     * @brief Set of variables that are marked.
     */
    std::set<napsat::Tvar> _marked_variables;

    /**
     * @brief Set of clauses that are marked.
     */
    std::set<napsat::Tclause> _marked_clauses;

    /**
     * @brief Set of breakpoints.
     */
    std::set<unsigned> _breakpoints;

    /**
     * @brief If true, the observer will only check the invariants without displaying the execution.
     */
    bool _check_invariants_only = false;

    /**
     * @brief If true, the observer will only display the statistics and not simulate the execution.
     */
    bool _stats_only = false;

    /**
     * @brief Hash map to count the number of notifications of each type.
     */
    std::unordered_map<napsat::gui::ENotifType, unsigned> notification_count;

    /**
     * @brief Hash map to count the number of pure statistics notifications of each label.
    */
    std::unordered_map<std::string, unsigned> stat_count;

    /**
     * @brief Number of the next file to be written
     */
    unsigned file_number = 0;

  public:
    /**
     * @brief Construct a new observer object
     */
    observer(napsat::options& options);

    /**
     * @brief Clone an observer object
     * @details The clone is a deep copy of the observer.
     * @details The clone advances to the same location as the original observer.
     */
    observer(const observer& other);

    /**
     * @brief Send a notification to the observer and update its state.
     */
    void notify(notification* notification);

    /**
     * @brief Formats a string that contains the statistics of the run. That is, the number of notifications of each type.
     */
    std::string get_statistics();

    /**  OBSERVING EXECUTION  **/

    /**
     * @brief When in replay mode, move one step forward in the execution and return the level of the notification that was applied.
     * @return The level of the notification that was applied.
     * @pre The observer must not be in real time.
     */
    unsigned next();

    /**
     * @brief Go back to the previous notification and return it.
     */
    unsigned back();

    /**
     * @brief Returns the message of the last notification that was applied.
     */
    std::string last_message();

    unsigned notification_number() { return _location; }

    /**
     * @brief Returns true if the state of the observer corresponds to the last notification it received. In other words, returns true if the observer is in real time.
     */
    bool is_real_time() { return _location == _notifications.size(); }

    /**
     * @brief Returns true if the state of the observer corresponds to before the first notification it received. In other words, returns true if the observer is at the beginning of the execution.
     */
    bool is_back_to_origin() { return _location == 0; }

    /**
     * @brief Marks a variable. When this variable is involved in a notification, level of that notification becomes 0.
     * @note This function does not require the variable to already exist.
     * @details If the variable is deleted and then recreated, it will be marked in both cases.
     */
    void mark_variable(napsat::Tvar var);

    /**
     * @brief Marks a clause. When this clause is involved in a notification, level of that notification becomes 0.
     * @note This function does not require the clause to already exist.
     * @details If the clause is deleted and then recreated, it will be marked in both cases. If a future developer wants to change that behavior, they should use the hash of literals to identify clauses instead of their id. But for efficiency reasons, we use the id of the clause (which may not be unique).
     */
    void mark_clause(napsat::Tclause cl);

    /**
     * @brief Unmarks a variable. No special behavior will be applied to this variable.
     */
    void unmark_variable(napsat::Tvar var);

    /**
     * @brief Unmarks a clause. No special behavior will be applied to this clause.
     */
    void unmark_clause(napsat::Tclause cl);

    /**
     * @brief Returns true if the variable is marked.
     */
    bool is_variable_marked(napsat::Tvar var);

    /**
     * @brief Returns true if the clause is marked.
     */
    bool is_clause_marked(napsat::Tclause cl);

    /**
     * @brief Stops the execution of the observer at the notification with the id n_notifications.
     */
    void set_breakpoint(unsigned n_notifications);

    /**
     * @brief Removes the breakpoint.
    */
    void unset_breakpoint(unsigned n_notifications);

    /**  ALTERING EXECUTION  **/
    /**
     * @brief Notify that a checkpoint has been reached, waiting for a user input or a command from the file input.
     */
    void notify_checkpoint();

    /**
     * @brief Reads commands from a file and stores them in the observer.
     * When the "notify_checkpoint" command is reached, the observer will send one command to the solver and wait for the next checkpoint. When the command buffer is empty, the observer will get back to its normal behavior.
     */
    void load_commands(std::string filename);

    /**
     * @brief Set the command parser function.
     * The command parser function is called when the observer reaches a checkpoint and has a command to send to the solver. The function should return true if the command was successfully parsed, and false otherwise.
     * @param parser The command parser function.
     * @details if no checkpoint is sent, the command parser is not called and does not need to be set.
     */
    void set_command_parser(command_parser parser);

    /**
     * @brief Send a command to the solver through the command parser.
     * @param command The command to send.
     * @return true if the command was successfully parsed, and false otherwise.
     * @pre The command parser must be set.
     */
    bool transmit_command(std::string command);

    /**  Saving and loading executions  **/
    /**
     * @brief Opens a file with the given name. When notifications are received, they are written to the file.
     * @param filename The name of the file to write to.
     * @details Only the notifications that are received after the call to this function are written to the file.
     * @details If the filename is empty, the file is closed.
     */
    void toggle_save(std::string filename);

    /**
     * @pre Assumes that the observer is empty. Otherwise, the behavior is undefined.
     * @brief Loads a file and executes the commands in it.
     * @param filename The name of the file to load.
     * @details The file must contain a list of notifications that can be parsed by the observer.
     */
    void load_execution(std::string filename);

    /**  QUERYING INTERNAL STATE  **/
    /**
     * @brief Returns the truth value of a variable in the assignment.
     */
    napsat::Tval var_value(napsat::Tvar var);

    /**
     * @brief Returns the truth value of a literal in the assignment.
     */
    napsat::Tval lit_value(napsat::Tlit lit);

    /**
     * @brief Returns the decision level of a variable in the assignment.
     */
    napsat::Tlevel var_level(napsat::Tvar var);

    /**
     * @brief Returns the reason of a variable in the assignment.
     */
    napsat::Tclause var_reason(napsat::Tvar var);

    /**
     * @brief Returns true if the variable was propagated.
     */
    bool var_propagated(napsat::Tvar var);

    /**
     * @brief Returns the decision level of a literal in the assignment.
     */
    napsat::Tlevel lit_level(napsat::Tlit lit);

    /**
     * @brief Returns the reason of a literal in the assignment.
     */
    napsat::Tclause lit_reason(napsat::Tlit lit);

    /**
     * @brief Returns true if the literal was propagated.
     */
    bool lit_propagated(napsat::Tlit lit);

    /**
     * @brief Returns a reference to the assignment stack.
     */
    const std::vector<napsat::Tlit>& get_assignment();

    /**
     * @brief Returns the list of clauses that are currently active.
     */
    std::vector<std::pair<napsat::Tclause, const std::vector<napsat::Tlit>*>> get_clauses();

    /**  PRINTING  **/

    std::string lit_to_string(napsat::Tlit lit);

    std::string variable_to_string(napsat::Tvar var);

    static bool enable_sorting;

    void sort_clauses(napsat::Tclause cl);

    std::string clause_to_string(napsat::Tclause cl);

    /**
     * @brief Prints the clauses to the standard output of the terminal
    // TODO add tag for learned / external
    */
    void print_clause_set();

    /**
     * @brief
     */
    void print_deleted_clauses();

    /**
     * @brief Prints the complete partial assignment to the standard output
     */
    void print_assignment();

    /**
     * @brief Prints the set of variables with their polarity and reason for propagation to the standard output
     */
    void print_variables();

    /**
     * @brief Returns a LaTeX string to represent a literal.
     * @details The returned string does not contain the '$' bounding symbols
     */
    std::string literal_to_latex(Tlit lit, bool watched = false, bool blocked = false);

    /**
     * @brief Returns a LaTeX string to represent a literal. The compiled LaTeX will see all theses literals to have the same length on the resulting pdf.
     */
    std::string literal_to_aligned_latex(Tlit lit, bool watched = false, bool blocked = false);

    /**
     * @brief Returns a LaTeX string to represent a clause.
     * @details The clause is surrounded by the '$' bounding symbols if it is not undefined
     */
    std::string clause_to_latex(Tclause cl);

    /**
     * @brief Returns a LaTeX string to represent a clause. The length of each literal is the same length such that clauses can be aligned.
     * @details The clause is surrounded by the '$' bounding symbols if it is not undefined
     */
    std::string clause_to_aligned_latex(Tclause cl);

    /**
     * @brief Returns a LaTeX tikz picture representing the trail.
     */
    std::string trail_to_latex();

    /**
     * @brief Returns a LaTeX table containing all the clauses, the literals are aligned from one clause to the other.
     */
    std::string clause_set_to_latex();

    /**
     * @brief Returns a LaTeX table containing the set of clause used to propagate a literal.
     * @details Also prints the conflicting clauses
     */
    std::string used_clauses_to_latex();

    /**
     * @brief Returns a LaTeX tikz picture representing the implication graph of the trail.
     * @warning There are no guarantees on clipping. This function is likely to generate ugly diagrams if the example is ill-chosen
     */
    std::string implication_graph_to_latex();

    bool _recording = false;

    /**
     * @brief Saves the tikz trail and the clause set to the files pre-held in field fileTrail and fileClauses + some id. If _recording is false, then do nothing
     * @details The id is incremented by one.
     */
    void save_state();

    ~observer();

    /*************************************************************************/
    /*                        Invariant Checkers                             */
    /*************************************************************************/
  private:
    std::string _error_message = "";

    bool _check_trail_sanity = false;
    bool _check_level_ordering = false;
    bool _check_trail_monoticity = false;
    bool _check_no_missed_implications = false;
    bool _check_topological_order = false;
#if NOTIFY_WATCH_CHANGE
    bool _check_strong_watch_literals = false;
    bool _check_blocked_watch_literals = false;
    bool _check_weak_watch_literals = false;
    bool _check_weak_blocked_watch_literals = false;
    bool _check_watch_literals_levels = false;
#endif
    bool _check_assignment_coherence = false;

  public:
    /**
     * @brief Returns the concatenetaion of all error messages since the last call to this function.
     */
    std::string get_error_message() { return _error_message; }

    /**
     * @brief Enables the invariant checker for trail sanity.
     * @details the file must contain a list of invariant name separated by a newline.
     */
    void load_invariant_configuration();

    /**
     * @brief Checks all the enabled invariants of the observer.
     */
    bool check_invariants();

    /**
     * @brief If this feature is on, the observer will only check the invariants without displaying the execution.
     */
    void toggle_checking_only(bool on) { _check_invariants_only = on; }

    /**
     * @brief If this feature is on, the observer will only display the statistics and not simulate the execution.
     */
    void toggle_stats_only(bool on) { _stats_only = on; }

    /**
     * @brief Returns true if the observer is only checking the invariants.
     */
    bool is_checking_only() { return _check_invariants_only; }

    /**
     * @brief Checks the enabled invariants of the observer which may be affected by the notification.
     */
    bool check_relevant_invariant(napsat::gui::notification* notification);

    /**
     * @brief Checks that the propagated literals do not falsify any clause.
     * @details Let T be the set of propagated literals. check_propagated_literals returns true if and only if for all clause C in the clause set, there exists a literal l in C such that ~l is not in T.
     */
    bool check_trail_sanity();

    /**
     * @brief Checks that literals in the partial assignment are implied at the correct level.
     * @details Let T be the set of propagated literals, and W be the set of literals in the propagation queue. check_propagated_literals returns true if and only if for all literal l in T union W, level(l) = max_{l' in C} level(l) where C is the reason of l.
     */
    bool check_level_ordering();

    /**
     * @brief Checks that the level of literals in the trail trail is monotonically increasing.
     * @details Let T be the set of propagated literals. check_trail_monoticity returns true if and only if for all i in [0, |T| - 2], level(T[i]) <= level(T[i+1]).
     */
    bool check_trail_monoticity();

    /**
     * @brief Checks that no literal can be implied by a clause and propagated literals. In other words, checks that no clause is unit under the partial assignement of propgated literals unless it is satisfied by the entire assignment.
     * @details Let T be the set of propagated literals, and W be the set of literals in the propagation queue. check_propagated_literals returns true if and only if for all clause C in the clause set, if there exists a literal l in C such that C \ {l} is falsified by T, then l is satisfied by T union W.
     */
    bool check_no_missed_implications();

    /**
     * @brief Checks that the trail is a topological order of the implication graph.
     * @details Let P be the partial assignement. check_topological_order returns true if and only if for all literal i in {0, ..., |P| - 1}, for all literal l in the reason of P[i], there exists j such that P[j] = l and j < i.
     */
    bool check_topological_order();
#if NOTIFY_WATCH_CHANGES
    /**
     * @brief Checks that if a literal is falsified by a propagated literal, then it the other watched literal is satisfied by the partial assignment.
     * @details Let T be the set of propagated literals, and W be the set of literals in the propagation queue. check_watch_literals returns true if and only if for all clause C in the clause set watched by l1 and l2, if ~l1 in T or ~l2 in T, then l1 or l2 is satisfied by T union W.
     */
    bool check_strong_watch_literals();

    /**
     * @brief Checks that if a watched literal is falsified by a propagated literals, then the clause is satisfied by the partial assignment.
     */
    bool check_blocked_watch_literals();

    /**
     * @brief Checks that, if a watched literals literal is falsified by a propagated literal, then the other watched literal is satisfied at a lower level than the falsified literal.
     */
    bool check_weak_watch_literals();

    /**
     * @brief Checks that, if a watched literals literal is falsified by a propagated literal, then the clause is satisfied at a lower level than the falsified literal.
     */
    bool check_weak_blocked_watch_literals();
#endif

    /**
     * @brief Checks that no variable is assigned twice in the propagation queue.
     * Also checks that all the reasons for propagations are unisat
     */
    bool check_assignment_coherence();
  };
}
