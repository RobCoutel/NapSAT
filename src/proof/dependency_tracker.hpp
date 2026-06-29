/**
 * This file is part of the source code of the software program
 * NapSAT. It is protected by applicable copyright laws.
 *
 * This source code is protected by the terms of the MIT License.
 */
/**
 * @file src/proof/dependency_tracker.hpp
 * @author Robin Coutelier
 *
 * @brief This file is part of the NapSAT solver. It defines a dependency tracker
 * to track on which input clauses a learned clause depends.
 */

#pragma once

#include "../../include/SAT-types.hpp"

#include <vector>
#include <set>

namespace napsat
{
  namespace proof
  {
    class dependency_tracker
    {
    public:
      dependency_tracker() = default;
      ~dependency_tracker() = default;

      /**
       * @brief Marks a clause as an input clause.
       * @param id the clause to mark as input.
       * @details This method will allocate space for the clause if needed.
       */
      void add_input(Tclause id);

      /**
       * @brief Marks a clause as a learned clause.
       * @param id the clause to mark as learned.
       */
      void add_learned(Tclause id);

      /**
       * @brief Links a clause to one of its dependencies.
       * @note Even an input clause can depend on other input clauses, if for example, it is shortened by root level propagation.
       * @param id the clause.
       * @param new_dep the clause on which the clause depends.
       */
      void link_dependencies(Tclause id, Tclause new_dep);

      /**
       * @brief Starts tracking dependencies for a clause whose id is still unknown.
       * This clause is always a learned clause.
       */
      void start_tracking();

      /**
       * @brief Tracks a dependency for the current clause being tracked.
       * @param dep the clause on which the current clause depends.
       */
      void track_dependency(Tclause dep);

      /**
       * @brief Cancels the current dependency tracking
       */
      void cancel_tracking();

      /**
       * @brief Commits the current dependency tracking to the given clause id.
       * @param id the clause to which the current dependency tracking is committed.
       */
      void finalize_tracking(Tclause id);

      /**
       * @brief Marks a clause as deleted. Its dependencies are no longer tracked.
       * @details This method enables to reuse the clause if for new dependency tracking.
       * @note Deleting an input clause is not allowed
       * @param id the clause to delete.
       */
      void delete_clause(Tclause id);

      const std::set<Tclause>& get_dependencies(Tclause id);

    private:
      struct clause_dependencies
      {
        bool is_input = false;
        bool in_use = false;
        std::set<Tclause> premises;
      };

      std::set<Tclause> _current_tracking;

      indexed_vector<clause_dependencies, Tclause> _clauses;
    };
  }
}
