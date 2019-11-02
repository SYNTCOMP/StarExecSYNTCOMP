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
 * File:  LitSet.cc
 * Author:  mikolas
 * Created on:  Wed Oct 12 15:43:36 CEDT 2011
 * Copyright (C) 2011, Mikolas Janota
 */
#include "LitSet.hh"
#include "mtl/Sort.h"
using Minisat::sort;
using Minisat::LessThan_default;
LitSet::LitSet(const LiteralVector& lits) {
  const size_t vsz = lits.size();
  if (vsz==0) {
    _literals=NULL;
    _size=0;
    _hash_code = EMPTY_HASH;
    return;
  }
  _literals = new Lit[vsz+1];
  _literals[0].x=1;
  for (size_t i=0; i<vsz; ++i) _literals[i+1]=lits[i];
  sort(_literals+1, vsz, LessThan_default<Lit>() );
  _size = vsz;
  size_t j=2;
  Lit last = _literals[1];
  for (size_t i=2; i <= vsz; ++i) {
    if (_literals[i] == last) continue;
    _literals[j]=_literals[i];
    last=_literals[i];
    ++j;
  }
  _size=j-1;
  _hash_code = 7;
  for (size_t i=1; i <= _size; ++i) _hash_code = _hash_code*31 + toInt(_literals[i]);
}


bool LitSet::equal(const LitSet& other) const {
  if (other._size!=_size) { return false; }
  if (other._literals==_literals) return true;
  for (size_t i=1; i <= _size; ++i) if (_literals[i]!=other._literals[i]) return false;
  return true;
}

LitSet::~LitSet() {
  decrease();
}

ostream& LitSet::print(ostream& out) const {
  FOR_EACH(i,*this) {
    if (Minisat::sign(*i)) out << '-';
    out<< Minisat::var(*i);
    out << ' ';
  }
  return out;
}

ostream & operator << (ostream& outs, const LitSet& ls) { return ls.print(outs); }
