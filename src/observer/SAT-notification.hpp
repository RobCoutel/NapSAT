/**
 * @file src/observer/SAT-notification.hpp
 * @author Robin Coutelier
 *
 * @brief This file is part of observer. It defines the notifications that can be sent by the SAT
 * solver to the observer.
 */
#pragma once

#include "SAT-config.hpp"
#include "SAT-types.hpp"
// #include "SAT-observer.hpp"

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
namespace napsat::gui
{
  const unsigned MAX_UNSIGNED = 0xFFFFFFFF;
  /**
   * @brief The level of marker events.
  */
  const unsigned MARKED_LEVEL = 1;

  class observer;

  enum ENotifType
  {
    CHECKPOINT,
    DONE,
    MARKER,
    NEW_VARIABLE,
    DELETE_VARIABLE,
    DECISION,
    IMPLICATION,
    PROPAGATION,
    REMOVE_PROPAGATION,
    CONFLICT,
    BACKTRACKING_STARTED,
    BACKTRACKING_DONE,
    UNASSIGNMENT,
    NEW_CLAUSE,
    DELETE_CLAUSE,
    WATCH,
    UNWATCH,
    BLOCKER,
    REMOVE_LITERAL,
    CHECK_INVARIANTS,
    MISSED_LOWER_IMPLICATION,
    REMOVE_LOWER_IMPLICATION,
    STAT
  };
  std::string notification_type_to_string(ENotifType type);

  /**
   * @brief Virtual class that defines notifications that can be sent by the SAT solver to the observer.
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

  public:
    /**
     * @brief Suppresses the warning that are displayed when notification do not make sense to the observer.
     */
    static void suppress_warning(bool suppress)

    {
      _suppress_warning = suppress;
    }
    /**
     * @brief Returns a copy of the notification.
     */
    virtual notification* clone() const = 0;
    /**
     * @brief Get the level of the event.
     * - 0: reserved for checkpoints.
     * - 1: reserved for markers.
     * - 2: reserved for decisions.
     * - 3: reserved for new variables and clauses.
     * - 4: reserved for backtracking started and done.
     * - 5: reserved for implications and unassignments.
     * - 6: reserved for propagations.
     * - 9: watch list changes.
     */
    virtual unsigned get_event_level(observer* observer) { return event_level; }

    /**
     * @brief Returns the type of the notification.
     */
    virtual const ENotifType get_type() = 0;

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
    virtual void apply(observer* observer) = 0;

