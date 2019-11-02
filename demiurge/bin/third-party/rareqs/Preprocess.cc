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
 * File:  Preprocess.cc
 * Author:  mikolas
 * Created on:  Sun Jan 15 12:57:14 GMTST 2012
 * Copyright (C) 2012, Mikolas Janota
 */
#include "Preprocess.hh"
#define PREPROCESS_DBG(t)
#define PREPROCESS_ST(t)

Preprocess::Preprocess(const Fla& formula, VarManager& vm, size_t _indent) 
  : ur(formula)
  , original_formula(use_universal_reduction ? ur.preprocess() : formula)
  , variable_manager(vm)
  , indent(_indent)
{
  if (verbose>4) {
    out() << "preprocessing of " << original_formula << endl;
  }
}

void Preprocess::reconstruct(const vec<lbool>& preprocessed_move, vec<lbool>& original_move) {
  original_move.growTo(std::max(preprocessed_move.size(), reconstruction_values.size()), l_Undef);
  for (Var vi=0; vi<preprocessed_move.size(); ++vi) original_move[vi]=preprocessed_move[vi];  
  for (Var vi=0; vi<reconstruction_values.size(); ++vi) {
    if(reconstruction_values[vi]==l_Undef) continue;
    original_move[vi]=reconstruction_values[vi];
  }
}

const Fla& Preprocess::preprocess() {
  vector <bool> top;
  vector < vector <bool> > inner;
  traverse(original_formula, top, inner);
  VariableVector variables;
  PREPROCESS_ST( out() << "prep" << endl; );
  PREPROCESS_ST(const double t0 = read_cpu_time(););
  // computer construction values for the top prefix
  FOR_EACH(vi, original_formula.pref) {
    const Var       v = *vi;
    const bool has_pl =  has_literal(mkLit(v), top);
    const bool has_nl =  has_literal(~mkLit(v), top);
    if (has_nl && has_pl) {
      variables.push_back(v);
      continue;
    }else {
      if (has_nl || has_pl) {
        const lbool fixed = (original_formula.q==EXISTENTIAL ? has_pl : has_nl) ? l_True : l_False;
        put_value(reconstruction_values, v, fixed);
      } else {
        put_value(reconstruction_values, v, l_False);
      }
    }
  }
  preprocessed_formula.pref=VarVector(variables);
  preprocessed_formula.q=original_formula.q;
  preprocessed_formula.op=original_formula.op;
  preprocessed_formula.flas.resize(original_formula.flas.size());
  lbool fv=l_Undef;
  RANGE (formula_index, 0, original_formula.flas.size ()) {
    const QFla& original_subformula = original_formula.flas[formula_index];
    QFla& preprocessed_subformula   = preprocessed_formula.flas[formula_index];
    fv = preprocess_subformula(original_subformula, inner[formula_index], preprocessed_formula, preprocessed_subformula);
    if (fv==l_False) break;
  }

  if (fv==l_False) {
    PREPROCESS_ST( out() << "const " << fv << endl );
    preprocessed_formula.flas.clear();
    QFla f;
    if (original_formula.q==EXISTENTIAL) f.cnf.push_back(LitSet());
    preprocessed_formula.flas.push_back(f);
  } 
    
  PREPROCESS_DBG( out() << original_formula << "->"  << preprocessed_formula << endl; 
                 out() << "t: " << (read_cpu_time()-t0) << endl;   );
  PREPROCESS_ST( out() << "tpr: " << original_formula.pref.size() << "->"  << preprocessed_formula.pref.size() << endl; 
                 out() << "tfr: " << original_formula.flas.size() << "->"  << preprocessed_formula.flas.size() << endl;
                 out() << "t: " << (read_cpu_time()-t0) << endl;   );
  return preprocessed_formula;
}

