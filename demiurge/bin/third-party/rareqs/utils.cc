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
 * File:  utils.cc
 * Author:  mikolas
 * Created on:  Mon Nov 14 17:54:18 EST 2011
 * Copyright (C) 2011, Mikolas Janota
 */
#include "utils.hh"
#include "auxiliary.hh"
#include "VarSet.hh"
#include <algorithm>
#define __SET

bool empty_substitution(const vec<lbool>& substitution) {
  for (int index = 0; index < substitution.size(); ++index) {
    if (substitution[index]!=l_Undef) return false;
  }
  return true;
}

void freshen (const CNF& cnf, const VarSet& variables, CNF& output, vec<Var>& oldToPrime, VarManager& ids) {
  if (variables.empty()) {
    output=cnf;
    return;
  }

  LiteralVector ls;
  FOR_EACH(cli,cnf) {
    LitSet cl = *cli; 
    ls.clear();
    bool change=false;
    FOR_EACH(li,cl) {
      const Lit& l = *li;
      const Var  v = var(l);
      const bool lsign = sign(l);
      if (!variables.get(v)) {
        ls.push_back(l);  // no change
      } else {
        if (oldToPrime.size() <= v) oldToPrime.growTo(v+1,0);
        if (oldToPrime[v]==0) {
          oldToPrime[v]=ids.new_var();
        }
        ls.push_back(mkLit(oldToPrime[v],lsign));                       // primed variable
        change=true;
      }
    }
    if (change) output.push_back(LitSet(ls));
    else output.push_back(cl);
  }
}

CNF& add_unit (CNF& cnf,Lit l) {
  vector<Lit> ls(1);
  assert(ls.size()==1);
  ls[0] = l;
  cnf.push_back(LitSet(ls));
  return cnf;
}


vec<Lit>& filter(vec<Lit>& destination, const vec<Lit>& source, const VarSet& variables) {
  RANGEA(i, 0, source.size()) {
    const Lit literal  = source[i];
    const Var variable = Minisat::var(literal);
    if (!variables.get(variable)) continue;
    destination.push(literal);
  }
  return destination;
}

bool closed(const vec<Lit>& literals, const VarVector& variables) {
  VarSet variable_set(variables);
  return closed(literals, variable_set);
}

bool closed(const vec<Lit>& literals, const VarSet& variable_set) {
  RANGEA(i, 0, literals.size()) {
    const Var variable = Minisat::var(literals[i]);
    if (!variable_set.get(variable)) {
      cerr << "unquantified variable: " << variable << endl;
      return false;
    }
  }
  return true;
}

bool closed(const CNF& cnf, const VarSet& variable_set) {
  FOR_EACH(cli, cnf) {
    FOR_EACH(li, *cli) {
      const Var variable = Minisat::var(*li);
      if (!variable_set.get(variable)) {
        cerr << "unquantified variable: " << variable << endl;
        return false;
      }
    }
  }
  return true;
}

bool closed(const CNF& cnf, const VarVector& variables) 
{ return closed(cnf, VarSet(variables)); }


vec<lbool>& copy_values (vec<lbool>&  destination, const vec<lbool> &source, const VarVector& variables) {
  FOR_EACH(vi,variables) {
    const Var v = *vi;
    if (destination.size()<=v) destination.growTo(v+1, l_Undef);
    destination[v]=source[v];
  }
  return destination;
}


bool closed(const Fla& formula) {
  FOR_EACH(sf, formula.flas) {
    const QFla& subformula = *sf;
    VarSet vs(formula.pref);
    FOR_EACH(q, subformula.pref) vs.add_all(q->second);
    if (!closed(subformula.cnf, vs)) return false;
  }
  return true;
}

void subst(const vector<LitSet>& cnf, const vec<lbool>& move, vector<LitSet>& res) {
  if (empty_substitution(move)) {
    res=cnf;
    return;
  }


#ifdef __SET
  LitSetSet set;
#endif
  vector<Lit> ls;
  FOR_EACH(cli,cnf) {
    LitSet cl = *cli; 
    ls.clear();
    bool taut_cl=false;
    FOR_EACH(li,cl) {
      const Lit& l = *li;
      const Var  v = var(l);
      const bool lsign = sign(l);
      if (v >= move.size() || move[v]==l_Undef) {
        ls.push_back(l);
      } else {
        if (lsign == (move[v]==l_False)) {
          taut_cl=true;
          break;
        }
      }
    }
    if (taut_cl) continue;
    if (ls.size()==cl.size()) {
#ifdef __SET
      const bool c = CONTAINS(set, cl);
#else 
      const bool c = false;
#endif 
      if (!c) {
        res.push_back(cl);
#ifdef __SET
        set.insert(cl);
#endif 
      }
    } else {
      LitSet scl(ls);
#ifdef __SET
      const bool c = CONTAINS(set, scl);
#else 
      const bool c = false;
#endif 
      if (!c) {
        res.push_back(scl);
#ifdef __SET
        set.insert(scl);
#endif 
      }
    }
  }
}


