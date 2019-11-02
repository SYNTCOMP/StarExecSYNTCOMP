/*
 * File:  RASolverNonLeaf.hh
 * Author:  mikolas
 * Created on:  Tue Dec 13 18:39:51 WET 2011
 * Copyright (C) 2011, Mikolas Janota
 */
#ifndef RASOLVERNONLEAF_HH_2136
#define RASOLVERNONLEAF_HH_2136
#include "RASolver.hh"
#include "ObjectCounter.hh"
#include "utils.hh"
class RASolverNonLeaf : public RASolver, private ObjectCounter {
public:
  RASolverNonLeaf(const Alts& global_prefix, bool unit_val, size_t _indent);
  virtual ~RASolverNonLeaf();
  virtual bool              solve(const vec<Lit>& a);
  virtual const vec<lbool>& move() const;
  virtual bool              add_formula(const QFla& formula);
  virtual void              add_variables(const VarVector& new_top_variables, bool free); 
  virtual Var               new_var();
  virtual const VarSet&     free_variables();
  bool                      set_unit(bool unit_val=true) { return unit=unit_val; }
protected:
  const Alts             global_prefix;
  const QuantifierType   Q;  
  bool                   unit;
  VarVector              prefix;
  VarSet                 _free_variables;
  Var                    max_id;
  vec<lbool>             _move;
  virtual bool           _solve(const vec<Lit>& a);
  void                   refine(const vec<lbool>& move, size_t opponent_index);
  void                   refine_unit(const vec<lbool>& move, size_t opponent_index);
public:
  const static size_t    INDENTATION = 1;
protected:
  size_t                      indent;
  size_t                      iteration_counter;
  RASolver*                   abstraction_solver;
  vector< RASolver* >         opponent_solvers;
  vector< QFla      >         subformulas; 
  inline ostream&             out() { return make_indent(cerr,indent); }
  bool                        check_header() const;
};
#endif /* RASOLVERNONLEAF_HH_2136 */
