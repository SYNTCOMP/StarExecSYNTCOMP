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
 * File:  Tests.cc
 * Author:  mikolas
 * Created on:  Fri Dec 30 00:33:16 CEST 2011
 * Copyright (C) 2011, Mikolas Janota
 */
#include <cstdarg>
#include "Tests.hh"
#include "Unit.hh"
#include "Univ.hh"
#include "utils.hh"
using Minisat::mkLit;
using std::cout; 
using std::endl;
using std::cerr;
using std::vector;
using std::unordered_set;
using std::unordered_map;
#define TEST(r) do { std::cerr << __FILE__<<":"  << __LINE__ <<  endl; test((r), __FILE__,__LINE__); } while (0)


void create_vars(vector<Var>& vs, size_t nCount, ...) {
  va_list list;
  va_start(list, nCount);
  for (size_t nArg=0; nArg < nCount; nArg++) vs.push_back(va_arg(list, int));
  va_end(list);
}


VarVector create_vars(size_t nCount, ...) {
  vector<Var> vs;
  va_list list;
  va_start(list, nCount);
  for (size_t nArg=0; nArg < nCount; nArg++) vs.push_back(va_arg(list, int));
  va_end(list);
  return VarVector(vs);
}


LitSet create_clause(size_t nCount, ...) {
  va_list list;
  va_start(list, nCount);
  LiteralVector ls;
  for (size_t nArg=0; nArg < nCount; nArg++) ls.push_back(va_arg(list, Lit));
  va_end(list);
  return LitSet(ls);
}


bool Tests::test_all() {
  test_1();
  test_2();
  test_3();
  test_4();
  test_5();
  test_6();

  test_traverse_1 ();

  test_VarSet_1();

  test_Univ_1();
  test_Univ_2();
  test_Univ_3();
  return ok;
}

void Tests::test_1() {
  vector<LitSet> cls;
  cls.push_back(create_clause(1, mkLit(1)));
  cls.push_back(create_clause(2, ~mkLit(1), mkLit(2)));
  cls.push_back(create_clause(1, ~mkLit(2)));


  Unit u(cls);
  TEST(!u.propagate());
}

void Tests::test_2() {
  LiteralVector ls;
  ls.push_back(mkLit(1));
  LitSet s1(ls);

  ls.clear();
  ls.push_back(~mkLit(1));
  ls.push_back(mkLit(2));
  LitSet s2(ls);

  ls.clear();
  ls.push_back(~mkLit(2));
  ls.push_back(mkLit(3));
  LitSet s3(ls);

  vector<LitSet> cls;
  cls.push_back(s1);
  cls.push_back(s2);
  cls.push_back(s3);


  Unit u(cls);
  TEST(u.propagate());
  TEST(u.value(3)==l_True);
}

void Tests::test_3() {
  vector<LitSet> cls;
  cls.push_back(create_clause(1, mkLit(1)));
  cls.push_back(create_clause(1, mkLit(2)));
  cls.push_back(create_clause(3, ~mkLit(1), ~mkLit(2), mkLit(3)));
  Unit u(cls);
  TEST(u.propagate());
  TEST(u.value(3)==l_True);
}

void Tests::test_4() {
  vector<LitSet> cls;
  cls.push_back(create_clause(1, ~mkLit(1)));
  cls.push_back(create_clause(1, ~mkLit(2)));
  cls.push_back(create_clause(1, mkLit(3)));
  cls.push_back(create_clause(3, mkLit(1), mkLit(2), mkLit(3)));
  Unit u(cls);
  TEST(u.propagate());
  TEST(u.value(3)==l_True);
}
 
void Tests::test_5() {
  vector<LitSet> cls;
  cls.push_back(create_clause(1,  mkLit(1)));
  cls.push_back(create_clause(3,  mkLit(1), mkLit(5), mkLit(3)));
  cls.push_back(create_clause(3, ~mkLit(1), mkLit(6), mkLit(4)));

  Unit u(cls);
  TEST(u.propagate());
  TEST(u.value(1)==l_True);
  for (Var v = 2; v <= 6; ++v) TEST(u.value(v)==l_Undef);
  CNF r;
  u.eval(r);
  TEST(r.size()==1);
  if (r.size()==1) TEST(r[0].equal(create_clause(2, mkLit(6), mkLit(4))));
}

void Tests::test_6() {
  vector<LitSet> cls;
  cls.push_back(create_clause(1,  mkLit(1)));
  cls.push_back(create_clause(3,  mkLit(1), mkLit(5), mkLit(3)));
  cls.push_back(create_clause(3, ~mkLit(1), mkLit(6), mkLit(4)));
  cls.push_back(create_clause(3,  mkLit(5), mkLit(6), mkLit(4)));

  Unit u(cls);
  TEST(u.propagate());
  TEST(u.value(1)==l_True);
  for (Var v = 2; v <= 6; ++v) TEST(u.value(v)==l_Undef);
  CNF r;
  u.eval(r);
  TEST(r.size()==2);
  TEST(r[0].equal(create_clause(2, mkLit(6), mkLit(4))));
  TEST(r[1].equal(create_clause(3, mkLit(5), mkLit(6), mkLit(4))));
}
 
