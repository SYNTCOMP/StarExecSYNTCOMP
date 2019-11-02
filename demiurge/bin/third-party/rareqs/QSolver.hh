/*
 * File:  QSolver.hh
 * Author:  mikolas
 * Created on:  Thu Dec 15 09:18:06 GMTST 2011
 * Copyright (C) 2011, Mikolas Janota
 */
#ifndef QSOLVER_HH_5755
#define QSOLVER_HH_5755
#include "qtypes.hh"
#include "minisat_auxiliary.hh"
class QSolver {
public:
  virtual bool solve() = 0;
  virtual const vec<lbool>&  move() const = 0;
};
#endif /* QSOLVER_HH_5755 */
