/**
 * This file is part of the source code of the software program
 * NapSAT. It is protected by applicable copyright laws.
 *
 * This source code is protected by the terms of the MIT License.
 */
/**
 * @file src/proof/proof.cpp
 * @author Robin Coutelier
 * @brief This file is part of the NapSAT solver. It implements a simple resolution proof for the SAT solver.
 * It is used to check the correctness of the SAT solver on unsatisfiable instances.
 * @note This implementation is simplistic and does not aim to be efficient. A more efficient
 * version should be implemented in the future.
 */
#include "proof.hpp"

#include "SAT-types.hpp"
#include "../utils/printer.hpp"

#include <iostream>
#include <vector>
#include <unordered_set>
#include <cassert>
#include <string.h>
#include <algorithm>

using namespace std;

void napsat::proof::resolution_proof::apply_resolution(vector<Tlit>& base, unsigned resolvent_index, Tlit pivot)
{
  auto pivot_location = find(base.begin(), base.end(), pivot); // TODO can use binary search if too slow
  assert(pivot_location != base.end());
  base.erase(pivot_location);

  clause& resolvent = clauses[resolvent_index];
  for (unsigned i = 0; i < resolvent.size; i++) {
    if (resolvent.lits[i] == lit_neg(pivot))
      continue;

    base.push_back(resolvent.lits[i]);
  }
  sort(base.begin(), base.end());
  // remove duplicates
  unsigned j = 1;
  for (unsigned i = 1; i < base.size(); i++) {
    if (base[i] != base[i-1]) {
      base[j++] = base[i];
    }
  }
  if (base.size() != 0)
    base.resize(j);
}

void napsat::proof::resolution_proof::input_clause(napsat::Tclause id, const napsat::Tlit* lits, unsigned size)
{
  clauses.push_back(clause());
  clause &c = clauses.back();
  c.size = size;

  if (id >= clause_matches.size()) {
    clause_matches.reserve(2 * id + 1);
    clause_matches.resize(id + 1, CLAUSE_UNDEF);
  }

  assert(clause_matches[id] == CLAUSE_UNDEF);
  clause_matches[id] = clauses.size() - 1;

  if (size == 0) {
    empty_clause_id = clause_matches[id];
    return;
  }

  c.lits = new Tlit[size];
  memcpy(c.lits, lits, size * sizeof(Tlit));
  // sort the literals for convenience and faster search
  sort(c.lits, c.lits + size);
  // remove duplicates
  unsigned j = 1;
  for (unsigned i = 1; i < size; i++)
    if (c.lits[i] != c.lits[i-1])
      c.lits[j++] = c.lits[i];
  c.size = min(j, c.size);
}

void napsat::proof::resolution_proof::start_resolution_chain(void)
{
  assert(current_resolution_chain.size() == 0);
}

void napsat::proof::resolution_proof::link_resolution(napsat::Tlit pivot, napsat::Tclause id)
{
  assert(id < clause_matches.size());
  unsigned cl_num = clause_matches[id];
  assert(cl_num != CLAUSE_UNDEF);
  assert(current_resolution_chain.size() != 0 || pivot == LIT_UNDEF);
  current_resolution_chain.push_back(pair<Tlit, unsigned>(pivot, cl_num));
}

void napsat::proof::resolution_proof::finalize_resolution(napsat::Tclause id, const napsat::Tlit* lits, unsigned size)
{
  input_clause(id, lits, size);
  clause &c = clauses.back();

  c.resolution_chain = vector<pair<Tlit, unsigned>>(current_resolution_chain);
  current_resolution_chain.clear();

  assert(check_resolution_chain(clauses.size() - 1));
}

static unsigned binary_search(napsat::Tlit* lits, unsigned size, napsat::Tlit lit)
{
  unsigned left = 0;
  unsigned right = size;
  while (left < right) {
    unsigned mid = left + (right - left) / 2;
    if (lits[mid] == lit)
      return mid;
    if (lits[mid] < lit)
      left = mid + 1;
    else
      right = mid;
  }
  return size;
}

static void binary_insert(vector<napsat::Tlit>& lits, napsat::Tlit lit)
{
  unsigned left = 0;
  unsigned right = lits.size();
  while (left < right) {
    unsigned mid = left + (right - left) / 2;
    if (lits[mid] == lit)
      return;
    if (lits[mid] < lit)
      left = mid + 1;
    else
      right = mid;
  }
  lits.insert(lits.begin() + left, lit);
}