void subst(const vector<LitSet>& cnf, const vec<lbool>& substitution, VarSet& used, vector<LitSet>& res) {
  if (empty_substitution(substitution)) {
    res=cnf;
    return;
  }

  LitSetSet   set;
  vector<Lit> ls;
  bool unsatisfiable = false;
  FOR_EACH(cli,cnf) {
    LitSet cl = *cli; 
    ls.clear();
    bool taut_cl=false;
    FOR_EACH(li,cl) {
      const Lit& l = *li;
      const Var  v = var(l);
      const bool lsign = sign(l);
      if (v >= substitution.size() || substitution[v]==l_Undef) {
        ls.push_back(l);
      } else {
        used.add(v);
        if (lsign == (substitution[v]==l_False)) {
          taut_cl=true;
          break;
        }
      }
    }
    if (taut_cl) continue;
    if (ls.empty()) {//empty clause was hit
      unsatisfiable = true;
      break;
    }
    if (ls.size()==cl.size()) {
      const bool c = CONTAINS(set, cl);
      if (!c) {
        res.push_back(cl);
        set.insert(cl);
      }
    } else {
      LitSet scl(ls);
      const bool c = CONTAINS(set, scl);
      if (!c) {
        res.push_back(scl);
        set.insert(scl);
      }
    }
  }
  if (unsatisfiable) {
    res.clear();
    res.push_back(LitSet());
  }
}


ostream& print(ostream& o, const VarVector& vs, const vec<lbool>& move) {
  FOR_EACH(vi,vs) {
      o << " ";
      const Var var = *vi;
      const lbool v = var < move.size() ? move[var] : l_Undef;
      if (v==l_True) o << "+";
      else if (v==l_False) o << "-";
      else o << "?";
      o << var;
  }
  return o;
}

bool unique_(VariableVector& variables) {
  std::sort(variables.begin(),variables.end());
  for (size_t index = 1; index<variables.size(); ++index) 
    if (variables[index]==variables[index-1]) return false;
  return true;
}

bool unique(const VarVector& _variables) {
  VariableVector variables; 
  FOR_EACH (vi, _variables) variables.push_back(*vi);
  return unique_(variables);
}

void traverse(const Fla& f, vector<bool>& top_lits, vector< vector<bool> >&  inner){
  const VarSet top(f.pref);
  inner.resize(f.flas.size());
  RANGE(index, 0, f.flas.size()) {
    FOR_EACH (clause_index, f.flas[index].cnf) {
      FOR_EACH (lit, *clause_index) {
        const Lit     l = *lit;
        const size_t li = literal_index(l);
        const Var     v = var(l);
        vector<bool>& o = top.get(v) ? top_lits : inner[index];
        if (o.size() <= li) o.resize(li+1, false);
        o[li] = true;
      }
    }
  }
}

void traverse_stats(const VarVector& pref, const vector<bool>& occurrences, size_t& removed, size_t& pure) {
  FOR_EACH(vi, pref) {
    const Var       v = *vi;
    const bool has_pl =  has_literal(mkLit(v), occurrences);
    const bool has_nl =  has_literal(~mkLit(v), occurrences);
    if (!has_nl && !has_pl) ++removed;
    else if (!has_nl || !has_pl) ++pure;
  }
}

ostream& traverse_stats(ostream& output, const Fla& f, const vector<bool>& top_lits, const vector< vector<bool> >&  inner) {
  size_t removed = 0;
  size_t pure = 0;
  traverse_stats(f.pref, top_lits, removed, pure);
  output << "r: " << removed << " p: " << pure;
  RANGE(index, 0, f.flas.size()) {
    removed = 0;
    pure    = 0;
    FOR_EACH (qi, f.flas[index].pref) {
      traverse_stats(qi->second, inner[index], removed, pure);
    }
    output << " r: " << removed << " p: " << pure;
  }
  return output << endl;
}



void subst(const vector<LitSet>& cnf, const vec<SUBSTITUTION_VALUE>& substitution, vector<LitSet>& res) {
  bool ou = true;
  for (int index = 0; index < substitution.size(); ++index) {
    if (substitution[index]!=NOOP) {
      ou=false;
      break;
    }
  }
  if (ou) {
    res=cnf;
    return;
  }

  LitSetSet set;
  vector<Lit> ls;
  bool unsatisfiable = false;
  FOR_EACH(cli,cnf) {
    LitSet cl = *cli; 
    ls.clear();
    bool taut_cl=false;
    FOR_EACH(li,cl) {
      const Lit& l = *li;
      const Var  v = var(l);
      const bool lsign = sign(l);
      if (v >= substitution.size() || substitution[v]==NOOP) {
        ls.push_back(l);
      } else {
        SUBSTITUTION_VALUE value = substitution[v];
        if ((value!=REMOVE) && (lsign==(value==SET_FALSE))) {
          taut_cl=true;
          break;
        }
      }
    }
    if (taut_cl) continue;
    if (ls.size()==cl.size()) {
      const bool c = CONTAINS(set, cl);
      if (!c) {
        res.push_back(cl);
        set.insert(cl);
      }
    } else {
      if (ls.empty()) {
        unsatisfiable = true;
        break;
      }
      LitSet scl(ls);
      const bool c = CONTAINS(set, scl);
      if (!c) {
        res.push_back(scl);
        set.insert(scl);
      }
    }
  }
  if (unsatisfiable) {
    res.clear();
    res.push_back(LitSet());
  }
}
