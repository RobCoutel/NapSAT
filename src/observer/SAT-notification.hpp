/*
 * This file is part of the source code of the software program
 * NapSAT. It is protected by applicable copyright laws.
 *
 * This source code is protected by the terms of the MIT License.
 */
/**
 * @file src/observer/SAT-notification.hpp
 * @author Robin Coutelier
 *
 * @brief This file is part of the NapSAT solver. It defines the notifications that can be sent by the SAT
 * solver to the observer.
 */
#pragma once

#include "SAT-config.hpp"
#include "SAT-types.hpp"

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

  class observer;

  enum ENotifType
  {
    CHECKPOINT,
    DONE,
    MARKER,
    VARIABLE_ADDED,
    VARIABLE_DELETED,
    CLAUSE_NEW,
    CLAUSE_DELETED,
    REMOVE_LITERAL,
    DECISION,
    IMPLICATION,
    UNASSIGNMENT,
    PROPAGATION_ADDED,
    PROPAGATION_REMOVED,
    CONFLICT,
    BACKTRACKING_STARTED,
    BACKTRACKING_DONE,
    WATCH,
    UNWATCH,
    BLOCKER,
    CHECK_INVARIANTS,
    MISSED_LOWER_IMPLICATION_LOGGED,
    REMOVE_LOWER_IMPLICATION_REMOVED,
    STAT
  };
  std::string notification_type_to_string(ENotifType type);

  /**
   * @brief Virtual class that defines notifications that can be sent by the SAT solver to the observer.
   */
  class notification
  {
  protected:
    /**
     * @brief return an event_level of 0 if set to true
     */
    bool muted = false;

  public:
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
    virtual unsigned get_event_level(observer* observer) const noexcept = 0;

    /**
     * @brief Returns the type of the notification.
     */
    virtual ENotifType get_type() const noexcept = 0;

    /**
     * @brief Returns a short string describing the event.
     * @return const std::string A short string describing the event.
     */
    virtual const std::string get_message() const noexcept = 0;

    /**
     * @brief Applies the notification to the observer.
     * @details Also updates internal variables of the notification to allow rollback.
     * @param observer The observer that will be modified.
     */
    virtual bool apply(observer* observer) = 0;

    /**
     * @brief Rollbacks the notification from the observer.
     * @param observer The observer that will be modified.
     */
    virtual bool rollback(observer* observer) = 0;

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
    bool applied = false;

  public:
    static const unsigned DEFAULT_LEVEL = 0;
    static const ENotifType NTYPE = CHECKPOINT;

    unsigned get_event_level(observer* observer) const noexcept override { return DEFAULT_LEVEL; }
    ENotifType get_type() const noexcept override { return NTYPE; }
    const std::string get_message() const noexcept override { return "Checkpoint"; }

    explicit checkpoint() = default;

    checkpoint* clone() const override { return new checkpoint(); }
    bool apply(observer* observer) override;
    bool rollback(observer* observer) override { return true; }
  };

  /**
   * @brief Notification that the solving process is done.
   */
  class done : public notification
  {
  private:
    bool sat;

  public:
    static const unsigned DEFAULT_LEVEL = 0;
    static const ENotifType NTYPE = DONE;

    unsigned get_event_level(observer* observer) const noexcept override { return DEFAULT_LEVEL; }
    ENotifType get_type() const noexcept override { return NTYPE; }
    const std::string get_message() const noexcept override { return "Done: " + std::to_string(sat); }

    explicit done(bool sat) : sat(sat) {}

    done* clone() const override { return new done(sat); }
    bool apply(observer* observer) override { return true; }
    bool rollback(observer* observer) override { return true; }
  };

  /**
   * @brief Notification that a marker was reached.
   * @details A marker is a moment during at which the solver will stop and wait for the user to navigate through the history.
   * @details Markers do not do anything in particular but are used as a logging tool by the display.
   */
  class marker : public notification
  {
  private:
    /**
     * @brief Additional information about the marker.
     */
    std::string description;

  public:
    static const unsigned DEFAULT_LEVEL = 1;
    static const ENotifType NTYPE = MARKER;

    unsigned get_event_level(observer* observer) const noexcept override { return DEFAULT_LEVEL; }
    ENotifType get_type() const noexcept override { return NTYPE; }
    const std::string get_message() const noexcept override { return "Marker : " + description; }

    explicit marker() = default;
    explicit marker(std::string description) : description(std::move(description)) {}

    marker* clone() const override { return new marker(); }
    bool apply(observer* observer) override { return true; }
    bool rollback(observer* observer) override { return true; }
  };

  /**
   * @brief Notification that a new variable was added.
   */
  class new_variable : public notification
  {
  private:
    /**
     * @brief The variable that was added.
     */
    napsat::Tvar var;

  public:
    static const unsigned DEFAULT_LEVEL = 4;
    static const ENotifType NTYPE = VARIABLE_ADDED;

    unsigned get_event_level(observer* observer) const noexcept override;
    ENotifType get_type() const noexcept override { return NTYPE; }
    const std::string get_message() const noexcept override { return "New variable " + std::to_string(var) + " added"; }

    explicit new_variable(napsat::Tvar var) : var(var) {}

    new_variable* clone() const override { return new new_variable(var); }
    bool apply(observer* observer) override;
    bool rollback(observer* observer) override;
  };

  /**
   * @brief Notification that a variable was deleted.
   * @pre Assumes that all clauses containing the variable have been deleted prior to this notification.
   */
  class delete_variable : public notification
  {
  private:
    /**
     * @brief The variable that was deleted.
     */
    napsat::Tvar var;

  public:
    static const unsigned DEFAULT_LEVEL = 3;
    static const ENotifType NTYPE = VARIABLE_DELETED;

    unsigned get_event_level(observer* observer) const noexcept override;
    ENotifType get_type() const noexcept override { return NTYPE; }
    const std::string get_message() const noexcept override { return "Variable " + std::to_string(var) + " deleted"; }

    explicit delete_variable(napsat::Tvar var) : var(var) {}

    delete_variable* clone() const override { return new delete_variable(var); }
    bool apply(observer* observer) override;
    bool rollback(observer* observer) override;
  };

  /**
   * @brief Notification that a literal was decided.
   * @details A literal is decided when it is added to the assignment arbitrarily, without being implied by the current assignment.
   */
  class decision : public notification
  {
  private:
    /**
     * @brief The literal that was decided.
     */
    napsat::Tlit lit;

  public:
    static const unsigned DEFAULT_LEVEL = 2;
    static const ENotifType NTYPE = DECISION;

    unsigned get_event_level(observer* observer) const noexcept override;
    ENotifType get_type() const noexcept override { return NTYPE; }
    const std::string get_message() const noexcept override { return "Decision literal : " + std::to_string(napsat::lit_to_int(lit)); }

    explicit decision(napsat::Tlit lit) : lit(lit) {}

    decision* clone() const override { return new decision(lit); }
    bool apply(observer* observer) override;
    bool rollback(observer* observer) override;
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
    static const unsigned DEFAULT_LEVEL = 5;
    static const ENotifType NTYPE = IMPLICATION;

    unsigned get_event_level(observer* observer) const noexcept override;
    ENotifType get_type() const noexcept override { return NTYPE; }
    const std::string get_message() const noexcept override { return "Implication : " + std::to_string(napsat::lit_to_int(lit)) + " implied by clause " + std::to_string(reason); }

    explicit implication(napsat::Tlit lit, napsat::Tclause cl, napsat::Tlevel level) : lit(lit), reason(cl), level(level) {}

    implication* clone() const override { return new implication(lit, reason, level); }
    bool apply(observer* observer) override;
    bool rollback(observer* observer) override;
  };

  class update_level : public notification
  {
  private:
    /**
     * @brief The literal that was updated.
     */
    napsat::Tlit lit;

    /**
     * @brief The new level of the literal.
     */
    napsat::Tlevel level = LEVEL_UNDEF;
    napsat::Tlevel old_level = LEVEL_UNDEF;

  public:
    static const unsigned DEFAULT_LEVEL = 5;
    static const ENotifType NTYPE = IMPLICATION;

    unsigned get_event_level(observer* observer) const noexcept override;
    ENotifType get_type() const noexcept override { return NTYPE; }
    const std::string get_message() const noexcept override { return "Update level : " + std::to_string(napsat::lit_to_int(lit)) + " updated to level " + std::to_string(level); }

    explicit update_level(napsat::Tlit lit, napsat::Tlevel level) : lit(lit), level(level) {}

    update_level* clone() const override { return new update_level(lit, level); }
    bool apply(observer* observer) override;
    bool rollback(observer* observer) override;
  };

  /**
   * @brief Notification that a literal was propagated.
   */
  class propagation : public notification
  {
  private:
    /**
     * @brief The literal that was propagated.
     */
    napsat::Tlit lit;

  public:
    static const unsigned DEFAULT_LEVEL = 6;
    static const ENotifType NTYPE = PROPAGATION_ADDED;

    unsigned get_event_level(observer* observer) const noexcept override;
    ENotifType get_type() const noexcept override { return NTYPE; }
    const std::string get_message() const noexcept override { return "Propagation : " + std::to_string(napsat::lit_to_int(lit)) + " propagated"; }

    explicit propagation(napsat::Tlit lit) : lit(lit) {}

    propagation* clone() const override { return new propagation(lit); }
    bool apply(observer* observer) override;
    bool rollback(observer* observer) override;
  };

  /**
   * @brief Notification that a literal was propagated.
   */
  class remove_propagation : public notification
  {
  private:
    /**
     * @brief The literal that was propagated.
     */
    napsat::Tlit lit;

  public:
    static const unsigned DEFAULT_LEVEL = 6;
    static const ENotifType NTYPE = PROPAGATION_REMOVED;

    unsigned get_event_level(observer* observer) const noexcept override;
    ENotifType get_type() const noexcept override { return NTYPE; }
    const std::string get_message() const noexcept override { return "Propagation removed : " + std::to_string(napsat::lit_to_int(lit)); }

    explicit remove_propagation(napsat::Tlit lit) : lit(lit) {}

    remove_propagation* clone() const override { return new remove_propagation(lit); }
    bool apply(observer* observer) override;
    bool rollback(observer* observer) override;
  };

  /**
   * @brief Notification that a conflict was detected.
   * @details This notification does not do anything but is used as a marker by the display.
   */
  class conflict : public marker
  {
  private:
    /**
     * @brief The clause that was used to detect the conflict.
     */
    napsat::Tclause cl;

  public:
    static const unsigned DEFAULT_LEVEL = 4;
    static const ENotifType NTYPE = CONFLICT;

    unsigned get_event_level(observer* observer) const noexcept override;
    ENotifType get_type() const noexcept override { return NTYPE; }
    const std::string get_message() const noexcept override { return "Conflict : clause " + std::to_string(cl) + " detected"; }

    explicit conflict(napsat::Tclause cl) : cl(cl) {}

    conflict* clone() const override { return new conflict(cl); }
    bool apply(observer* observer) override { return true; }
    bool rollback(observer* observer) override { return true; }
  };

  /**
   * @brief Notification that backtracking was done.
   * @details This notification does not do anything but is used as a marker by the display.
   */
  class backtracking_started : public marker
  {
  private:
    napsat::Tlevel level = LEVEL_UNDEF;

  public:
    static const unsigned DEFAULT_LEVEL = 4;
    static const ENotifType NTYPE = BACKTRACKING_STARTED;

    unsigned get_event_level(observer* observer) const noexcept override { return DEFAULT_LEVEL; }
    ENotifType get_type() const noexcept override { return NTYPE; }
    const std::string get_message() const noexcept override { return "Backtracking started at level " + std::to_string(level); }

    explicit backtracking_started(napsat::Tlevel level) : level(level) {}

    backtracking_started* clone() const override { return new backtracking_started(level); }
    bool apply(observer* observer) override { return true; }
    bool rollback(observer* observer) override { return true; }
  };

  /**
   * @brief Notification that backtracking was done.
   */
  class backtracking_done : public marker
  {
  public:
    static const unsigned DEFAULT_LEVEL = 4;
    static const ENotifType NTYPE = BACKTRACKING_DONE;

    unsigned get_event_level(observer* observer) const noexcept override { return DEFAULT_LEVEL; }
    ENotifType get_type() const noexcept override { return NTYPE; }
    const std::string get_message() const noexcept override { return "Backtracking done"; }

    backtracking_done() = default;

    backtracking_done* clone() const override { return new backtracking_done(); }
    bool apply(observer* observer) override { return true; };
    bool rollback(observer* observer) override { return true; };
  };

  /**
   * @brief Notification that a literal was unassigned.
   */
  class unassignment : public notification
  {
  private:
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
    static const unsigned DEFAULT_LEVEL = 4;
    static const ENotifType NTYPE = UNASSIGNMENT;

    unsigned get_event_level(observer* observer) const noexcept override;
    ENotifType get_type() const noexcept override { return NTYPE; }
    const std::string get_message() const noexcept override { return "Unassignment : " + std::to_string(napsat::lit_to_int(lit)) + " unassigned"; }

    explicit unassignment(napsat::Tlit lit) : lit(lit) {}

    unassignment* clone() const override { return new unassignment(lit); }
    bool apply(observer* observer) override;
    bool rollback(observer* observer) override;
  };

  /**
   * @brief Notification that a new clause was added.
   */
  class new_clause : public notification
  {
  private:
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
    static const unsigned DEFAULT_LEVEL = 3;
    static const ENotifType NTYPE = CLAUSE_NEW;

    unsigned get_event_level(observer* observer) const noexcept override;
    ENotifType get_type() const noexcept override { return NTYPE; }
    const std::string get_message() const noexcept override;

    explicit new_clause(napsat::Tclause cl, std::vector<napsat::Tlit> lits, bool learnt, bool external) : cl(cl), lits(std::move(lits)), learnt(learnt), external(external) {}

    new_clause* clone() const override { return new new_clause(cl, lits, learnt, external); }
    bool apply(observer* observer) override;
    bool rollback(observer* observer) override;
  };

  /**
   * @brief Notification that a clause was deleted.
   */
  class delete_clause : public notification
  {
  private:
    /**
     * @brief The clause id that was deleted.
     */
    napsat::Tclause cl;

    /**
     * Id computed by the observer to identify the clause.
     */
    unsigned long hash = 0;

  public:
    static const unsigned DEFAULT_LEVEL = 3;
    static const ENotifType NTYPE = CLAUSE_DELETED;

    unsigned get_event_level(observer* observer) const noexcept override;
    ENotifType get_type() const noexcept override { return NTYPE; }
    const std::string get_message() const noexcept override { return "Delete clause : " + std::to_string(cl); }

    explicit delete_clause(napsat::Tclause cl) : cl(cl) {}

    delete_clause* clone() const override { return new delete_clause(cl); }
    bool apply(observer* observer) override;
    bool rollback(observer* observer) override;
  };

  class watch : public notification
  {
  private:
    /**
     * @brief The clause id that was deleted.
     */
    napsat::Tclause cl;

    /**
     * @brief The literal that was watched.
     */
    napsat::Tlit lit;

  public:
    static const unsigned DEFAULT_LEVEL = 9;
    static const ENotifType NTYPE = WATCH;

    unsigned get_event_level(observer* observer) const noexcept override;
    ENotifType get_type() const noexcept override { return NTYPE; }
    const std::string get_message() const noexcept override { return "Watch literal : " + std::to_string(napsat::lit_to_int(lit)) + " in clause " + std::to_string(cl); }

    explicit watch(napsat::Tclause cl, napsat::Tlit lit) : cl(cl), lit(lit) {}

    watch* clone() const override { return new watch(cl, lit); }
    bool apply(observer* observer) override;
    bool rollback(observer* observer) override;
  };

  class unwatch : public notification
  {
  private:
    /**
     * @brief The clause id that was deleted.
     */
    napsat::Tclause cl;

    /**
     * @brief The literal that was watched.
     */
    napsat::Tlit lit;

    napsat::Tlit previous_blocker = LIT_UNDEF;

  public:
    static const unsigned DEFAULT_LEVEL = 9;
    static const ENotifType NTYPE = UNWATCH;

    unsigned get_event_level(observer* observer) const noexcept override;
    ENotifType get_type() const noexcept override { return NTYPE; }
    const std::string get_message() const noexcept override { return "Unwatch literal : " + std::to_string(napsat::lit_to_int(lit)) + " in clause " + std::to_string(cl); }

    explicit unwatch(napsat::Tclause cl, napsat::Tlit lit) : cl(cl), lit(lit) {}

    unwatch* clone() const override { return new unwatch(cl, lit); }
    bool apply(observer* observer) override;
    bool rollback(observer* observer) override;
  };

  /**
   * @brief Notification that a blocker was set to a clause.
  */
  class block : public notification
  {
  private:
    /**
     * @brief The clause id that was deleted.
     */
    napsat::Tclause cl;

    /**
     * @brief The  blocker that was set
     */
    napsat::Tlit blocker;

    /**
     * @brief The literal that is blocked in the watch list.
     */
    napsat::Tlit blocked_lit;

    /**
     * @brief The previous blocker of the clause (for rollback)
    */
    napsat::Tlit previous_blocker = LIT_UNDEF;

  public:
    static const unsigned DEFAULT_LEVEL = 9;
    static const ENotifType NTYPE = BLOCKER;

    unsigned get_event_level(observer* observer) const noexcept override;
    ENotifType get_type() const noexcept override { return NTYPE; }
    const std::string get_message() const noexcept override { return "Block literal : " + std::to_string(napsat::lit_to_int(blocker)) + " in clause " + std::to_string(cl); }

    explicit block(napsat::Tclause cl, napsat::Tlit blocker, napsat::Tlit blocked_lit) : cl(cl), blocker(blocker), blocked_lit(blocked_lit) {}

    block* clone() const override { return new block(cl, blocker, previous_blocker); }
    bool apply(observer* observer) override;
    bool rollback(observer* observer) override;
  };

  /**
   * @brief Notification that a literal has be removed from a clause.
   * @pre The literal should be a level 0 literal.
   */
  class remove_literal : public notification
  {
  private:
    /**
     * @brief The clause id that was deleted.
     */
    napsat::Tclause cl;

    /**
     * @brief The literal that was removed.
     */
    napsat::Tlit lit;

  public:
    static const unsigned DEFAULT_LEVEL = 9;
    static const ENotifType NTYPE = REMOVE_LITERAL;

    unsigned get_event_level(observer* observer) const noexcept override;
    ENotifType get_type() const noexcept override { return NTYPE; }
    const std::string get_message() const noexcept override { return "Remove literal : " + std::to_string(napsat::lit_to_int(lit)) + " from clause " + std::to_string(cl); }

    explicit remove_literal(napsat::Tclause cl, napsat::Tlit lit) : cl(cl), lit(lit) {}

    remove_literal* clone() const override { return new remove_literal(cl, lit); }
    bool apply(observer* observer) override;
    bool rollback(observer* observer) override;
  };

  class check_invariants : public notification
  {
  public:
    static const unsigned DEFAULT_LEVEL = -1;
    static const ENotifType NTYPE = CHECK_INVARIANTS;

    unsigned get_event_level(observer* observer) const noexcept override { return muted ? 0 : DEFAULT_LEVEL; }
    ENotifType get_type() const noexcept override { return NTYPE; }
    const std::string get_message() const noexcept override { return "Check invariants"; }

    explicit check_invariants() = default;

    check_invariants* clone() const override { return new check_invariants(); }
    bool apply(observer* observer) override;
    bool rollback(observer* observer) override;
  };

  class missed_lower_implication : public notification
  {
  private:
    Tvar var;
    Tclause cl;
    Tclause last_cl = CLAUSE_UNDEF;

  public:
    static const unsigned DEFAULT_LEVEL = 5;
    static const ENotifType NTYPE = MISSED_LOWER_IMPLICATION_LOGGED;

    unsigned get_event_level(observer* observer) const noexcept override { return DEFAULT_LEVEL; }
    ENotifType get_type() const noexcept override { return NTYPE; }
    const std::string get_message() const noexcept override { return "Missed lower implication: " + std::to_string(var) + " in clause " + std::to_string(cl); }

    explicit missed_lower_implication(Tvar var, Tclause cl) : var(var), cl(cl) {}

    missed_lower_implication* clone() const override { return new missed_lower_implication(var, cl); }
    bool apply(observer* observer) override;
    bool rollback(observer* observer) override;
  };

  class remove_lower_implication : public notification
  {
  private:
    Tvar var;
    Tclause last_cl = CLAUSE_UNDEF;

  public:
    static const unsigned DEFAULT_LEVEL = 5;
    static const ENotifType NTYPE = REMOVE_LOWER_IMPLICATION_REMOVED;

    unsigned get_event_level(observer* observer) const noexcept override { return DEFAULT_LEVEL; }
    ENotifType get_type() const noexcept override { return NTYPE; }
    const std::string get_message() const noexcept override { return "Remove lower implication: " + std::to_string(var) + " in clause " + std::to_string(last_cl); }

    explicit remove_lower_implication(Tvar var) : var(var) {}

    remove_lower_implication* clone() const override { return new remove_lower_implication(var); }
    bool apply(observer* observer) override;
    bool rollback(observer* observer) override;
  };




  class stat : public notification
  {
  private:
    /**
     * @brief The variable that was measured.
     * @details The variable should be a string that can be used as a key in a map.
     * @details This is what will be displayed at the end of the execution if the observer computes stats
    */
    std::string measured_variable;

  public:
    static const unsigned DEFAULT_LEVEL = -1;
    static const ENotifType NTYPE = STAT;

    unsigned get_event_level(observer* observer) const noexcept override { return DEFAULT_LEVEL; }
    ENotifType get_type() const noexcept override { return NTYPE; }
    const std::string get_message() const noexcept override { return "Stat : " + measured_variable; }

    explicit stat(std::string measured_variable) : measured_variable(measured_variable) {}

    stat* clone() const override { return new stat(measured_variable); }
    bool apply(observer* observer) override { return true; }
    bool rollback(observer* observer) override { return true; }
  };

}
