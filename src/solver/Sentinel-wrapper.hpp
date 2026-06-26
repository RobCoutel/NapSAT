/**
 * @file Type-converter.hpp
 * @author Robin Coutelier
 * @brief This file is part of the NapSAT solver. It implements functions to convert between the types used in the NapSAT solver and the types used in the Sentinel API.
 */

#pragma once

#include "SAT-types.hpp"
#include "Sentinel-API.hpp"



namespace sentinel::wrapper
{
  inline sentinel::Tvar convert(napsat::Tvar var) {
    return sentinel::Tvar(var.value);
  }
  inline sentinel::Tlit convert(napsat::Tlit lit) {
    return sentinel::Tlit(convert(lit.var()), lit.pol());
  }
  inline sentinel::Tclause convert(napsat::Tclause clause) {
    return sentinel::Tclause(clause.value);
  }
  inline sentinel::Tlevel convert(napsat::Tlevel level) {
    return sentinel::Tlevel(level.value);
  }

  inline sentinel::SATSentinel* create_sentinel(const sentinel::SentinelOptions& options) {
    return sentinel::create_sentinel(options);
  }

  inline void delete_sentinel(sentinel::SATSentinel* sentinel) {
    sentinel::delete_sentinel(sentinel);
  }

  inline bool add_variable(sentinel::SATSentinel* sentinel, napsat::Tvar var) {
    return sentinel::add_variable(sentinel, convert(var));
  }
  inline bool set_variable_alias(sentinel::SATSentinel* sentinel, napsat::Tvar var, std::string alias) {
    return sentinel::set_variable_alias(sentinel, convert(var), alias);
  }

  inline bool add_clause(sentinel::SATSentinel* sentinel, napsat::Tclause cl, const napsat::Tlit* lits, unsigned int size, bool external = false) {
    std::vector<sentinel::Tlit> sentinel_lits;
    for (unsigned int i = 0; i < size; i++) {
      sentinel_lits.push_back(convert(lits[i]));
    }
    return sentinel::add_clause(sentinel, convert(cl), sentinel_lits.data(), size, external);
  }
  inline bool delete_clause(sentinel::SATSentinel* sentinel, napsat::Tclause clause) {
    return sentinel::delete_clause(sentinel, convert(clause));
  }
  inline bool shrink_clause(sentinel::SATSentinel* sentinel, napsat::Tclause clause, napsat::Tlit removed_lit) {
    return sentinel::shrink_clause(sentinel, convert(clause), convert(removed_lit));
  }

  inline bool assign  (sentinel::SATSentinel* sentinel, napsat::Tlit blocker, napsat::Tclause reason = napsat::CLAUSE_UNDEF) {
    return sentinel::assign(sentinel, convert(blocker), convert(reason));
  }
  inline bool unassign(sentinel::SATSentinel* sentinel, napsat::Tlit blocker) {
    return sentinel::unassign(sentinel, convert(blocker));
  }

  inline bool propagate  (sentinel::SATSentinel* sentinel, napsat::Tlit blocker) {
    return sentinel::propagate(sentinel, convert(blocker));
  }
  inline bool unpropagate(sentinel::SATSentinel* sentinel, napsat::Tlit blocker) {
    return sentinel::unpropagate(sentinel, convert(blocker));
  }

  inline bool update_level(sentinel::SATSentinel* sentinel, napsat::Tlit blocker, napsat::Tlevel level) {
    return sentinel::update_level(sentinel, convert(blocker), convert(level));
  }
  inline bool update_reason(sentinel::SATSentinel* sentinel, napsat::Tlit blocker, napsat::Tclause reason) {
    return sentinel::update_reason(sentinel, convert(blocker), convert(reason));
  }

  inline bool watch(sentinel::SATSentinel* sentinel, napsat::Tclause clause, napsat::Tlit blocker) {
    return sentinel::watch(sentinel, convert(clause), convert(blocker));
  }
  inline bool unwatch(sentinel::SATSentinel* sentinel, napsat::Tclause clause, napsat::Tlit blocker) {
    return sentinel::unwatch(sentinel, convert(clause), convert(blocker));
  }
  inline bool block(sentinel::SATSentinel* sentinel, napsat::Tclause clause, napsat::Tlit blocker, napsat::Tlit watch = napsat::LIT_UNDEF) {
    return sentinel::block(sentinel, convert(clause), convert(blocker), convert(watch));
  }

  inline bool check_invariants(sentinel::SATSentinel* sentinel) {
    return sentinel::check_invariants(sentinel);
  }
  inline bool checkpoint(sentinel::SATSentinel* sentinel) {
    return sentinel::checkpoint(sentinel);
  }
  inline bool message(sentinel::SATSentinel* sentinel, const std::string& msg, unsigned level = 0) {
    return sentinel::message(sentinel, msg, level);
  }

  inline bool save_execution(sentinel::SATSentinel* sentinel, std::string filename) {
    return sentinel::save_execution(sentinel, filename);
  }
  inline bool load_execution(sentinel::SATSentinel* sentinel, std::string filename) {
    return sentinel::load_execution(sentinel, filename);
  }

  inline void set_command_parser(sentinel::SATSentinel* sentinel, sentinel::Tparser* parser) {
    sentinel::set_command_parser(sentinel, parser);
  }
}