void Tests::test_traverse_1() {
  Fla formula;
  formula.pref=create_vars(2, 1, 2);
  formula.flas.resize(2);

  {
  QFla& inner_formula0=formula.flas[0];
  inner_formula0.pref.push_back(Quantification(UNIVERSAL, create_vars(2 , 3, 4)));
  inner_formula0.cnf.push_back(create_clause(2, mkLit(1), ~mkLit(3)));
  }

  {
  QFla& inner_formula1=formula.flas[1];
  inner_formula1.pref.push_back(Quantification(UNIVERSAL, create_vars(2 , 3, 4)));
  inner_formula1.cnf.push_back(create_clause(2, ~mkLit(2), mkLit(3)));
  }

  vector <bool> top;
  vector < vector <bool> > inner;
  traverse (formula, top, inner);
  TEST (has_literal(mkLit(1), top));
  TEST (!has_literal(~mkLit(1), top));
  TEST (has_literal(~mkLit(2), top));
  TEST (!has_literal(mkLit(2), top));

  TEST (has_literal(~mkLit(3), inner[0]));
  TEST (!has_literal(mkLit(3), inner[0]));
  TEST (has_literal(mkLit(3), inner[1]));
  TEST (!has_literal(~mkLit(3), inner[1]));
}


void Tests::test_VarSet_1() {
  VarSet vs;
  vs.add(1);
  vs.add(2);
  vs.add(5);

  TEST(vs.get(5));
  TEST(!vs.get(3));

  vector<bool> ts;
  FOR_EACH(vi,vs) {
    const Var v=*vi;
    const size_t vc=(size_t)v;
    if (vc>=ts.size()) ts.resize(vc+1, false);
    ts[vc]=true;
  }

  RANGE (index, 0, ts.size()) {
    const Var v=(Var)index;
    TEST( (v==1 || v==2 || v==5) == ts[index]);
  }
}


void Tests::test_Univ_1() {
  Fla f;
  f.pref=create_vars(1, 1);
  f.q = UNIVERSAL;
  f.op = OR;
  f.flas.resize(1);
  f.flas[0].pref.push_back(Quantification(EXISTENTIAL, create_vars(1,3)));
  f.flas[0].pref.push_back(Quantification(UNIVERSAL, create_vars(1,2)));
  f.flas[0].pref.push_back(Quantification(EXISTENTIAL, create_vars(1,4)));
  f.flas[0].cnf.push_back(create_clause(2, mkLit(3), mkLit(2)));
  Univ u(f);
  const Fla& pf=u.preprocess();
  TEST(pf.flas.size()==1);
  TEST(pf.flas[0].cnf.size()==1);
  TEST(pf.flas[0].cnf[0].equal(create_clause(1, mkLit(3))));
}


void Tests::test_Univ_2() {
  Fla f;
  f.pref=create_vars(1, 1);
  f.q = UNIVERSAL;
  f.op = OR;
  f.flas.resize(1);
  f.flas[0].pref.push_back(Quantification(EXISTENTIAL, create_vars(1,3)));
  f.flas[0].pref.push_back(Quantification(UNIVERSAL, create_vars(1,2)));
  f.flas[0].pref.push_back(Quantification(EXISTENTIAL, create_vars(1,4)));
  f.flas[0].cnf.push_back(create_clause(2, mkLit(3), mkLit(2)));
  f.flas[0].cnf.push_back(create_clause(3, mkLit(3), mkLit(2), mkLit(4)));
  Univ u(f);
  const Fla& pf=u.preprocess();
  TEST(pf.flas.size()==1);
  TEST(pf.flas[0].cnf.size()==2);
  TEST(pf.flas[0].cnf[0].equal(create_clause(1, mkLit(3))));
  TEST(pf.flas[0].cnf[1].equal(create_clause(3, mkLit(2),mkLit(3),mkLit(4))));
}

void Tests::test_Univ_3() {
  Fla f;
  f.pref=create_vars(1, 1);
  f.q = UNIVERSAL;
  f.op = OR;
  f.flas.resize(4);
  f.flas[0].pref.push_back(Quantification(EXISTENTIAL, create_vars(1,3)));
  f.flas[0].pref.push_back(Quantification(UNIVERSAL, create_vars(1,2)));
  f.flas[0].pref.push_back(Quantification(EXISTENTIAL, create_vars(1,4)));
  f.flas[0].cnf.push_back(create_clause(2, mkLit(3), mkLit(2)));

  f.flas[1].pref.push_back(Quantification(EXISTENTIAL, create_vars(1,3)));
  f.flas[1].pref.push_back(Quantification(UNIVERSAL, create_vars(1,2)));
  f.flas[1].pref.push_back(Quantification(EXISTENTIAL, create_vars(1,4)));
  f.flas[1].cnf.push_back(create_clause(3, mkLit(3), mkLit(2), mkLit(4)));

  f.flas[2].pref.push_back(Quantification(EXISTENTIAL, create_vars(1,3)));
  f.flas[2].pref.push_back(Quantification(UNIVERSAL, create_vars(1,2)));
  f.flas[2].pref.push_back(Quantification(EXISTENTIAL, create_vars(1,4)));
  f.flas[2].cnf.push_back(create_clause(1, mkLit(1)));

  f.flas[3].pref.push_back(Quantification(EXISTENTIAL, create_vars(1,3)));
  f.flas[3].pref.push_back(Quantification(UNIVERSAL, create_vars(2,2,4)));
  f.flas[3].pref.push_back(Quantification(EXISTENTIAL, create_vars(1,5)));
  f.flas[3].cnf.push_back(create_clause(2, ~mkLit(2),mkLit(4)));

  Univ u(f);
  const Fla& pf=u.preprocess();
  TEST(pf.flas.size()==4);
  TEST(pf.flas[0].cnf.size()==1);
  TEST(pf.flas[1].cnf.size()==1);
  TEST(pf.flas[0].cnf[0].equal(create_clause(1, mkLit(3))));
  TEST(pf.flas[1].cnf[0].equal(create_clause(3, mkLit(2),mkLit(3),mkLit(4))));
  TEST(pf.flas[2].cnf[0].equal(create_clause(1, mkLit(1))));
  TEST(pf.flas[3].cnf[0].empty());
}