bool napsat::proof::resolution_proof::check_resolution_chain(unsigned index)
{
  clause &c = clauses[index];
  if (c.resolution_chain.size() == 0) {
    // input clause
    return true;
  }
  tmp_lits.clear();
  for (pair<Tlit, unsigned> link : c.resolution_chain) {
    Tlit pivot = link.first;
    assert(link.second < clauses.size());
    clause &cl = clauses[link.second];
    for (unsigned i = 0; i < cl.size; i++)
      binary_insert(tmp_lits, cl.lits[i]);

    // remove duplicates
    unsigned j = 1;
    for (unsigned i = 1; i < tmp_lits.size(); i++)
      if (tmp_lits[i] != tmp_lits[i-1])
        tmp_lits[j++] = tmp_lits[i];

    if (pivot == LIT_UNDEF)
      continue;

    unsigned pivot_index = binary_search(tmp_lits.data(), tmp_lits.size(), pivot);
    assert (pivot_index != tmp_lits.size());
    tmp_lits.erase(pivot_index + tmp_lits.begin());
    unsigned neg_pivot_index = binary_search(tmp_lits.data(), tmp_lits.size(), lit_neg(pivot));
    assert (neg_pivot_index != tmp_lits.size());
    tmp_lits.erase(neg_pivot_index + tmp_lits.begin());
  }

  if(tmp_lits.size() != c.size) {
    string error_msg = "The resolution chain does not match the clause\n";
    error_msg += "Resolution chain:\n";
    for (pair<Tlit, unsigned> link : c.resolution_chain) {
      error_msg += to_string(lit_to_int(link.first)) + " -> ";
      for (unsigned i = 0; i < clauses[link.second].size; i++)
        error_msg += to_string(lit_to_int(clauses[link.second].lits[i])) + " ";
      error_msg += "\n";
    }
    error_msg += "Actual clause (in DB): ";
    for (unsigned i = 0; i < c.size; i++) {
      error_msg += to_string(lit_to_int(c.lits[i])) + " ";
    }
    error_msg += "\n";
    error_msg += "Expected clause (calculated): ";
    for (Tlit lit : tmp_lits)
      error_msg += to_string(lit_to_int(lit)) + " ";
    error_msg += "\n";
    LOG_ERROR(error_msg);
    return false;
  }

  for (unsigned i = 0; i < c.size; i++) {
    if (c.lits[i] != tmp_lits[i]) {
      string error_msg = "Error: resolution chain does not match the clause\n";
      error_msg += "Resolution chain:\n";
      for (pair<Tlit, unsigned> link : c.resolution_chain) {
        error_msg += to_string(lit_to_int(link.first)) + " -> ";
        for (unsigned i = 0; i < clauses[link.second].size; i++)
          error_msg += to_string(lit_to_int(clauses[link.second].lits[i])) + " ";
        error_msg += "\n";
      }
      error_msg += "Expected clause: ";
      for (unsigned i = 0; i < c.size; i++)
        error_msg += to_string(lit_to_int(c.lits[i])) + " ";
      error_msg += "\n";
      error_msg += "Actual clause: ";
      for (Tlit lit : tmp_lits)
        error_msg += to_string(lit_to_int(lit)) + " ";
      error_msg += "\n";
      LOG_ERROR(error_msg);
      return false;
    }
  }
  return true;
}

void napsat::proof::resolution_proof::root_assign(napsat::Tlit lit, napsat::Tclause reason)
{
  root_lit.push_back(lit);
  root_reason.push_back(reason);
}

void napsat::proof::resolution_proof::remove_root_literals(napsat::Tclause id)
{
  assert(clause_matches[id] != CLAUSE_UNDEF);
  clause &c = clauses[clause_matches[id]];

  Tlit* end = c.lits + c.size;

  vector<Tlit> simplified_clause(c.lits, end);

  start_resolution_chain();
  link_resolution(LIT_UNDEF, id);

  unsigned i = root_lit.size();
  while (i > 0) {
    while (i > 0 && find(c.lits, end, lit_neg(root_lit[i-1])) == end) // TODO can use binary search if too slow
      i--;
    if (i == 0)
      break;

    link_resolution(lit_neg(root_lit[i-1]), root_reason[i-1]);
    apply_resolution(simplified_clause, clause_matches[root_reason[i-1]], lit_neg(root_lit[i-1]));
    i--;
  }
  // this is tricky because we want to replace the clause id with the new one.
  // this is why we need to delete it
  deactivate_clause(id);
  finalize_resolution(id, simplified_clause.data(), simplified_clause.size());
}

