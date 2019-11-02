/*****************************************************************************/
/*    This file is part of RAReQS.                                           */
/*                                                                           */
/*    rareqs is free software: you can redistribute it and/or modify         */
/*    it under the terms of the GNU General Public License as published by   */
/*    the Free Software Foundation, either version 3 of the License, or      */
/*    (at your option) any later version.                                    */
/*                                                                           */
/*    rareqs is distributed in the hope that it will be useful,              */
/*    but WITHOUT ANY WARRANTY; without even the implied warranty of         */
/*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          */
/*    GNU General Public License for more details.                           */
/*                                                                           */
/*    You should have received a copy of the GNU General Public License      */
/*    along with rareqs.  If not, see <http://www.gnu.org/licenses/>.        */
/*****************************************************************************/
/*
 * File:  Unit.cc
 * Author:  mikolas
 * Created on:  Thu Dec 29 22:29:15 CEST 2011
 * Copyright (C) 2011, Mikolas Janota
 */
#include "Unit.hh"
#include "utils.hh"

Unit::Unit(const CNF& cnf) 
  : conflict(false)
{
  VarSet used_values;
  add_clauses(cnf, used_values);
}

Unit::Unit(const CNF& cnf, const vec<lbool>& move)
  : conflict(false)
{
  for (Var i=0; i<move.size(); ++i) 
    if (move[i]!=l_Undef) set_value(i, move[i]);
  VarSet used_values;
  add_clauses(cnf, used_values);
  assert(original_clauses.size()==dirty_clauses.size());
  assert(original_clauses.size()==clauses.size());
}

Unit::Unit(const CNF& cnf, const vec<lbool>& move, VarSet& used_values) 
  : conflict(false)
{
  for (Var i=0; i<move.size(); ++i) 
    if (move[i]!=l_Undef) set_value(i, move[i]);
  add_clauses(cnf, used_values);
  assert(original_clauses.size()==dirty_clauses.size());
  assert(original_clauses.size()==clauses.size());
}

bool Unit::add_clauses(const CNF& cnf, VarSet& used_values) {
  FOR_EACH (clause_index, cnf) {
    if (!add_clause(*clause_index, used_values)) return false;
  }
  return true;
}


void Unit::eval(CNF& cnf) {
  if (conflict) {
    cnf.push_back(LitSet());
    return;
  }

  LiteralVector ls;
  for (size_t i=0; i < clauses.size(); ++i) {
    const LiteralVector& clause = clauses[i];
    ls.clear();
    bool taut=false;
    FOR_EACH(li, clause) {
      const Lit literal = *li;
      const lbool v = value(literal);
      if (v==l_Undef) {
        ls.push_back(literal);
      } else if(v==l_True) {
        taut=true;
        break;
      } else {
        assert (v==l_False);
        dirty_clauses[i]=true;
      }
    }
    if (!taut) {
      if (dirty_clauses[i]) { 
        cnf.push_back(LitSet(ls));
      } else {
        const LitSet& orig_cl = original_clauses[i];
        assert (LitSet(ls).equal(orig_cl));
        cnf.push_back(orig_cl);
      }
    }
  }
}

bool Unit::add_clause(const LitSet& clause, VarSet& used_values) {
  clauses.resize(clauses.size()+1);
  const size_t clause_index = clauses.size() - 1;
  LiteralVector& ls = clauses[clause_index];
  ls.clear();
  bool taut=false;
  bool change=false;
  FOR_EACH (li, clause) {
    const Lit literal = *li;
    const lbool     v = value(literal);
    if (v==l_Undef) ls.push_back(literal);
    else {
      change=true;
      used_values.add(var(literal));
      if (v==l_True) {
        taut=true;
        break;
      }
    }
  }
  if (taut) {
    clauses.pop_back();  // discard
    return true;
  }

  if (ls.size()==1) { // unit
    schedule(ls[0]); 
    clauses.pop_back();   // discard
    return true;
  }

  if (ls.size()==0) { //confl
    clauses.pop_back();
    conflict=true;
    return false;
  }

  assert(clause.size() >= 2);
  original_clauses.push_back(clause);
  assert(dirty_clauses.size()==clause_index);
  dirty_clauses.resize(clause_index+1,false);
  if (change) dirty_clauses[clause_index]=true;
  // setup watches
  watch(~ls[0], clause_index);
  watch(~ls[1], clause_index);
  return true;
}


