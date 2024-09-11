/**
 * This file is part of the source code of the software program
 * NapSAT. It is protected by applicable copyright laws.
 *
 * This source code is protected by the terms of the MIT License.
 */
/**
 * @file src/solver/NapSAT.hpp
 * @author Robin Coutelier
 *
 * @brief This file is part of the NapSAT solver. It defines the interface to
 * the proof generator and checker.
 *
 * @details A proof is recorded as a set of resolutions applied to the current
 * set of clauses. The input clauses are assumed to be properly given, and the
 * checker will then verify that the rest of the proof is correct.
 *
 * The proof as structured as follows:
 * - Input clauses simply recorded.
 * - A derived clause is recorded as a resolution chain of the form
 *     C, <■, C₁> <ℓ₂, C₂> ... <ℓₙ, Cₙ>
 *   where ℓᵢ is the pivot literal, and Cᵢ is the clause ID that was resolved
 *   As such, if R(C, C', ℓ) denotes the clause obtained by resolving C and C',
 *   that is, R(C, C', ℓ) = (C \ {ℓ}) ∪ (C' \ {¬ℓ}), then the clause C obtained
 *   from the chain C, <■, C₁> <ℓ₂, C₂> ... <ℓₙ, Cₙ> is
 *   C = R(... R(R(C₁, C₂, ℓ₂), C₃, ℓ₃) ..., Cₙ, ℓₙ)
 * @note Note that the first pivot is LIT_UNDEF since it is irrelevant for the
 * resolution. Therefore the first link of the chain will always be <■, C₁>.
 *
 * @details Since most SAT solvers use clause deletion and reuse the clause IDs,
 * the proof checker must match the virtual ID to the actual clause. We use the
 * data type Tclause to refer to the virtual ID, and TclauseID for the internal
 * ID.
 * A user can therefore deactivating a clause without losing it in the proof.
 *
 * @details
 * Tip: If the user desires to update a clause (such as remove a literal from
 * it), they can do so by first starting a resolution chain, linking the clauses
 * that justify the removal of the literal, then deactivate the clause, and
 * finally finalize the clause with the same index as before.
 */
#include "SAT-types.hpp"

#include <vector>
#include <unordered_set>

namespace napsat::proof
{
  typedef unsigned TclauseID;

  struct clause
  {
    /**
     * @brief The literals of the clause. */
    napsat::Tlit* lits;
    /**
     * @brief The number of literals in the clause. */
    unsigned size : 31;
    /**
     * @brief Simple marker to avoid rechecking the clause.
    */
    unsigned marked : 1;
    /**
     * @brief Resolution chain that led to the clause.
     * @details Input clauses have an empty resolution chain. */
    std::vector<std::pair<napsat::Tlit, TclauseID>> resolution_chain;
  };

  class resolution_proof
  {
    /**
     * @brief Buffer used to store the current resolution chain inputted by the
     * user. The chain is cleared when the function finalize_resolution is
     * called. */
    std::vector<std::pair<napsat::Tlit, napsat::Tclause>> current_resolution_chain;

    /**
     * @brief The clauses in the proof.
     * @details The clauses are indexed with TclauseID. */
    std::vector<clause> clauses;

    /**
     * @brief Dictionary holding the clause index for each clause ID provided
     * by the solver. It maps Tclause to TclauseID. */
    std::vector<TclauseID> clause_matches;

    /**
     * @brief Internal ID of the empty clause if it exists. It is the starting
     * point of the backward verification of the proof. */
    TclauseID empty_clause_id = CLAUSE_UNDEF;

    /**
     * @brief List of literals at root level.
     * @details This is used to simplify the clauses by removing all literals
     * falsified at level 0.
     * @invariant The order of the literals must follow a topological order of
     * the implication graph.
     * @invariant The literal at position i must have its reason at position i
     * in the root_reason vector. */
    std::vector<napsat::Tlit> root_lit;
    /**
     * @brief List of clause ID holding the reasons for the literals at root
     * level.
     * @details This is used to simplify the clauses by removing all literals
     * falsified at level 0.
     * @invariant The literal at position i in the root_lit vector must have its
     * reason at position i in the root_reason vector. */
    std::vector<napsat::Tclause> root_reason;

    /**
     * @brief Temporary vector used in check_resolution_chain
     */
    std::vector<Tlit> tmp_lits;

