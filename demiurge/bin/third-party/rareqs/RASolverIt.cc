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
 * File:  RASolverIt.cc
 * Author:  mikolas
 * Created on:  Thu Dec 15 08:36:58 GMTST 2011
 * Copyright (C) 2011, Mikolas Janota
 */
#include "RASolverIt.hh"
#include "RASolverLeaf.hh"
#include "RASolverNonLeaf.hh"

RASolver* mk(Var max_id, const Alts& alternations, bool unit_val , size_t indent ) 
{
  assert (alternations.size());
  if (alternations.size()==1 ) return new RASolverLeaf(alternations, indent) ;
  else return new RASolverNonLeaf(alternations, unit_val, indent);
}

RASolverIt::RASolverIt(Var max_id, const Prefix& pref, const CNF& cnf, bool unit_val) 
  : solver( mk(max_id, toAlts(pref), unit_val, 0) )
{
  while (max_id > solver->new_var()) ;
  QFla    stripped_formula;
  Prefix& stripped_prefix = stripped_formula.pref;
  solver->add_variables(pref[0].second, false);
  for (size_t i=1; i<pref.size(); ++i) stripped_prefix.push_back(pref[i]);
  stripped_formula.cnf = cnf;
  solver->add_formula(stripped_formula);
}

RASolverIt::RASolverIt(Var max_id, const VarVector& prefix, const Alts& alternations, const vector<QFla>&  flas, bool unit_val, size_t indent ) 
  : solver( mk(max_id, alternations, unit_val,  indent) )
{
  while (max_id > solver->new_var()) ;
  solver->add_variables(prefix, false);
  FOR_EACH(i, flas) solver->add_formula(*i);
}


RASolverIt::~RASolverIt() { 
  if (solver) {
    delete solver;
  }
}

bool RASolverIt::solve(const vec<Lit>& a) { return solver->solve(a); }
bool RASolverIt::solve() { 
  const vec<Lit> e;
  return solve(e); 
}

const vec<lbool>& RASolverIt::move() const { return solver->move(); }  