bool Unit::propagate() {
  while (!trail.empty() && !conflict) {
    const Lit literal  = trail.back();
    trail.pop_back();
    if (!propagate(literal)) conflict=true;
  }
  return !conflict;
}

void Unit::schedule(Lit literal) {
  trail.push_back(literal);
}

void Unit::watch(Lit literal, size_t clause_index) {
  const size_t index = literal_index(literal);
  if (index >= watches.size()) watches.resize(index+1);
  auto& w = watches[index];
  w.push_back(clause_index);
}

void Unit::set_value(Var variable, lbool value) {
  const size_t index = (size_t) variable;
  if (index >= values.size()) values.resize(index+1, l_Undef);
  values[index]=value;
}


lbool Unit::value(Var variable) const {
  const size_t index = (size_t) variable;
  if (index >= values.size()) return l_Undef;
  return values[index];
}

lbool Unit::value(Lit literal) const {
  const lbool v = value(var(literal));
  if (v==l_Undef) return l_Undef;
  return (v==l_False) == sign(literal) ? l_True : l_False;
}

bool Unit::propagate(Lit literal) {
  const Var variable = var(literal);
  const lbool literal_value = sign(literal) ? l_False : l_True;
  if (value(variable) != l_Undef) {
    return (literal_value == value(variable));
  }
  set_value(variable, literal_value);
  const Lit false_literal = ~literal;
  const size_t li=literal_index(literal);
  if (li>=watches.size()) return true; 
  auto w = watches[li]; // TODO: could this be more efficient? (Need to copy because watches is being resized in the loop)
  bool return_value = true;     
  FOR_EACH (i, w) {
    const size_t clause_index = *i;
    assert(clause_index<clauses.size());
    LiteralVector& clause = clauses[clause_index];
    if (clause[0]==false_literal) {
      clause[0]=clause[1];
      clause[1]=false_literal;
    }
    assert(clause[1]==false_literal);
    if (value(clause[0]) == l_True) continue; // clause already true
    size_t new_watch = 0;
    for (size_t index = 2; index < clause.size(); ++index) { // find a new watch
      if (value(clause[index]) != l_False) {
        new_watch = index;
        break;
      }
    }
    if (new_watch > 0) { // new watch found
      assert(new_watch > 1);
      watch(~clause[new_watch], clause_index);
      clause[1]=clause[0];
      clause[0]=clause[new_watch];
      clause[new_watch]=false_literal;
    } else { // no new watch found
      if (value(clause[0]) == l_False) {
        return_value = false;
        break;
      }
      assert(value(clause[0]) == l_Undef);
      schedule(clause[0]);
    }
  }
  w.clear(); // TODO OK?
  return return_value;
}



bool any_fixed(const Unit& values, const VarVector& variables) {
  FOR_EACH (index, variables) {
    if (values.value(*index)!=l_Undef) return true;
  }
  return false;
}

size_t fix_unit(const Unit& values, const VarVector& variables, CNF& output) {
  size_t r=0;
  FOR_EACH(vi,variables) {
    const Var v = *vi;
    const lbool propagated_value = values.value(v);
    if (propagated_value!=l_Undef) {
      ++r;
      add_unit(output,  propagated_value == l_True ? mkLit(v) : ~mkLit(v));
    }
  }
  return r;
}

size_t fix_unit(const Unit& values, const VarSet& _variables, CNF& output) {
  size_t r=0;
  const vector<bool>& variables = _variables.bs();
  for (size_t index = 0; index < variables.size(); ++index) {
    if (!variables [index]) continue;
    const Var v = (Var) index;
    const lbool propagated_value = values.value(v);
    if (propagated_value!=l_Undef) {
      ++r;
      add_unit(output,  propagated_value == l_True ? mkLit(v) : ~mkLit(v));
    }
  }
  return r;
}
