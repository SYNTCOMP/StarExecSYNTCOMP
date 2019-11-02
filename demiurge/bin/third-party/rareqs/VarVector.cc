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
 * File:  VarVector.cc
 * Author:  mikolas
 * Created on:  Sun Jan 15 14:53:28 GMTST 2012
 * Copyright (C) 2012, Mikolas Janota
 */
#include "VarVector.hh"

VarVector::VarVector(const vector<Var>& vars) {
  _size = vars.size();
  if (_size==0) {
    _variables=NULL;
    _hash_code = EMPTY_HASH;
    return;
  }
  _variables = new Var[_size+1];
  _variables[0]=1;
  for (size_t i=0; i<_size; ++i) _variables[i+1]=vars[i];
  _hash_code = 7;
  for (size_t i=1; i <= _size; ++i) _hash_code = _hash_code*31 + _variables[i];
}


bool VarVector::equal(const VarVector& other) const {
  if (other._size!=_size) { return false; }
  if (other._variables==_variables) return true;
  for (size_t i=1; i <= _size; ++i) if (_variables[i]!=other._variables[i]) return false;
  return true;
}

VarVector::~VarVector() {
  decrease();
}

ostream& VarVector::print(ostream& out) const {
  FOR_EACH(i,*this)  out << *i << " ";
  return out;
}

ostream & operator << (ostream& outs, const VarVector& vs) { return vs.print(outs); }
