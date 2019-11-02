/* 
 * File:   minisat_aux.hh
 * Author: mikolas
 *
 * Created on October 21, 2010, 2:10 PM
 */

#ifndef MINISAT_AUX_HH
#define	MINISAT_AUX_HH
#include "auxiliary.hh"
#include "MiniSatExt.hh"
#include <ostream>
#include "mtl/Vec.h"
using std::ostream;
using std::vector;
using Minisat::MiniSatExt;
using Minisat::lbool;
using Minisat::mkLit;
using Minisat::sign;
using Minisat::var;
using Minisat::vec;
using Minisat::Lit;
using Minisat::Var;

typedef std::vector<Lit>                      LiteralVector;
typedef vector< vector<Lit>* >                ClauseVector;

ostream& print_model(ostream& out, const vec<lbool>& lv);
ostream& print_model(ostream& out, const vec<lbool>& lv, int l, int r);
ostream& print(ostream& out, const vec<Lit>& lv);
ostream& print(ostream& out, const vector<Lit>& lv);
ostream& print(ostream& out, Lit l);
ostream& operator << (ostream& outs, Lit lit);
ostream& operator << (ostream& outs, lbool lb);

inline ostream& operator << (ostream& outs, const vec<Lit>& lv) {
  return print(outs, lv);
}

inline void to_lits(const vec<lbool>& bv, vec<Lit>& output, int s, const int e) {
  for (int index = s; index <= e; ++index) {
    if (bv[index]==l_True) output.push(mkLit((Var)index));
    else if (bv[index]==l_False) output.push(~mkLit((Var)index));
  }
}

class Lit_equal {
public:
  inline bool operator () (const Lit& l1,const Lit& l2) const { return l1==l2; }
};

class Lit_hash {
public:
  inline size_t operator () (const Lit& l) const { return Minisat::toInt(l); }
};

inline size_t literal_index(Lit l) { return sign(l) ? (var(l) << 1) : ( (var(l) << 1)+1 ) ; }
#endif	/* MINISAT_AUX_HH */
