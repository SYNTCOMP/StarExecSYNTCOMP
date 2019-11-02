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
 * File:  qtypes.cc
 * Author:  mikolas
 * Created on:  Wed Nov 16 11:44:01 EST 2011
 * Copyright (C) 2011, Mikolas Janota
 */
#include "qtypes.hh"

const Alts mkAlts(QuantifierType q, size_t levels) {
  Alts r(levels);
  RANGE(i, 0, levels) {
    r[i]=q;
    q = (q==EXISTENTIAL) ? UNIVERSAL : EXISTENTIAL;
  }
  return r;
}

const Alts toAlts(const Prefix& pre) {
  Alts r;
  FOR_EACH(i,pre) r.push_back(i->first);
  return r;
}

ostream & operator << (ostream& outs, const Alts& qs) {
  outs << '[' ;
  FOR_EACH(qi,qs) outs << '[' << *qi << ']';
  return outs << "]" ;
}

void build_fla(const Prefix& pref, const CNF& cnf, Fla& fla) {
  assert(pref.size()>0);
  fla.q=pref[0].first;
  fla.pref=pref[0].second;
  fla.op=pref[0].first==EXISTENTIAL ? AND : OR;
  QFla qfla;
  for (size_t i=1; i<pref.size(); ++i) {
    qfla.pref.push_back(pref[i]);
  }
  qfla.cnf=cnf;
  fla.flas.push_back(qfla);
};

ostream & operator << (ostream& outs, const CNF& f) {
  outs << '[' ;
  FOR_EACH(ci,f) outs << '[' << *ci << ']';
  return outs << "]" ;
}

ostream & operator << (ostream& outs, const QFla& f) {
  outs << "[" << f.pref << "]"  << "[" ;
  RANGE(i,0,f.cnf.size()) {
    if (i) outs << ", ";
    outs << f.cnf[i];
  }
  return outs << "]" ;
}

ostream & operator << (ostream& outs, QuantifierType q) {
  switch (q) {
  case UNIVERSAL: return outs << "A"; break;
  case EXISTENTIAL: return outs << "E"; break;
  default:  assert(0); return outs << "?";
  }
}

ostream & operator << (ostream& outs, const Fla& f) {
  outs << "[" << (f.q==EXISTENTIAL ? 'e' : 'a') << " " << f.pref << ']' ;
  outs << '[' ;
  char oc=0;
  switch (f.op) {
  case AND: oc = '&'; break;
  case OR: oc = '|'; break;
    //  default: assert(0);
  }
  for (size_t i=0; i<f.flas.size(); ++i) {
    if (i) outs << " " << oc << " ";
    outs<<f.flas[i] << " " ;
  }
  return outs << "]" ;
}

ostream & operator << (ostream& outs, const Prefix& p) {
  FOR_EACH (qi,p) {
    outs << '[' << (qi->first==EXISTENTIAL ? 'e' : 'a');
    outs << " " << qi->second;
    outs << ']' ;
  }
  return outs;
}

ostream & operator << (ostream& outs, const VariableVector& vs) {
  FOR_EACH (v,vs) outs << " " << *v;
  return outs;
}
