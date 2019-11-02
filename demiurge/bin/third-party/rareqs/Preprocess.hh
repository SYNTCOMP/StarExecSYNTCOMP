/*
 * File:  Preprocess.hh
 * Author:  mikolas
 * Created on:  Sun Jan 15 12:57:09 GMTST 2012
 * Copyright (C) 2012, Mikolas Janota
 */
#ifndef PREPROCESS_HH_8276
#define PREPROCESS_HH_8276
#include "qtypes.hh"
#include "utils.hh"
#include "Univ.hh"
class Preprocess {
public:
  Preprocess(const Fla& formula, VarManager& variable_manager, size_t _indent);
  void              reconstruct(const vec<lbool>& preprocessed_move, vec<lbool>& original_move); 
  const Fla&        preprocess();
  const Fla&        get_original_formula() const { return original_formula; }
private:
  Univ              ur;
  const Fla&        original_formula;
  VarManager&       variable_manager;
  Fla               preprocessed_formula;
  vec<lbool>        reconstruction_values;
  lbool preprocess_subformula(const QFla& original_subformula, const vector<bool>& inner, 
                                        Fla& preprocessed_formula,
                                        QFla& preprocessed_subformula);

private:
  size_t    indent;
  ostream& out() { size_t i=indent; while (i) { cerr<<" "; --i; } return cerr; }
};
#endif /* PREPROCESS_HH_8276 */
