/*
 * This file is part of the source code of the software program
 * NapSAT. It is protected by applicable copyright laws.
 *
 * This source code is protected by the terms of the MIT License.
 */
/**
 * @file src/solver/NapSAT-proofs.cpp
 * @author Robin Coutelier
 *
 * @brief This file is part of the NapSAT solver. It implements some utilities to finalize proofs.
 */
#include "NapSAT.hpp"

#include "custom-assert.hpp"

#include <iostream>
#include <cstring>
#include <functional>
#include <chrono>

using namespace napsat;
using namespace std;

void napsat::NapSAT::prove_root_literal_removal(Tlit* lits, unsigned size)
{
  ASSERT(_proof);
  // we assume that a resolution chain is already started
  unsigned count = 0;

  for (unsigned i = 0; i < size; i++) {
    ASSERT(lit_false(lits[i]));
    if (lit_level(lits[i]) != LEVEL_ROOT)
      continue;
    if (!lit_marked(lits[i])) {
      count++;
    }
    lit_mark(lits[i]);
  }

  unsigned i = _trail.size() - 1;
  while (count != 0) {
    while (!lit_marked(_trail[i]))
      i--;
    Tlit lit = _trail[i];
    ASSERT(lit_level(lit) == LEVEL_ROOT);
    ASSERT(lit_reason(lit) != CLAUSE_UNDEF);
    Tclause reason = lit_reason(lit);
    ASSERT(reason != CLAUSE_UNDEF);
    _proof->link_resolution(~lit, reason);

    for (unsigned j = 1; j < clause_size(reason); j++) {
      Tlit lit = clause_lits(reason)[j];
      ASSERT(lit_false(lit));
      if (lit_marked(lit))
        continue;
      lit_mark(lit);
      count++;
    }
    count--;
    lit_unmark(lit);
  }
}

void napsat::NapSAT::prove_lazy_merge_unsat(Tclause cl)
{
  ASSERT(_options->lazy_chunk_merging);
  ASSERT(_proof);
  ASSERT(check_clause_falsified(cl));
  _proof->start_resolution_chain();
  _proof->link_resolution(LIT_UNDEF, cl);

  unsigned n_lits_remaining = 0;
  for (unsigned i = 0; i < clause_size(cl); i++) {
    Tlit lit = clause_lits(cl)[i];
    ASSERT(!lit_marked(lit));
    lit_mark(lit);
    n_lits_remaining++;
  }

  unsigned index = _trail.size();
  while (n_lits_remaining > 0) {
    ASSERT(index > 0);
    Tlit lit = _trail[--index];
    if (!lit_marked(lit))
      continue;
    Tclause reason = lit_reason(lit);
    if (reason == CLAUSE_UNDEF) {
      // this must be a lazy chunk merge
      reason = lit_lazy_reason(lit);
    }
    ASSERT(reason != CLAUSE_UNDEF);
    ASSERT(_clauses[reason].lits[0] == lit);
    for (unsigned i = 1; i < clause_size(reason); i++) {
      Tlit lit = clause_lits(reason)[i];
      if (lit_marked(lit))
        continue;
      lit_mark(lit);
      n_lits_remaining++;
    }

    n_lits_remaining--;
    _proof->link_resolution(~lit, reason);
    lit_unmark(lit);
  }

  _proof->finalize_resolution(_clauses.size(), nullptr, 0);
}
