/*
 * File:  utils.hh
 * Author:  mikolas
 * Created on:  Mon Nov 14 17:54:26 EST 2011
 * Copyright (C) 2011, Mikolas Janota
 */
#ifndef UTILS_HH_26370
#define UTILS_HH_26370
#include "qtypes.hh"
#include "LitSet.hh"
#include "VarSet.hh"
#include <ostream>
using std::ostream;

class VarManager {
public:
  virtual Var new_var() = 0;
};


enum SUBSTITUTION_VALUE { NOOP, SET_TRUE, SET_FALSE, REMOVE };

void subst(const vector<LitSet>& cnf, const vec<lbool>& move, vector<LitSet>& res);
void subst(const vector<LitSet>& cnf, const vec<SUBSTITUTION_VALUE>& substitution, vector<LitSet>& res);
void subst(const vector<LitSet>& cnf, const vec<lbool>& substitution, VarSet& used, vector<LitSet>& res);

vec<lbool>& copy_values(vec<lbool>&  destination, const vec<lbool> &source, const VarVector& variables);
vec<Lit>& filter(vec<Lit>& destination, const vec<Lit>& source, const VarSet& variables);


inline vec<lbool>&  put_value(vec<lbool>&  destination, int index, lbool value) {
  if (index >= destination.size()) destination.growTo(index + 1, l_Undef);
  destination[index] = value;
  return destination;
}

inline vec<SUBSTITUTION_VALUE>&  put_value(vec<SUBSTITUTION_VALUE>&  destination, int index, SUBSTITUTION_VALUE value) {
  if (index >= destination.size()) destination.growTo(index + 1, NOOP);
  destination[index] = value;
  return destination;
}


ostream& print(ostream& o, const VarVector& vs, const vec<lbool>& move);

CNF& add_unit (CNF& cnf, Lit l);

inline ostream & operator << (ostream& outs, const vec<lbool>& vs) {
  for (int i=1;i<vs.size();++i) {
      outs << " ";
      const lbool v = vs[i];
      if (v==l_True) outs << "+";
      else if (v==l_False) outs << "-";
      else outs << "?";
      outs << i;
  }
  return outs;
}

inline ostream& make_indent(ostream & output_stream, size_t indent) {
  size_t i=indent; 
  while (i) { output_stream<<" "; --i; } 
  return output_stream;
}

inline VarVector append(const VarVector& a, const VariableVector& b) {
  if (b.size()) {
    VariableVector vs;
    FOR_EACH (vi, a) vs.push_back(*vi);
    FOR_EACH (vi, b) vs.push_back(*vi);
    return VarVector(vs);
  } else {
    return a;
  }
}

inline VarVector append(const VarVector& a, const VarVector& b) {
  if (a.empty()) return b;
  if (b.empty()) return a;
  VariableVector vs;
  FOR_EACH (vi, a) vs.push_back(*vi);
  FOR_EACH (vi, b) vs.push_back(*vi);
  return VarVector(vs);
}

inline VariableVector& append(VariableVector& destination, const VariableVector& src) {
  FOR_EACH(vi,src) destination.push_back(*vi);
  return destination;
}

inline VariableVector& append(VariableVector& destination, const VarVector& src) {
  FOR_EACH(vi,src) destination.push_back(*vi);
  return destination;
}

/*  Debugging */
bool unique(const VarVector& variables);
bool unique_(VariableVector& variables);
bool closed(const CNF& cnf, const VarVector& variables);
bool closed(const vec<Lit>& literals, const VarVector& variables);
bool closed(const vec<Lit>& literals, const VarSet& variables);
bool closed(const Fla& formula);
ostream& traverse_stats(ostream& output, const Fla& f, const vector<bool>& top_lits, const vector< vector<bool> >&  inner);
/**************/

void freshen(const CNF& cnf, const VarSet& variables, CNF& output, vec<Var>& oldToPrime, VarManager& ids);

void traverse(const Fla& f, vector<bool>& top_lits, vector< vector<bool> >&  inner);

inline bool has_literal (Lit l, const vector<bool>& occurrences) {
  const size_t index = literal_index(l);
  return (index < occurrences.size()) && occurrences[index];
}

inline Var max(const VarVector& variables) {
  Var max_id = -1;
  FOR_EACH(variable_index, variables) {
    const Var v = *variable_index;
    assert (v>=0);
    if (max_id < v) max_id = v;
  }
  return max_id;
}
#endif /* UTILS_HH_26370 */
