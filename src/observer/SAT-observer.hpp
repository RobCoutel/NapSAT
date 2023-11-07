#pragma once

#include "../solver/SAT-types.hpp"
#include "../display/SAT-display.hpp"
#include "SAT-notification.hpp"

#include <vector>
#include <map>
#include <functional>
#include <string>

namespace sat::gui
{
  class notification;
  class display;
  class observer
  {
    friend class notification;
    friend class new_variable;
    friend class delete_variable;
    friend class decision;
    friend class implication;
    friend class propagation;
    friend class conflict;
    friend class unassignment;
    friend class new_clause;
    friend class delete_clause;
    friend class checkpoint;
    friend class marker;

  public:
    using command_parser = std::function<bool (std::string)>;

  private:
    struct variable
    {
      variable(sat::Tval value, sat::Tlevel level, sat::Tclause reason, bool active) : value(value), level(level), reason(reason), active(active) {}
      variable() = default;

      sat::Tval value = sat::VAR_UNDEF;
      sat::Tlevel level = sat::LEVEL_UNDEF;
      sat::Tclause reason = sat::CLAUSE_UNDEF;
      bool active = false;
    };

    struct clause
    {
      clause() = default;
      clause(const std::vector<sat::Tlit> &literals, sat::Tclause cl, bool learnt, bool external) : literals(literals), cl(cl), learnt(learnt), external(external) {}
      std::vector<sat::Tlit> literals;
      sat::Tclause cl;
      bool active = false;
      bool learnt = false;
      bool external = false;
    };

    std::vector<notification *> _notifications;

    unsigned _location = 0;

    bool _stopped = true;

    /**
     * Set of clauses that were added to the solver from the beginning.
     */
    std::map<long unsigned, clause *> _clauses_dict;

    /**
     * Set of clauses that are currently active.
     */
    std::vector<clause *> _active_clauses;

    /**
     * @brief Set of variables that were added to the solver from the beginning.
     * The index of the variable in the vector is its id.
    */
    std::vector<variable> _variables;

    /**
     * @brief Stack of assignments. This is the trail of the solver.
    */
    std::vector<sat::Tlit> _assignment_stack;

    /**
     * @brief Number of propagated literals.
     * @pre We assume that the solver does not reorder the literals in the trail.
    */
    unsigned _n_propagated = 0;

    /**
     * @brief Highest decision level in the trail.
    */
    sat::Tlevel _decision_level = 0;

    /**
     * @brief The display interface used to view the state of the solver.
    */
    sat::gui::display *_display = nullptr;

    /**
     * @brief Hash function for clauses.
    */
    long unsigned hash_clause(const std::vector<sat::Tlit> &literals);

    /**
     * @brief List of commands to be executed when the observer receives a checkpoint.
    */
    std::vector<std::string> _commands;

    /**
     * @brief Function of the solver used to parse commands
    */
    command_parser _command_parser = nullptr;

  public:
    /**
     * @brief Construct a new observer object
    */
    observer();

    /**
     * @brief Send a notification to the observer and update its state.
    */
    void notify(sat::gui::notification *notification);

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

    /**
     * @brief Returns true if the state of the observer corresponds to the last notification it received. In other words, returns true if the observer is in real time.
    */
    bool is_real_time() { return _location == _notifications.size(); }

    /**
     * @brief Returns true if the state of the observer corresponds to before the first notification it received. In other words, returns true if the observer is at the beginning of the execution.
    */
    bool is_back_to_origin() { return _location == 0; }

    /**  QUERYING INTERNAL STATE  **/
    /**
     * @brief Returns the truth value of a variable in the assignment.
    */
    sat::Tval var_value(sat::Tvar var);

    /**
     * @brief Returns the truth value of a literal in the assignment.
    */
    sat::Tval lit_value(sat::Tlit lit);

    /**
     * @brief Returns the decision level of a variable in the assignment.
    */
    sat::Tlevel var_level(sat::Tvar var);

    /**
     * @brief Returns the decision level of a literal in the assignment.
    */
    sat::Tlevel lit_level(sat::Tlit lit);

    /**
     * @brief Returns a reference to the assignment stack.
    */
    const std::vector<sat::Tlit> &get_assignment();

    /**
     * @brief Returns the list of clauses that are currently active.
    */
    std::vector<std::pair<sat::Tclause, const std::vector<sat::Tlit> *>> get_clauses();

    /**  PRINTING  **/

    std::string literal_to_string(sat::Tlit lit);

    std::string variable_to_string(sat::Tvar var);

    void sort_clauses(sat::Tclause cl);

    std::string clause_to_string(sat::Tclause cl);

    // TODO add tag for learned / external
    void print_clause_set();

    void print_deleted_clauses();

    void print_assignment();

    void print_variables();

    ~observer();
  };
}