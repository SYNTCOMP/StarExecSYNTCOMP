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
 * File:  RASolverLeaf.cc
 * Author:  mikolas
 * Created on:  Tue Dec 13 20:18:19 WET 2011
 * Copyright (C) 2011, Mikolas Janota
 */
#include "RASolverLeaf.hh"
#include "RASolverNonLeaf.hh"
#include "utils.hh"
RASolverLeaf::RASolverLeaf(const Alts& _pre, size_t _indent)
  : Q(_pre[0])
  , indent(_indent)
{
  assert(_pre.size()==1);
  if (verbose) out() << "Incr Leaf Solver at level: " << (indent/RASolverNonLeaf::INDENTATION) <<endl;
  if (verbose>1) out() << _pre << endl;
  sat_solver.newVar();
}

RASolverLeaf::~RASolverLeaf() {
  if (verbose>4) {
    out() << "Leaf solver destr at level: " << endl;
  } 
}

bool RASolverLeaf::solve(const vec<Lit>& a) {
  if (verbose > 1) {
    print(out() << "solve(", a) << ")" << endl;
    if (verbose > 3) {
      out() << Q << prefix << endl;
      FOR_EACH(ci,__clauses)  out() << "  " << **ci << endl;
    }
  }

  const double t0 = read_cpu_time();
  const bool r = _solve(a);
  const double d = read_cpu_time()-t0;
  if (verbose) {
    out() << "RES: " << r << endl;
    out() << "t: " << d << endl;
    if (verbose > 1) { if(r) { print(out() << "Move: ", prefix, move()) << endl; }}
  }
  return r;
}

bool RASolverLeaf::_solve(const vec<Lit>& assumptions) {
  assert(closed(assumptions, free_variables()));
  if (!sat_solver.okay()) return false;
  const bool r = sat_solver.solve(assumptions);
  if (!r) return false;
  _move.clear();
  const vec<lbool>& model = sat_solver.model;
  FOR_EACH(vi,prefix) {
    const Var v = *vi;
    if (v>=_move.size())  _move.growTo(v+1,l_Undef);
    _move[v]= ((v < model.size()) && (model[v]==l_True)) ? l_True : l_False;
  }
  return true;
}

bool RASolverLeaf::add_formula(const QFla& formula) {
  if (verbose>3) out() << "add_formula " << formula << endl;
  assert(formula.pref.size()==0);
  //assert(closed(formula.cnf, prefix));
  const CNF& cnf = formula.cnf;
  bool ok = true;
  if (Q==EXISTENTIAL) {
    vec<Lit> ls;
    FOR_EACH(cli,cnf) {
      ls.clear();
      FOR_EACH(li,*cli) ls.push(*li);
      ok &= sat_solver.addClause_(ls);
      if (verbose>3) {
        vec<Lit> *pls = new vec<Lit>();
        ls.copyTo(*pls);
        __clauses.push_back(pls);
      }
    }
  } else {
    vec<Lit> ts;
    FOR_EACH(cli,cnf) {
      const LitSet& cl=*cli;
      if (cl.size()==1) {
        ts.push(~cl[0]);
        continue;
      }
      
      const LitSet2Lit::const_iterator tsi=tseitin.find(cl);
      if (tsi!=tseitin.end()) {
        ts.push(tsi->second);
      } else {
        const Lit tsl  = mkLit(sat_solver.newVar());
        FOR_EACH(li,cl) ok &= sat_solver.addClause( ~tsl, ~(*li) );
        tseitin[cl]=tsl;
        ts.push(tsl);
      }
    }
    ok &= sat_solver.addClause(ts);
  }

  return ok;
}

void RASolverLeaf::add_variables(const VarVector& new_top_variables, bool free) {
  if (verbose>3) out() << "add_variables " << new_top_variables << endl;
  sat_solver.new_variables(max(new_top_variables));
  if (free) { 
    FOR_EACH(vi,new_top_variables) _free_variables.add(*vi);
  } else { 
    prefix = append(prefix, new_top_variables);
  }
  assert(unique(prefix));
}

Var RASolverLeaf::new_var() {
  const Var max_id = sat_solver.newVar();
  return  max_id;
}

const vec<lbool>& RASolverLeaf::move() const { return _move; }

const VarSet& RASolverLeaf::free_variables() { return _free_variables; } 
