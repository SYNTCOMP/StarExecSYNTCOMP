/*
 * File:  RASolverNoIt.hh
 * Author:  mikolas
 * Created on:  Mon Nov 14 16:05:37 EST 2011
 * Copyright (C) 2011, Mikolas Janota
 */
#ifndef RASOLVERNOIT_HH_19158
#define RASOLVERNOIT_HH_19158
#include "qtypes.hh"
#include "minisat_auxiliary.hh"
#include "LitSet.hh"
#include "QSolver.hh"
#include "utils.hh"
#include "Preprocess.hh"

class RASolverNoIt : public QSolver, public VarManager {
public:
  RASolverNoIt(size_t max_id, const Fla& fla, int unit_val, size_t _indent);
  bool                 solve();
  const vec<lbool>&    move() const { return _rmove; }
  bool                 set_hybrid(size_t hybrid_val);
  Var                  new_var();
protected:
  Var                    abs_max_id;
  Preprocess             prep;
  const Fla&             fla;
  const QuantifierType   Q;
  vec<lbool>             _move;
  vec<lbool>             _rmove;
  int                    unit;
  size_t                 hybrid;
  bool      _solve();
  bool      sat();
  bool      refine(const vec<lbool>& move, const QFla& winning_qfla, Fla& abs);
  bool      refine_unit(const vec<lbool>& move, const QFla& winning_fla, Fla& abs);
  void      block(const vec<lbool>& abs_move, const VarSet& used_values, Fla& abs);
  bool      play_opponents(const vec<lbool>& abs_move, vec<lbool>& opponent_move, VarSet& used_values, size_t& winning_opponent);
  void      build_oponnent_game(const QFla& qfla, const vec<lbool>& cand_move, VarSet& used_values, Fla& oponnent_game);
  Var       new_abstraction_var();
protected:
  size_t    indent;
  size_t    iteration_counter;
  ostream& out() { size_t i=indent; while (i) { cerr<<" "; --i; } return cerr; }
  const static size_t INDENTATION = 2;
};
#endif /* RASOLVERNOIT_HH_19158 */
