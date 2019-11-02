/*
 * File:  Univ.hh
 * Author:  mikolas
 * Created on:  Wed Jan 25 15:45:27 GMTST 2012
 * Copyright (C) 2012, Mikolas Janota
 */
#ifndef UNIV_HH_14491
#define UNIV_HH_14491
#include "qtypes.hh"
class Univ {
public:
  Univ(const Fla& formula);
  const Fla& preprocess();
  const Fla& preprocessed() {return _preprocessed;}
private:
  const Fla& formula;
  Fla        _preprocessed;
  void       preprocess(Fla& output);
};
#endif /* UNIV_HH_14491 */
