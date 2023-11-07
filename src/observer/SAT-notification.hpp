#pragma once

#include "../solver/SAT-config.hpp"
#include "../solver/SAT-types.hpp"
#include "SAT-observer.hpp"

#include <vector>
#include <string>
#include <cassert>

/**
 * Vocabulary:
 * - Variable: a variable is a literal without its sign.
 * - Literal: a literal is a variable with its sign.
 * - Clause: a clause is a disjunction of literals.
 * - Assignment: an assignment is a set of literals.
 * - A variable is assigned if it, or its negation, is in the assignment.
 * - A literal l is implied by an assignment and a clause if the clause is unit under the assignment and l is the only literal of the clause that is not falsified by the assignment.
 * - A literal l is propagated when it has been checked that it does not create a conflict and the literals implied by l have been added to the assignment.
 * - A literal is a decision literal if it is not implied by the current assignment but does not create a conflict when it is added to the assignment.
 */
namespace sat::gui
{
  class observer;
  const unsigned MAX_UNSIGNED = 0xFFFFFFFF;

  /**
   * @brief Virtual class that defines notifications that can be sent by the SAT solver.
   */
  class notification
  {
  private:
    /**
     * @brief The level of the event. The lower the level, the more important the event.
     */
    unsigned event_level;

  protected:
    /**
     * @brief True if the warning that are displayed when notification do not make sense to the observer are suppressed. False otherwise.
     * The default value is false.
     * To suppress the warnings, use notification::suppress_warning(true).
     * To display the warnings, use notification::suppress_warning(false).
     */
    static bool _suppress_warning;

    /**
     * @brief Suppresses the warning that are displayed when notification do not make sense to the observer.
     */
    static void suppress_warning(bool suppress)
    {
      _suppress_warning = suppress;
    }

  public:
    /**
     * @brief Get the level of the event.
     * - 0: reserved for checkpoints.
     * - 1: reserved for markers.
     * - 2: reserved for decisions.
     * - 3: reserved for new variables and clauses.
     * - 4: reserved for backtracking started and done.
     * - 5: reserved for implications and unassignments.
     * - 6: reserved for propagations.
     */
    virtual unsigned get_event_level() = 0;

    /**
     * @brief Returns a short string describing the event.
     * @return const std::string A short string describing the event.
     */
    virtual const std::string get_message() = 0;

    /**
     * @brief Applies the notification to the observer.
     * @details Also updates internal variables of the notification to allow rollback.
     * @param observer The observer that will be modified.
     */
    virtual void apply(observer &observer) = 0;

    /**
     * @brief Rollbacks the notification from the observer.
     * @param observer The observer that will be modified.
     */
    virtual void rollback(observer &observer) = 0;

    /**
     * @brief Destroy the notification object
     */
    virtual ~notification() = default;
  };

  /**
   * @brief Notification that a checkpoint was reached.
   * @details A checkpoint is a moment during the solving process at which the solver will stop and wait for the user to enter a command. If the command is empty, the solver will continue with its default behavior. If the command is not empty, the solver will execute the command and then continue with its default behavior.
   */
  class checkpoint : public notification
  {
  private:
    unsigned event_level = 0;
    bool applied = false;

  public:
    checkpoint() {}
    unsigned get_event_level() override { return event_level; }
    const std::string get_message() override { return "Checkpoint"; }
    virtual void apply(observer &observer);
    virtual void rollback(observer &observer);
    ~checkpoint() override = default;
  };

  /**
   * @brief Notification that a marker was reached.
   * @details A marker is a moment during at which the solver will stop and wait for the user to navigate through the history.
   * @details Markers do not do anything in particular but are used as a logging tool by the display.
   */
  class marker : public notification
  {
  private:
    unsigned event_level = 1;

  public:
    marker() {}
    unsigned get_event_level() override { return event_level; }
    const std::string get_message() override { return "Marker"; }
    virtual void apply(observer &observer) {}
    virtual void rollback(observer &observer) {}
    ~marker() override = default;
  };

