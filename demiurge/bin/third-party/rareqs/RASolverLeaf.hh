/*
 * File:  RASolverLeaf.hh
 * Author:  mikolas
 * Created on:  Tue Dec 13 18:39:57 WET 2011
 * Copyright (C) 2011, Mikolas Janota
 */
#ifndef RASOLVERLEAF_HH_4803
#define RASOLVERLEAF_HH_4803
#include "RASolver.hh"
#include "minisat_auxiliary.hh"
#include "qtypes.hh"
#include "utils.hh"
#include "ObjectCounter.hh"

class RASolverLeaf : public RASolver
//, private ObjectCounter 
{
public:
  RASolverLeaf(const Alts& pref, size_t _indent=0);
  ~RASolverLeaf();
  virtual bool              solve(const vec<Lit>& a);
  virtual const vec<lbool>& move() const;
  virtual bool add_formula(const QFla& formula);
  virtual void add_variables(const VarVector& new_top_variables, bool free); 
  virtual Var  new_var();
  virtual const VarSet& free_variables();
protected:
  const QuantifierType   Q;  
  VarVector              prefix;
  VarSet                 _free_variables;
  vec<lbool>             _move;
protected:
  LitSet2Lit             tseitin;
  MiniSatExt             sat_solver;
  bool                   _solve(const vec<Lit>& assumptions);
protected:
  size_t                 indent;
  inline ostream&        out() { return make_indent(cerr,indent); }
  vector< vec<Lit>* >      __clauses;
};
#endif /* RASOLVERLEAF_HH_4803 */