    /**
     * @brief Applies the resolution rule in place on the base clause with the
     * resolvent clause over the literal pivot.
     * @param base The base clause. Will be modified to contain the result of
     * the resolution.
     * @param resolvent_index The internal index of the resolvent clause in the
     * clauses
     * @param pivot The pivot literal.
     * @pre The pivot ℓ must be in the base clause.
     * @pre Both the base and the literals of the resolvent must be sorted
     * @pre The negation of the pivot ¬ℓ must be in the resolvent clause.
     * @post If the base clause was C and the resolvent clause was C', then
     *     C ← C \ {ℓ} ∪ C' \ {¬ℓ}
     * @post The base clause is sorted.
     */
    void apply_resolution(std::vector<Tlit> &base, TclauseID resolvent_index, Tlit pivot);
    /**
     * @brief Check the resolution chain of the clause id.
     * @param id The clause ID to check.
     * @return True if the resolution chain is correct, false if applying the
     * resolution chain yields different literals than the clause.
     */
    bool check_resolution_chain(TclauseID id);

    /**
     * @brief Print the clause with the given index.
     */
    void print_clause(TclauseID index);

    /**
     * @brief Print the mapping between the virtual clause ID and the internal
     * clause ID.
     */
    void print_clause_matches(void);

    /**
     * @brief Prints all the clauses in the proof.
     */
    void print_clause_set(void);


    public:
    /**
     * @brief Construct a new resolution_proof object */
    resolution_proof(void) = default;

    /**
     * @brief Add an input clause to the proof.
     * @details The literal pointer is left untouched.
     * @param id The clause ID provided by the solver.
     * @param lits The literals of the clause.
     * @param n_lits The number of literals in the clause.
     * @pre The clause id must not be currently in use. The clause should
     * be first deactivated before overriding it.
     */
    void input_clause(napsat::Tclause id, const napsat::Tlit* lits, unsigned n_lits);

    /**
     * @brief Start a new resolution chain.
     * @pre Assumes that the previous resolution chain was finalized.
     */
    void start_resolution_chain(void);

    /**
     * @brief Adds a new link to the resolution chain.
     * @details The first link of the chain must always have the pivot set to
     * LIT_UNDEF since it is irrelevant for the resolution.
     * @param pivot The pivot literal.
     * @param id The clause ID that was resolved.
     */
    void link_resolution(napsat::Tlit pivot, napsat::Tclause id);

    /**
     * @brief Finalize the resolution chain and add the derived clause to the
     * proof. A new clause will be added with virtual ID id.
     * @param id The clause ID provided by the solver.
     * @param lits The literals of the clause.
     * @param n_lits The number of literals in the clause.
     * @pre The clause id must not be currently in use. The clause should
     * be first deactivated before overriding it.
     * @pre The resolution chain must be properly formed.
     * @post The resolution chain is cleared.
     * @post The clause is added to the proof.
     * @post The clause is sorted.
     * @details If ran in debug mode, the function will check that the clause
     * is correctly derived from the resolution chain.
     */
    void finalize_resolution(napsat::Tclause id, const napsat::Tlit* lits, unsigned n_lits);

    /**
     * @brief Assigns a literal at root level.
     * @details This must be done before calling remove_root_literals.
     * @param lit The literal to assign.
     * @param reason The clause ID that justifies the assignment.
     * @pre The literal must not be already assigned at root level.
     * @pre All the literals in the reason clause must be falsified by the
     * literals assigned at root level in previous calls (except lit).
     * @pre The reason clause must not be deactivated.
     */
    void root_assign(napsat::Tlit lit, napsat::Tclause reason);

    /**
     * @brief Create a resolution chain that removes from the clause id, all
     * literals falsified at the root level. The clause without the literals is
     * then added to the proof and replaces the original clause with ID id
     * @details The clause will only be simplified by the literals assigned at
     * root level with the function root_assign.
     * @param id The clause ID to simplify.
     */
    void remove_root_literals(napsat::Tclause id);

    /**
     * @brief Deactivate a clause. This enable to recycle the clause ID.
     * @param id The clause ID to deactivate.
     * @pre The clause must not be already deactivated.
     * @post The clause is deactivated but still present in the proof.
     * It is no longer accessible from the outside.
     */
    void deactivate_clause(napsat::Tclause id);

    /**
     * @brief Check the proof if the empty clause is present.
     * If the proof is incorrect, an error message is printed.
     * @return True if the proof is correct, false otherwise.
     */
    bool check_proof(void);

    /**
     * Prints a resolution chain leading to the clause with the given index.
     */
    void print_resolution_chain(unsigned index);

    /**
     * @brief Print the proof in a top-down format. Only the clauses relevant
     * to reaching the empty clause are printed.
     * @pre The proof must be correct.
     * @pre The empty clause must be present.
     */
    void print_proof(void);

    /**
     * @brief Destroy the resolution_proof object
     */
    ~resolution_proof();
  };
}
