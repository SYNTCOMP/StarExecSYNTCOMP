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
 * File:  RASolverNonLeaf.cc
 * Author:  mikolas
 * Created on:  Wed Dec 14 14:35:23 GMTST 2011
 * Copyright (C) 2011, Mikolas Janota
 */
#include <algorithm>
#include "RASolverNonLeaf.hh"
#include "RASolverLeaf.hh"
#include "Unit.hh"

RASolverNonLeaf::RASolverNonLeaf(const Alts& pre, bool unit_val, size_t _indent) 
  : global_prefix(pre)
  , Q(pre[0])
  , unit(unit_val)
  , max_id(0)
  , indent(_indent)
  , iteration_counter(0)
{
  assert(global_prefix.size() >= 2);
  if (verbose) {
    out() << "Nonleaf incr solver at level: " << (indent/INDENTATION) << endl;
    out() << "unit: " << unit <<endl;
    if (verbose>1) out() << global_prefix << endl;
  } 

  Alts stripped_prefix;
  stripped_prefix.push_back(global_prefix[0]);
  RANGE(i,3,global_prefix.size()) stripped_prefix.push_back(global_prefix[i]);
  abstraction_solver = stripped_prefix.size() == 1 ? (RASolver*) new RASolverLeaf(stripped_prefix, indent+INDENTATION) 
                                                  : (RASolver*) new RASolverNonLeaf(stripped_prefix, indent+INDENTATION, unit);
}

RASolverNonLeaf::~RASolverNonLeaf() {
  if (abstraction_solver!=NULL) delete abstraction_solver;
  FOR_EACH(solver_index, opponent_solvers) delete *solver_index;
  opponent_solvers.clear();
  if (verbose>4) out() << "Nonleaf destr at level: " << (indent/INDENTATION) << endl;
}

const VarSet& RASolverNonLeaf::free_variables() { return _free_variables; } 

void RASolverNonLeaf::add_variables(const VarVector& new_top_variables, bool free) {
  abstraction_solver->add_variables(new_top_variables,free);
  if (free) { 
    FOR_EACH(vi,new_top_variables) _free_variables.add(*vi);
  } else { 
    prefix = append(prefix, new_top_variables);
  }
  assert(check_header());
}

bool RASolverNonLeaf::add_formula(const QFla& formula) {
  if (verbose > 3) out() << "add_formula " << formula << endl;
  if(formula.pref.empty()){
     return abstraction_solver->add_formula(formula);
  }

  assert (formula.pref[0].first != Q);
  RASolver*  formula_solver = NULL;
  Alts fa(toAlts(formula.pref));
  if (formula.pref.size()==1) formula_solver = new RASolverLeaf(fa, indent+INDENTATION);
  else  {
    formula_solver = new RASolverNonLeaf(fa, unit, indent+INDENTATION);
  }

  while (max_id > formula_solver->new_var()) ;
  QFla    stripped_formula;
  Prefix& stripped_prefix = stripped_formula.pref;
  formula_solver->add_variables(prefix, true);
  VariableVector _free_variables_c; //TODO nicer
  for (Var v = 1; v < (Var)free_variables().physical_size(); ++v)  {
    if (free_variables().get(v)) {
      _free_variables_c.push_back(v);
    }
  }

  formula_solver->add_variables(_free_variables_c, true);
  formula_solver->add_variables(formula.pref[0].second, false);
  for (size_t i=1; i<formula.pref.size(); ++i) stripped_prefix.push_back(formula.pref[i]);
  stripped_formula.cnf = formula.cnf;
  formula_solver->add_formula(stripped_formula);
  opponent_solvers.push_back(formula_solver);
  subformulas.push_back(formula);
  assert(check_header());
  return true; // TODO: anything else?
}

Var RASolverNonLeaf::new_var()  { 
  max_id = abstraction_solver->new_var(); 
  return max_id;
}

const vec<lbool>& RASolverNonLeaf::move() const { return _move; }

bool RASolverNonLeaf::solve(const vec<Lit>& a) {
  if (verbose > 1) {
    print(out() << "solve(", a) << ")" << endl;
    if (verbose > 3) {
      out() << Q << prefix << endl;
      FOR_EACH(fi,subformulas)  out() << "  " << *fi << endl;
    }
  }
  const double t0 = read_cpu_time();
  const bool    r = _solve(a);
  const double  d = read_cpu_time()-t0;
  if (verbose) {
    out() << "RES: " << r << endl;
    out() << "t: " << d << endl;
    out() << "#iterations: " << iteration_counter << endl;
    if (verbose > 1) { if(r) { print(out() << "Move: ", prefix, move()) << endl; }}
  }
  return r;
}

bool RASolverNonLeaf::_solve(const vec<Lit>& a) {
  iteration_counter = 0;
  while (true) {
    // = play abstract game 
    const bool c = abstraction_solver->solve(a);
    if (!c) return false;
    // = let opponents play
    // - create move for the opponent
    if (verbose > 3) print(out() << "can: ", prefix, abstraction_solver->move()) << endl;
    // - iterate through opponents
    bool opponent_win=false;
    vec<lbool> opponent_move;
    size_t opponent_index;
    for (opponent_index=0; opponent_index<opponent_solvers.size(); ++opponent_index) {
      RASolver& opponent_solver = *(opponent_solvers[opponent_index]);
      const auto& crude_move = abstraction_solver->move();
      vec<Lit> abs_move;
      filter(abs_move, a, opponent_solver.free_variables());
      FOR_EACH (variable_index, prefix) {
        const Var v = *variable_index;
        if (!opponent_solver.free_variables().get(v)) continue;
        abs_move.push(crude_move[v]==l_True ? mkLit(v) : ~mkLit(v));
      }

      if (opponent_solver.solve(abs_move)) {
        //copy_values (opponent_move, opponent_solver.move(), *(global_prefix[1].second));
        opponent_solver.move().copyTo(opponent_move);
        opponent_win = true; 
        break;
      }
    }

    // = game analysis
    if (!opponent_win) { // none of the opponents has won, therefore I did
      _move.clear();
      copy_values(_move, abstraction_solver->move(), prefix);
      return true;
    }
    // - refine
    if (verbose > 3) out() << "cex: " << opponent_move << endl;
    if (unit) refine_unit(opponent_move, opponent_index);
    else refine(opponent_move, opponent_index);
    ++iteration_counter;
  }
}