  /**
   * @brief Notification that a new variable was added.
   */
  class new_variable : public notification
  {
  private:
    unsigned event_level = 3;

    /**
     * @brief The variable that was added.
     */
    sat::Tvar var;

  public:
    new_variable(sat::Tvar var) : var(var) {}
    unsigned get_event_level() override { return event_level; }
    const std::string get_message() override { return "New variable " + std::to_string(var) + " added"; }
    virtual void apply(observer &observer);
    virtual void rollback(observer &observer);
    ~new_variable() override = default;
  };

  /**
   * @brief Notification that a variable was deleted.
   * @pre Assumes that all clauses containing the variable have been deleted prior to this notification.
   */
  class delete_variable : public notification
  {
  private:
    unsigned event_level = 3;

    /**
     * @brief The variable that was deleted.
     */
    sat::Tvar var;

  public:
    delete_variable(sat::Tvar var) : var(var) {}
    unsigned get_event_level() override { return event_level; }
    const std::string get_message() override { return "Variable " + std::to_string(var) + " deleted"; }
    virtual void apply(observer &observer);
    virtual void rollback(observer &observer);
    ~delete_variable() override = default;
  };

  /**
   * @brief Notification that a literal was decided.
   * @details A literal is decided when it is added to the assignment arbitrarily, without being implied by the current assignment.
   */
  class decision : public notification
  {
  private:
    unsigned event_level = 2;

    /**
     * @brief The literal that was decided.
     */
    sat::Tlit lit;

  public:
    decision(sat::Tlit lit) : lit(lit) {}
    unsigned get_event_level() override { return event_level; }
    const std::string get_message() override { return "Decision literal : " + std::to_string(sat::lit_to_int(lit)); }
    virtual void apply(observer &observer);
    virtual void rollback(observer &observer);
    ~decision() override = default;
  };

  /**
   * @brief Notification that a literal was implied.
   * @details A literal is implied when it is added to the assignment because it is the only literal of a clause that is not falsified by the current assignment.
   * @pre Assumes that the clause is unit under the current assignment. Otherwise, the notification will fire a warning. To suppress the warning, use notification::suppress_warning(true).
   * @pre Assumes that the level provided is consistent with the current assignment. Otherwise, the notification will fire a warning. To suppress the warning, use notification::suppress_warning(true).
   * @pre Assumes that the literal is not already assigned.
   */
  class implication : public notification
  {
  private:
    unsigned event_level = 5;

    /**
     * @brief The literal that was propagated.
     */
    sat::Tlit lit;

    /**
     * @brief The clause that was used to propagate the literal.
     */
    sat::Tclause reason;

    /**
     * @brief The level at which the literal was assigned.
     */
    sat::Tlevel level = LEVEL_UNDEF;

  public:
    implication(sat::Tlit lit, sat::Tclause cl, sat::Tlevel level) : lit(lit), reason(cl), level(level) {}
    unsigned get_event_level() override { return event_level; }
    const std::string get_message() override { return "Implication : " + std::to_string(sat::lit_to_int(lit)) + " implied by clause " + std::to_string(reason); }
    virtual void apply(observer &observer);
    virtual void rollback(observer &observer);
    ~implication() override = default;
  };

  /**
   * @brief Notification that a literal was propagated.
   */
  class propagation : public notification
  {
  private:
    unsigned event_level = 6;

    /**
     * @brief The literal that was propagated.
     */
    sat::Tlit lit;

  public:
    propagation(sat::Tlit lit) : lit(lit) {}
    unsigned get_event_level() override { return event_level; }
    const std::string get_message() override { return "Propagation : " + std::to_string(sat::lit_to_int(lit)) + " propagated"; }
    virtual void apply(observer &observer);
    virtual void rollback(observer &observer);
    ~propagation() override = default;
  };

  /**
   * @brief Notification that a conflict was detected.
   * @details This notification does not do anything but is used as a marker by the display.
   */
  class conflict : public marker
  {
  private:
    unsigned event_level = 4;

    /**
     * @brief The clause that was used to detect the conflict.
     */
    sat::Tclause cl;

