/*
 * This file is part of the source code of the software program
 * NapSAT. It is protected by applicable copyright laws.
 *
 * This source code is protected by the terms of the MIT License.
 */
/**
 * @file src/proof/dependency_tracker.cpp
 * @author Robin Coutelier
 *
 * @brief This file is part of the NapSAT solver. It implements a dependency tracker
 * to track on which input clauses a learned clause depends.
 * This is used to produce UNSAT cores.
 */

#include "dependency_tracker.hpp"

#include <cassert>

namespace napsat
{
namespace proof
{
  void dependency_tracker::add_input(Tclause id) {
    if (id >= _clauses.size()) {
      _clauses.resize(id + 1);
    }
    assert (!_clauses[id].in_use);
    assert (_clauses[id].premises.empty());
    _clauses[id].is_input = true;
    _clauses[id].in_use = true;
    _clauses[id].premises.insert(id);
  }

  void dependency_tracker::add_learned(Tclause id)
  {
    if (id >= _clauses.size()) {
      _clauses.resize(id + 1);
    }
    assert (!_clauses[id].in_use);
    assert (_clauses[id].premises.empty());
    _clauses[id].is_input = false;
    _clauses[id].in_use = true;
  }

  void dependency_tracker::link_dependencies(Tclause id, Tclause new_dep)
  {
    assert (id < _clauses.size());
    assert (_clauses[id].in_use);
    assert (new_dep < _clauses.size());
    assert (_clauses[new_dep].in_use);
    // add all the dependencies of new_dep to id
    _clauses[id].premises.insert(_clauses[new_dep].premises.begin(), _clauses[new_dep].premises.end());
  }

  void dependency_tracker::start_tracking()
  {
    assert (_current_tracking.empty());
  }

  void dependency_tracker::track_dependency(Tclause dep)
  {
    assert (dep < _clauses.size());
    assert (_clauses[dep].in_use);
    _current_tracking.insert(_clauses[dep].premises.begin(), _clauses[dep].premises.end());
  }

  void dependency_tracker::cancel_tracking()
  {
    _current_tracking.clear();
  }

  void dependency_tracker::finalize_tracking(Tclause id)
  {
    assert (id < _clauses.size());
    assert (_clauses[id].in_use);
    assert (!_clauses[id].is_input);
    for (Tclause dep : _current_tracking) {
      assert (dep < _clauses.size());
      assert (_clauses[dep].in_use);
      _clauses[id].premises.insert(_clauses[dep].premises.begin(), _clauses[dep].premises.end());
    }
    _current_tracking.clear();
  }

  void dependency_tracker::delete_clause(Tclause id)
  {
    assert (id < _clauses.size());
    assert (_clauses[id].in_use);
    assert (!_clauses[id].is_input);
    _clauses[id].in_use = false;
    _clauses[id].premises.clear();
  }

  const std::set<Tclause>& dependency_tracker::get_dependencies(Tclause id)
  {
    assert (id < _clauses.size());
    assert (_clauses[id].in_use);
    return _clauses[id].premises;
  }

}
}