    /**
     * @brief Rollbacks the notification from the observer.
     * @param observer The observer that will be modified.
     */
    virtual void rollback(observer* observer) = 0;

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
    checkpoint* clone() const override { return new checkpoint(); }
    unsigned get_event_level(observer* observer) override { return event_level; }
    const ENotifType get_type() override { return CHECKPOINT; }
    const std::string get_message() override { return "Checkpoint"; }
    virtual void apply(observer* observer) override;
    virtual void rollback(observer* observer) override;
    ~checkpoint() override = default;
  };

  /**
   * @brief Notification that the solving process is done.
   */
  class done : public notification
  {
  private:
    unsigned event_level = 0;

    bool sat;

  public:
    done(bool sat) : sat(sat) {}
    done* clone() const override { return new done(sat); }
    unsigned get_event_level(observer* observer) override { return event_level; }
    const ENotifType get_type() override { return DONE; }
    const std::string get_message() override { return "Done: " + std::to_string(sat); }
    virtual void apply(observer* observer) override;
    virtual void rollback(observer* observer) override {}
    ~done() override = default;
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
    /**
     * @brief Additional information about the marker.
     */
    std::string description;

  public:
    marker() {}
    marker(std::string description) : description(description) {}
    marker* clone() const override { return new marker(); }
    unsigned get_event_level(observer* observer) override { return event_level; }
    const ENotifType get_type() override { return MARKER; }
    const std::string get_message() override { return "Marker : " + description; }
    virtual void apply(observer* observer) override {}
    virtual void rollback(observer* observer) override {}
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
    napsat::Tvar var;

  public:
    new_variable(napsat::Tvar var) : var(var) {}
    new_variable* clone() const override { return new new_variable(var); }
    unsigned get_event_level(observer* observer) override;
    const ENotifType get_type() override { return NEW_VARIABLE; }
    const std::string get_message() override { return "New variable " + std::to_string(var) + " added"; }
    virtual void apply(observer* observer) override;
    virtual void rollback(observer* observer) override;
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
    napsat::Tvar var;

  public:
    delete_variable(napsat::Tvar var) : var(var) {}
    delete_variable* clone() const override { return new delete_variable(var); }
    unsigned get_event_level(observer* observer) override;
    const ENotifType get_type() override { return DELETE_VARIABLE; }
    const std::string get_message() override { return "Variable " + std::to_string(var) + " deleted"; }
    virtual void apply(observer* observer) override;
    virtual void rollback(observer* observer) override;
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
    napsat::Tlit lit;

  public:
    decision(napsat::Tlit lit) : lit(lit) {}
    decision* clone() const override { return new decision(lit); }
    unsigned get_event_level(observer* observer) override;
    const ENotifType get_type() override { return DECISION; }
    const std::string get_message() override { return "Decision literal : " + std::to_string(napsat::lit_to_int(lit)); }
    virtual void apply(observer* observer) override;
    virtual void rollback(observer* observer) override;
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
    napsat::Tlit lit;

    /**
     * @brief The clause that was used to propagate the literal.
     */
    napsat::Tclause reason;

    /**
     * @brief The level at which the literal was assigned.
     */
    napsat::Tlevel level = LEVEL_UNDEF;

  public:
    implication(napsat::Tlit lit, napsat::Tclause cl, napsat::Tlevel level) : lit(lit), reason(cl), level(level) {}
    implication* clone() const override { return new implication(lit, reason, level); }
    unsigned get_event_level(observer* observer) override;
    const ENotifType get_type() override { return IMPLICATION; }
    const std::string get_message() override { return "Implication : " + std::to_string(napsat::lit_to_int(lit)) + " implied by clause " + std::to_string(reason); }
    virtual void apply(observer* observer) override;
    virtual void rollback(observer* observer) override;
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
    napsat::Tlit lit;

  public:
    propagation(napsat::Tlit lit) : lit(lit) {}
    propagation* clone() const override { return new propagation(lit); }
    unsigned get_event_level(observer* observer) override;
    const ENotifType get_type() override { return PROPAGATION; }
    const std::string get_message() override { return "Propagation : " + std::to_string(napsat::lit_to_int(lit)) + " propagated"; }
    virtual void apply(observer* observer) override;
    virtual void rollback(observer* observer) override;
    ~propagation() override = default;
  };

  /**
   * @brief Notification that a literal was propagated.
   */
  class remove_propagation : public notification
  {
  private:
    unsigned event_level = 6;

    /**
     * @brief The literal that was propagated.
     */
    napsat::Tlit lit;

  public:
    remove_propagation(napsat::Tlit lit) : lit(lit) {}
    remove_propagation* clone() const override { return new remove_propagation(lit); }
    unsigned get_event_level(observer* observer) override;
    const ENotifType get_type() override { return PROPAGATION; }
    const std::string get_message() override { return "Propagation removed : " + std::to_string(napsat::lit_to_int(lit)); }
    virtual void apply(observer* observer) override;
    virtual void rollback(observer* observer) override;
    ~remove_propagation() override = default;
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
    napsat::Tclause cl;

  public:
    conflict(napsat::Tclause cl) : cl(cl) {}
    conflict* clone() const override { return new conflict(cl); }
    unsigned get_event_level(observer* observer) override;
    const ENotifType get_type() override { return CONFLICT; }
    const std::string get_message() override { return "Conflict : clause " + std::to_string(cl) + " detected"; }
    virtual void apply(observer* observer) override;
    virtual void rollback(observer* observer) override {}
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

    napsat::Tlevel level = LEVEL_UNDEF;

  public:
    backtracking_started(napsat::Tlevel level) : level(level) {}
    backtracking_started* clone() const override { return new backtracking_started(level); }
    unsigned get_event_level(observer* observer) override { return event_level; }
    const ENotifType get_type() override { return BACKTRACKING_STARTED; }
    const std::string get_message() override { return "Backtracking started at level " + std::to_string(level); }
    virtual void apply(observer* observer) override {}
    virtual void rollback(observer* observer) override {}
    ~backtracking_started() override = default;
  };

  /**
   * @brief Notification that backtracking was done.
   */
  class backtracking_done : public marker
  {
  private:
    unsigned event_level = 4;

  public:
    backtracking_done() {}
    backtracking_done* clone() const override { return new backtracking_done(); }
    unsigned get_event_level(observer* observer) override { return event_level; }
    const ENotifType get_type() override { return BACKTRACKING_DONE; }
    const std::string get_message() override { return "Backtracking done"; }
    virtual void apply(observer* observer) override;
    virtual void rollback(observer* observer) override;
    ~backtracking_done() override = default;
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
    napsat::Tlit lit;

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
    napsat::Tlevel level = LEVEL_UNDEF;

    /**
     * @brief The clause that was used to propagate the literal.
     * Should be set by the observer when the notification is received.
     */
    napsat::Tclause reason = CLAUSE_UNDEF;

  public:
    unassignment(napsat::Tlit lit) : lit(lit) {}
    unassignment* clone() const override { return new unassignment(lit); }
    unsigned get_event_level(observer* observer) override;
    const ENotifType get_type() override { return UNASSIGNMENT; }
    const std::string get_message() override { return "Unassignment : " + std::to_string(napsat::lit_to_int(lit)) + " unassigned"; }
    virtual void apply(observer* observer) override;
    virtual void rollback(observer* observer) override;
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
    napsat::Tclause cl;

    /**
     * @brief The literals of the clause.
     */
    std::vector<napsat::Tlit> lits;

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
    new_clause(napsat::Tclause cl, std::vector<napsat::Tlit> lits, bool learnt, bool external) : cl(cl), lits(lits), learnt(learnt), external(external) {}
    new_clause* clone() const override { return new new_clause(cl, lits, learnt, external); }
    unsigned get_event_level(observer* observer) override;
    const ENotifType get_type() override { return NEW_CLAUSE; }
    const std::string get_message() override
    {
      std::string s = "New clause : " + std::to_string(cl) + ": ";
      for (auto l : lits) {
        s += " " + std::to_string(napsat::lit_to_int(l));
      }
      return s;
    }
    virtual void apply(observer* observer) override;
    virtual void rollback(observer* observer) override;
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
    napsat::Tclause cl;

    /**
     * Id computed by the observer to identify the clause.
     */
    unsigned long hash;

  public:
    delete_clause(napsat::Tclause cl) : cl(cl) {}
    delete_clause* clone() const override { return new delete_clause(cl); }
    unsigned get_event_level(observer* observer) override;
    const ENotifType get_type() override { return DELETE_CLAUSE; }
    const std::string get_message() override { return "Delete clause : " + std::to_string(cl); }
    virtual void apply(observer* observer) override;
    virtual void rollback(observer* observer) override;
    ~delete_clause() override = default;
  };

  class watch : public notification
  {
  private:
    unsigned event_level = 9;

    /**
     * @brief The clause id that was deleted.
     */
    napsat::Tclause cl;

    /**
     * @brief The literal that was watched.
     */
    napsat::Tlit lit;

  public:
    watch(napsat::Tclause cl, napsat::Tlit lit) : cl(cl), lit(lit) {}
    watch* clone() const override { return new watch(cl, lit); }
    unsigned get_event_level(observer* observer) override;
    const ENotifType get_type() override { return WATCH; }
    const std::string get_message() override { return "Watch literal : " + std::to_string(napsat::lit_to_int(lit)) + " in clause " + std::to_string(cl); }
    virtual void apply(observer* observer) override;
    virtual void rollback(observer* observer) override;
  };

  class unwatch : public notification
  {
  private:
    unsigned event_level = 9;

    /**
     * @brief The clause id that was deleted.
     */
    napsat::Tclause cl;

    /**
     * @brief The literal that was watched.
     */
    napsat::Tlit lit;

  public:
    unwatch(napsat::Tclause cl, napsat::Tlit lit) : cl(cl), lit(lit) {}
    unwatch* clone() const override { return new unwatch(cl, lit); }
    unsigned get_event_level(observer* observer) override;
    const ENotifType get_type() override { return UNWATCH; }
    const std::string get_message() override { return "Unwatch literal : " + std::to_string(napsat::lit_to_int(lit)) + " in clause " + std::to_string(cl); }
    virtual void apply(observer* observer) override;
    virtual void rollback(observer* observer) override;
  };

  /**
   * @brief Notification that a blocker was set to a clause.
  */
  class block : public notification
  {
  private:
    unsigned event_level = 9;

    /**
     * @brief The clause id that was deleted.
     */
    napsat::Tclause cl;

    /**
     * @brief The literal that was watched.
     */
    napsat::Tlit lit;
    /**
     * @brief The previous blocker of the clause (for rollback)
    */
    napsat::Tlit previous_blocker;

  public:
    block(napsat::Tclause cl, napsat::Tlit lit) : cl(cl), lit(lit) {}
    block* clone() const override { return new block(cl, lit); }
    unsigned get_event_level(observer* observer) override;
    const ENotifType get_type() override { return BLOCKER; }
    const std::string get_message() override { return "Block literal : " + std::to_string(napsat::lit_to_int(lit)) + " in clause " + std::to_string(cl); }
    virtual void apply(observer* observer) override;
    virtual void rollback(observer* observer) override;
  };

  /**
   * @brief Notification that a literal has be removed from a clause.
   * @pre The literal should be a level 0 literal.
   */
  class remove_literal : public notification
  {
  private:
    unsigned event_level = 9;

    /**
     * @brief The clause id that was deleted.
     */
    napsat::Tclause cl;

    /**
     * @brief The literal that was removed.
     */
    napsat::Tlit lit;

  public:
    remove_literal(napsat::Tclause cl, napsat::Tlit lit) : cl(cl), lit(lit) {}
    remove_literal* clone() const override { return new remove_literal(cl, lit); }
    const std::string get_message() override { return "Remove literal : " + std::to_string(napsat::lit_to_int(lit)) + " from clause " + std::to_string(cl); }
    unsigned get_event_level(observer* observer) override;
    const ENotifType get_type() override { return REMOVE_LITERAL; }
    virtual void apply(observer* observer) override;
    virtual void rollback(observer* observer) override;
  };

  class check_invariants : public notification
  {
  private:
    unsigned event_level = 0xFFFFFFFF;

  public:
    check_invariants() {}
    check_invariants* clone() const override { return new check_invariants(); }
    const std::string get_message() override { return "Check invariants"; }
    unsigned get_event_level(observer* observer) override { return event_level; }
    const ENotifType get_type() override { return CHECK_INVARIANTS; }
    virtual void apply(observer* observer) override;
    virtual void rollback(observer* observer) override;
  };

  class missed_lower_implication : public notification
  {
  private:
    unsigned event_level = 5;

    Tvar var;
    Tclause cl;
    Tclause last_cl;

  public:
    missed_lower_implication(Tvar var, Tclause cl) : var(var), cl(cl) {}
    missed_lower_implication* clone() const override { return new missed_lower_implication(var, cl); }
    const std::string get_message() override { return "Missed lower implication: " + std::to_string(var) + " in clause " + std::to_string(cl); }
    unsigned get_event_level(observer* observer) override { return event_level; }
    const ENotifType get_type() override { return MISSED_LOWER_IMPLICATION; }
    virtual void apply(observer* observer) override;
    virtual void rollback(observer* observer) override;
  };

  class remove_lower_implication : public notification
  {
  private:
    unsigned event_level = 5;

    Tvar var;
    Tclause last_cl;

  public:
    remove_lower_implication(Tvar var) : var(var) {}
    remove_lower_implication* clone() const override { return new remove_lower_implication(var); }
    const std::string get_message() override { return "Remove lower implication: " + std::to_string(var) + " in clause " + std::to_string(last_cl); }
    unsigned get_event_level(observer* observer) override { return event_level; }
    const ENotifType get_type() override { return REMOVE_LOWER_IMPLICATION; }
    virtual void apply(observer* observer) override;
    virtual void rollback(observer* observer) override;
  };




  class stat : public notification
  {
  private:
    unsigned event_level = 0xFFFFFFFF;
    /**
     * @brief The variable that was measured.
     * @details The variable should be a string that can be used as a key in a map.
     * @details This is what will be displayed at the end of the execution if the observer computes stats
    */
    std::string measured_variable;

  public:
    stat(std::string measured_variable) : measured_variable(measured_variable) {}
    stat* clone() const override { return new stat(measured_variable); }
    const std::string get_message() override { return "Stat : " + measured_variable; }
    unsigned get_event_level(observer* observer) override { return event_level; }
    const ENotifType get_type() override { return STAT; }
    virtual void apply(observer* observer) override {}
    virtual void rollback(observer* observer) override {}
  };

}