void RASolverNonLeaf::refine(const vec<lbool>& move, size_t opponent_index) {
  const QFla& winning_fla = subformulas[opponent_index];
  // alloc new vars for first block in winning_fla in abstr
  vec<Var> oldToPrime;
  if (winning_fla.pref.size()>1) {
    VariableVector new_variables;
    FOR_EACH (vari, winning_fla.pref[1].second) {
      const Var var = *vari;
      assert (var>0);
      if (oldToPrime.size()<=var) oldToPrime.growTo(var+1,0);
      const Var pv = abstraction_solver->new_var();
      oldToPrime[var] = pv;
      new_variables.push_back(pv);
    }
    abstraction_solver->add_variables(VarVector(new_variables),false);
  }


  QFla disjunct;
  RANGE(i,2,winning_fla.pref.size()) disjunct.pref.push_back(winning_fla.pref[i]);

  vector<Lit> ls;
  FOR_EACH(cli,winning_fla.cnf) {
    LitSet cl = *cli; 
    ls.clear();
    bool taut_cl = false;
    bool change  = false;
    FOR_EACH(li,cl) {
      const Lit& l = *li;
      const Var  v = var(l);
      const bool lsign = sign(l);
      if (v >= move.size() || move[v]==l_Undef) {
        if (v >= oldToPrime.size() || oldToPrime[v]==0) ls.push_back(l);  // no change
        else {
          ls.push_back(mkLit(oldToPrime[v],lsign));                       // primed variable
          change=true;
        }
      } else { 
        // substituted variable
        if (lsign == (move[v]==l_False)) {
          taut_cl=true;
          break;
        }
        change=true; // omit lit
      }
    }
    if (taut_cl) continue;
    if (change) disjunct.cnf.push_back(LitSet(ls));
    else disjunct.cnf.push_back(cl);
  }
  if (verbose > 3) out() << "ref: " << disjunct << endl;
  abstraction_solver->add_formula(disjunct);
}

void RASolverNonLeaf::refine_unit(const vec<lbool>& move, size_t opponent_index) {
  const QFla& winning_fla = subformulas[opponent_index];
  QFla disjunct;
  CNF  temp;
  CNF& cnf = (winning_fla.pref.size()>1) ? temp : disjunct.cnf; // use temporary when freshening is needed at the end

  // perform unit propagation with the given move
  if (verbose>4) {
    out() << "unit propagation on: " << winning_fla.cnf << endl;
    out() << move << endl;
  }

  Unit u(winning_fla.cnf,move);
  bool ok=true;
  ok &= u.propagate();
  if (!ok) {
    disjunct.cnf.push_back(LitSet());
    goto FINISH;
  }
  u.eval(cnf);
  if (verbose>4) {
     out() << "unit propagation result: " << cnf << endl;
     ostream& o = out();
     for (Var v=1; v < u.size(); ++v) if (u.value(v)!=l_Undef) {
         o  << mkLit(v,  u.value(v)==l_False) << " ";
     }
     o << endl;
  }

  // add unit clauses
  fix_unit(u, prefix, cnf);
  fix_unit(u, _free_variables, cnf);

  for (size_t i=1; i < winning_fla.pref.size(); i++) {
    if (winning_fla.pref[i].first==UNIVERSAL) {
      if (any_fixed(u, winning_fla.pref[i].second)) {
        cnf.clear(); 
        fix_unit(u, winning_fla.pref[i].second, cnf);
        break;
      }} else {
      fix_unit(u, winning_fla.pref[i].second, cnf);
    }
  }

  
  if (winning_fla.pref.size()>1) {// freshen vars for first block in winning_fla
    VarSet      q1(winning_fla.pref[1].second);
    vec<Var>    oldToPrime;
    freshen(cnf, q1, disjunct.cnf, oldToPrime, *abstraction_solver); 
    // insert fresh variables into the prefix of the abstraction
    VariableVector new_variables;
    for (int i=0; i<oldToPrime.size(); ++i) { 
      const Var v = oldToPrime[i];
      if (v!=0) new_variables.push_back(v);
    }
    abstraction_solver->add_variables(VarVector(new_variables),false);
  }

 FINISH:

  if (!ok) {
    if (!ok && verbose>3) out() << "refinement false by unit propagation"  << endl;
  }

  for (size_t i=2; i < winning_fla.pref.size(); i++) {
    disjunct.pref.push_back(winning_fla.pref[i]);
  }

  if (verbose>3) {
    out() << "Refined with:"  << endl;
    out() << disjunct  << endl;
  }
  abstraction_solver->add_formula(disjunct);
}



bool RASolverNonLeaf::check_header() const {
  if (!unique(prefix)) return false;
  FOR_EACH (formula_index, subformulas) { 
    VariableVector variables;
    append(variables, prefix);
    FOR_EACH(quantification, formula_index->pref) append(variables, quantification->second);
    if (!unique_(variables)) return false;
  }
  return true;
}