  public:
    conflict(sat::Tclause cl) : cl(cl) {}
    unsigned get_event_level() override { return event_level; }
    const std::string get_message() override { return "Conflict : clause " + std::to_string(cl) + " detected"; }
    virtual void apply(observer &observer) {}
    virtual void rollback(observer &observer) {}
    ~conflict() override = default;
  };

  /**
   * @brief Notification that backtracking was done.
   * @details This notification does not do anything but is used as a marker by the display.
   */
  class backtracking_started : public marker
  {
  private:
    unsigned event_level = 4;

  public:
    backtracking_started() {}
    unsigned get_event_level() override { return event_level; }
    const std::string get_message() override { return "Backtracking started"; }
    virtual void apply(observer &observer) {}
    virtual void rollback(observer &observer) {}
    ~backtracking_started() override = default;
  };

  /**
   * @brief Notification that backtracking was done.
   */
  class backtracking_done : public marker
  {
  private:
    unsigned event_level = 4;
  };

  /**
   * @brief Notification that a literal was unassigned.
   */
  class unassignment : public notification
  {
  private:
    unsigned event_level = 4;

    /**
     * @brief The literal that was unassigned.
     */
    sat::Tlit lit;

    /**
     * @brief True if the unassignment was a propagated literal.
     */
    bool propagated = false;

    /**
     * @brief The location of the literal in the assignment stack.
     * Should be set by the observer when the notification is received.
     */
    unsigned location = MAX_UNSIGNED;

    /**
     * @brief The level at which the literal was assigned.
     * Should be set by the observer when the notification is received.
     */
    sat::Tlevel level = LEVEL_UNDEF;

    /**
     * @brief The clause that was used to propagate the literal.
     * Should be set by the observer when the notification is received.
     */
    sat::Tclause reason = CLAUSE_UNDEF;

  public:
    unassignment(sat::Tlit lit) : lit(lit) {}
    unsigned get_event_level() override { return event_level; }
    const std::string get_message() override { return "Unassignment : " + std::to_string(sat::lit_to_int(lit)) + " unassigned"; }
    virtual void apply(observer &observer);
    virtual void rollback(observer &observer);
    ~unassignment() override = default;
  };

  /**
   * @brief Notification that a new clause was added.
  */
  class new_clause : public notification
  {
  private:
    unsigned event_level = 3;

    /**
     * @brief The clause id that was added.
     */
    sat::Tclause cl;

    /**
     * @brief The literals of the clause.
    */
    std::vector<sat::Tlit> lits;

    /**
     * @brief True if the clause was learnt.
    */
    bool learnt = false;

    /**
     * @brief True if the clause was added externally (by the user or from the problem statement).
    */
    bool external = false;

    /**
     * @brief Id computed by the observer to identify the clause when it is deleted.
     * @details The id is computed when the notification is applied.
    */
    long unsigned hash = 0;

  public:
    new_clause(sat::Tclause cl, std::vector<sat::Tlit> lits, bool learnt, bool external) : cl(cl), lits(lits), learnt(learnt), external(external) {}
    unsigned get_event_level() override { return event_level; }
    const std::string get_message() override
    {
      std::string s = "New clause : " + std::to_string(cl) + ": ";
      for (auto l : lits)
      {
        s += " " + std::to_string(sat::lit_to_int(l));
      }
      return s;
    }
    virtual void apply(observer &observer);
    virtual void rollback(observer &observer);
    ~new_clause() override = default;
  };

  /**
   * @brief Notification that a clause was deleted.
  */
  class delete_clause : public notification
  {
  private:
    unsigned event_level = 3;

    /**
     * @brief The clause id that was deleted.
     */
    sat::Tclause cl;

    /**
     * Id computed by the observer to identify the clause.
     */
    unsigned long hash;

  public:
    delete_clause(sat::Tclause cl) : cl(cl) {}
    unsigned get_event_level() override { return event_level; }
    const std::string get_message() override { return "Delete clause : " + std::to_string(cl); }
    virtual void apply(observer &observer);
    virtual void rollback(observer &observer);
    ~delete_clause() override = default;
  };
}