lbool Preprocess::preprocess_subformula(const QFla& original_subformula, const vector<bool>& inner, 
                                        Fla& preprocessed_formula,
                                        QFla& preprocessed_subformula) {
  vec<SUBSTITUTION_VALUE> substitution;
  // initialize a substitution with the substitution for top prefix
  for (int i=0; i<reconstruction_values.size(); ++i) {
    const lbool rv = reconstruction_values[i];
    if (rv==l_Undef) continue;
    put_value(substitution, i, rv==l_True ? SET_TRUE : SET_FALSE);
  }

  Prefix preprocessed_prefix;
  VariableVector variables;
  FOR_EACH(qi, original_subformula.pref) {
    const Quantification& quantification = *qi;
    variables.clear();
    FOR_EACH(vi, quantification.second) {
      const Var       v = *vi;
      const bool has_pl =  has_literal(mkLit(v), inner);
      const bool has_nl =  has_literal(~mkLit(v), inner);
      if (has_nl && has_pl) {
        variables.push_back(v);
        continue;
      } else {
        if (has_nl || has_pl) {
          const SUBSTITUTION_VALUE fixed = (quantification.first==EXISTENTIAL ? has_pl : has_nl) ? SET_TRUE : SET_FALSE;   
          put_value(substitution, v, fixed);
        }
      }
    }
    const size_t osz = quantification.second.size();
    const size_t nsz = variables.size();
    PREPROCESS_DBG( out() << "red: "<< osz << "->" << nsz << endl; );
    if (nsz) {
      const VarVector& rp =  osz==nsz ?  quantification.second : VarVector(variables);
      preprocessed_prefix.push_back(Quantification(quantification.first, rp));
    }
  }

  size_t start = 0;
  size_t stop = preprocessed_prefix.size();
  // remove universal quantifiers at the end
  while (stop && (preprocessed_prefix[stop-1].first==UNIVERSAL)) { 
    --stop;
    FOR_EACH(vi, preprocessed_prefix[stop].second) { 
      PREPROCESS_DBG( out() << "removing:" <<*vi <<endl; )
        put_value(substitution, *vi, REMOVE);
    }
  }
  preprocessed_prefix.resize(stop);

  // if the prefix starts with the same quantifier a the whole multi-game, it gets to go into the top-level prefix
  while ((start<stop) && (preprocessed_prefix[start].first==preprocessed_formula.q) ) ++start;

  PREPROCESS_DBG( out() << " pp:" <<preprocessed_prefix << endl; );

  // collate levels with the same quantifier
  for (size_t i = start; i<stop;  ) {
    size_t j=i+1;
    while ( (j<stop) && (preprocessed_prefix[i].first==preprocessed_prefix[j].first) ) ++j;
    if (j==(i+1)) preprocessed_subformula.pref.push_back(preprocessed_prefix[i]);
    else {
      VariableVector vs;
      for (size_t k=i; k<j; ++k) append(vs, preprocessed_prefix[k].second);
      preprocessed_subformula.pref.push_back(Quantification(preprocessed_prefix[i].first, VarVector(vs)));
    }
    i=j;
  }

  // perform the substitution on the cnf
  if (start>0 && (stop > 0)) {
    CNF temp;
    subst(original_subformula.cnf, substitution, temp);
    // freshen variables that get merged into the top-level prefix
    vec<Var> oldToPrime;  
    VarSet to_fresh;
    for (size_t index = 0; index<start; ++index) {
      FOR_EACH(vi,preprocessed_prefix[index].second) { 
        to_fresh.add(*vi);
      }
    }
    freshen(temp, to_fresh, preprocessed_subformula.cnf, oldToPrime, variable_manager);
    VariableVector to_freshv;
    for (int i=0; i< oldToPrime.size(); ++i) if (oldToPrime[i]>0) to_freshv.push_back(oldToPrime[i]);
    preprocessed_formula.pref = append(preprocessed_formula.pref, to_freshv);
  } else {
    subst(original_subformula.cnf, substitution, preprocessed_subformula.cnf);
  }
  PREPROCESS_DBG( out() << original_subformula << "->"  << preprocessed_subformula << endl; );
  PREPROCESS_ST( out() << original_subformula.pref.size() << "->"  << preprocessed_subformula.pref.size() << endl; );

  const bool is_tautology = preprocessed_subformula.cnf.empty();
  const bool is_unsatisfiable = !preprocessed_subformula.cnf.empty() && preprocessed_subformula.cnf[0].empty();
  const QuantifierType q = original_formula.q;
  PREPROCESS_DBG( out() << "tautology:" << is_tautology << endl );
  PREPROCESS_DBG( out() << "unsatisfiable:" << is_unsatisfiable << endl );
  if (is_tautology)  return q==EXISTENTIAL ? l_True : l_False;
  if (is_unsatisfiable)  return q==EXISTENTIAL ? l_False : l_True;
  return l_Undef;
}
