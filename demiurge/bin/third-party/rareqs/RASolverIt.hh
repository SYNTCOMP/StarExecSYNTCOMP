/*
 * File:  RASolverIt.hh
 * Author:  mikolas
 * Created on:  Thu Dec 15 08:35:49 GMTST 2011
 * Copyright (C) 2011, Mikolas Janota
 */
#ifndef RASOLVERIT_HH_9407
#define RASOLVERIT_HH_9407
#include "RASolver.hh"
#include "QSolver.hh"

class RASolverIt : public QSolver {
public:
  RASolverIt(Var max_id, const Prefix& pref, const CNF& cnf, bool unit_val);
  RASolverIt(Var max_id, const VarVector& prefix, const Alts& alternations, const vector<QFla>&  flas, bool unit_val, size_t indent );
  virtual ~RASolverIt();
  bool solve(const vec<Lit>& a);
  bool solve();
  const vec<lbool>&  move() const;  
private:
  RASolver* const    solver;
};
#endif /* RASOLVERIT_HH_9407 */
