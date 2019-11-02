/*
 * File:  RASolver.hh
 * Author:  mikolas
 * Created on:  Thu Nov 10 20:40:54 EST 2011
 * Copyright (C) 2011, Mikolas Janota
 */
#ifndef RASOLVER_HH_19891
#define RASOLVER_HH_19891
#include "qtypes.hh"
#include "minisat_auxiliary.hh"
#include "LitSet.hh"
#include "VarSet.hh"
#include "utils.hh"
class RASolver : public VarManager {
public:
  virtual bool solve(const vec<Lit>& a) = 0;
  virtual const vec<lbool>&  move() const = 0;
  virtual bool add_formula(const QFla& formula) = 0;
  virtual void add_variables(const VarVector& new_top_variables, bool free) = 0;
  virtual const VarSet& free_variables() = 0;
  virtual ~RASolver() {}
  // virtual Var  new_var() = 0; // inherited from VarManager
};
#endif /* RASOLVER_HH_19891 */