void napsat::proof::resolution_proof::deactivate_clause(napsat::Tclause id)
{
  assert(id < clause_matches.size());
  assert(clause_matches[id] != CLAUSE_UNDEF);
  clause_matches[id] = CLAUSE_UNDEF;
}

bool napsat::proof::resolution_proof::check_proof(void)
{
  assert(empty_clause_id != CLAUSE_UNDEF);
  vector<unsigned> clauses_to_check;
  clauses_to_check.push_back(empty_clause_id);
  while(!clauses_to_check.empty()) {
    unsigned index = clauses_to_check.back();
    clauses[index].marked = true;
    clauses_to_check.pop_back();
    if (!check_resolution_chain(index))
      return false;
    clause &c = clauses[index];
    for (pair<Tlit, unsigned> link : c.resolution_chain)
      if (!clauses[link.second].marked)
        clauses_to_check.push_back(link.second);
  }
  for (unsigned i = 0; i < clauses.size(); i++)
    clauses[i].marked = false;
  return true;
}

void napsat::proof::resolution_proof::print_clause(unsigned index)
{
  assert(index < clauses.size());
  clause &c = clauses[index];
  for (unsigned i = 0; i < c.size; i++) {
    cout << lit_to_int(c.lits[i]);
    if (i != c.size - 1)
      cout << " ";
  }
}

void napsat::proof::resolution_proof::print_resolution_chain(unsigned index) {
  assert(index < clauses.size());
  clause &c = clauses[index];
  vector<Tlit> base;
  // first clause in the resolution chain
  unsigned first_index = c.resolution_chain[0].second;
  clause cl = clauses[first_index];

  for (unsigned i = 0; i < cl.size; i++)
    base.push_back(cl.lits[i]);

  string last_clause_number = to_string(first_index);
  assert(c.resolution_chain.size() >= 2);
  for (unsigned i = 1; i < c.resolution_chain.size(); i++) {
    pair<Tlit, unsigned> link = c.resolution_chain[i];
    apply_resolution(base, link.second, link.first);
    cl = clauses[link.second];

    if (i != c.resolution_chain.size() - 1)
      cout << index << "." << i-1 << ": (";
    else
      cout << index << ": (";

    for (unsigned i = 0; i < base.size(); i++) {
      cout << lit_to_int(base[i]);
      if (i != base.size() - 1)
        cout << " ";
    }
    cout << ") [resolution " << last_clause_number << ", " << link.second << "]\n";
    last_clause_number = to_string(index) + "." + to_string(i-1);
  }
}

void napsat::proof::resolution_proof::print_proof(void)
{
  assert(empty_clause_id != CLAUSE_UNDEF);
  vector<unsigned> clauses_to_check;
  clauses_to_check.push_back(empty_clause_id);
  while(!clauses_to_check.empty()) {
    unsigned index = clauses_to_check.back();
    clauses_to_check.pop_back();
    clause &c = clauses[index];
    c.marked = true;
    for (pair<Tlit, unsigned> link : c.resolution_chain)
      clauses_to_check.push_back(link.second);
  }

  for (unsigned index = 0; index < clauses.size(); index++) {
    clause& c = clauses[index];
    if (!c.marked)
      continue;

    c.marked = false;
    if (c.resolution_chain.size() == 0) {
      cout << index << ": (";
      print_clause(index);
      cout << ") [input]\n";
      continue;
    }
    print_resolution_chain(index);
  }
}

void napsat::proof::resolution_proof::print_clause_matches(void)
{
  cout << "Clause matches:\n";
  for (unsigned i = 0; i < clause_matches.size(); i++)
    cout << i << " -> " << clause_matches[i] << endl;
  cout << "Empty clause: " << empty_clause_id << endl;
}

void napsat::proof::resolution_proof::print_clause_set(void)
{
  cout << "Clauses:\n";
  for (unsigned i = 0; i < clauses.size(); i++) {
    cout << i << ": ";
    print_clause(i);
    cout << endl;
  }
}

napsat::proof::resolution_proof::~resolution_proof()
{
  for (clause c : clauses)
    if (c.size != 0)
      delete[] c.lits;
}
