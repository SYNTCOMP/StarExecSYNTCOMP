/*
 * File:  Unit.hh
 * Author:  mikolas
 * Created on:  Thu Dec 29 22:29:07 CEST 2011
 * Copyright (C) 2011, Mikolas Janota
 */
#ifndef UNIT_HH_24023
#define UNIT_HH_24023
#include "minisat_auxiliary.hh"
#include "qtypes.hh" 
#include "VarSet.hh"
#include "VarVector.hh"

using Minisat::lbool;
class Unit {
public:
  Unit(const CNF& cnf, const vec<lbool>& move, VarSet& used_values);
  Unit(const CNF& cnf, const vec<lbool>& move);
  Unit(const CNF& cnf);
  bool                           propagate();
  lbool                          value(Var variable) const;
  void                           eval(CNF& cnf);
  void                           eval(CNF& cnf, VarSet& used_values);
  Var                            size() { return values.size(); }
private:
  bool                           conflict;           // a conflict was reached
  CNF                            original_clauses;   // original_clauses[i] corresponds to clauses[i]
  vector<bool>                   dirty_clauses;      // dirty_clauses[i] iff clause[i] modified  
  vector< vector<size_t> >       watches;
  vector<LiteralVector>          clauses;
  vector<Lit>                    trail;
  vector<lbool>                  values;
  lbool                          value(Lit literal) const;
  void                           mark_dirty(size_t clause_index);
  bool                           is_dirty(size_t clause_index) const;
  void                           set_value(Var variable, lbool value);
  bool                           propagate(Lit literal);
  bool                           add_clause(const LitSet& clause, VarSet& used_values);
  void                           schedule(Lit literal);
  bool                           add_clauses(const CNF& cnf, VarSet& used_values);
  void                           watch(Lit literal, size_t clause_index);
  inline size_t literal_index(Lit l) {
    assert(var(l)>=0);
    const size_t vi = (size_t) var(l);
    return sign(l) ? 2*vi : 2*vi+1;
  }
};

size_t fix_unit(const Unit& values, const VarVector& variables, CNF& output);
size_t fix_unit(const Unit& values, const VarSet& variables, CNF& output);
bool   any_fixed(const Unit& values, const VarVector& variables);

#endif /* UNIT_HH_24023 */
