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
 * File:  Univ.cc
 * Author:  mikolas
 * Created on:  Wed Jan 25 15:45:32 GMTST 2012
 * Copyright (C) 2012, Mikolas Janota
 */
#include "Univ.hh"
#include "VarSet.hh"
#include <algorithm>
#include <vector>
#include <assert.h>
using std::sort;

Univ::Univ(const Fla& _formula) 
  : formula(_formula)
{
  assert( (formula.op==AND)==(formula.q==EXISTENTIAL) );
}

inline void set_level(vector<size_t>& levels, Var variable, size_t level) {
  assert(variable>=0);
  const size_t vi = (size_t)variable;
  if (levels.size()<=vi) levels.resize(vi+1,-1);
  levels[vi]=level;
}

inline size_t get_level(const vector<size_t>& levels, Var variable) {
  assert(variable>=0);
  const size_t vi = (size_t)variable;
  assert(levels.size() > vi);
  return levels[vi];
}

inline size_t get_level(const vector<size_t>& levels, Lit literal) { return get_level(levels, var(literal)); }


struct LevCmp {
  LevCmp(vector<size_t>& _levels) : levels(_levels)  { 
    //  int i=0; FOR_EACH (l,levels) cerr << i++ << ":" << *l << " "; cerr << endl;
  }

  inline bool operator() (Lit i, Lit j) { return get_level(levels,i) < get_level(levels,j);}
  const vector<size_t>& levels;
};


const Fla& Univ::preprocess() {
  preprocess(_preprocessed);
  return _preprocessed;
}

void Univ::preprocess(Fla& output) {
  vector<size_t> levels;
  VarSet         universals;

  // top prefix assigned level 0
  FOR_EACH(vi, formula.pref) {
    set_level(levels, *vi, 0);
    if (formula.q==UNIVERSAL) universals.add(*vi);
  }

  output.q=formula.q;
  output.pref=formula.pref;
  output.op=formula.op;

  FOR_EACH(fi, formula.flas) {
    const QFla& subformula = *fi;
    output.flas.resize(output.flas.size()+1);
    QFla& output_subformula=output.flas.back();
 
    size_t level = 0;
    FOR_EACH(q, subformula.pref)  {
      ++level;
      output_subformula.pref.push_back(*q);
      FOR_EACH(vi, q->second) {
        set_level(levels, *vi, level);
        if (q->first==UNIVERSAL) universals.add(*vi);
      }
    }

    vector<Lit> ls;
    const bool can_remove_top_level = formula.q==UNIVERSAL && formula.flas.size()==1;
    FOR_EACH(ci, subformula.cnf) {
      const LitSet& clause = *ci;
      ls.clear();
      FOR_EACH(li, clause) ls.push_back(*li);
      sort(ls.begin(),ls.end(), LevCmp(levels));

      if (can_remove_top_level && ls.size() && (get_level(levels, ls.back())==0)) {
        output_subformula.cnf.clear();
        output_subformula.cnf.push_back(clause);
        break;
      }

      while (ls.size()) {
        const Var variable = var(ls.back());
        if (universals.get(variable) && get_level(levels,variable)) ls.pop_back();
        else break;
      }

      if (ls.empty()) {
        output_subformula.cnf.clear();
        output_subformula.cnf.push_back(LitSet());
        break;
      }

      if (ls.size() != clause.size()) output_subformula.cnf.push_back(LitSet(ls));
      else {
        assert(clause.equal(LitSet(ls)));
        output_subformula.cnf.push_back(clause);
      }
    }
  }
}
