/*
 * File:  Tests.hh
 * Author:  mikolas
 * Created on:  Fri Dec 30 00:33:12 CEST 2011
 * Copyright (C) 2011, Mikolas Janota
 */
#ifndef TESTS_HH_8046
#define TESTS_HH_8046
#include "auxiliary.hh"

class Tests {
public:
  Tests()  : ok(true) {}
  bool test_all();
private:
  bool ok;
  void test(bool r, const char* s, int ln) { if (!r) { ok=false; std::cerr << "FAIL, " << s << ":" << ln << endl; } }
  void test_1();
  void test_2();
  void test_3();
  void test_4();
  void test_5();
  void test_6();

  void test_traverse_1();

  void test_VarSet_1();

  void test_Univ_1();
  void test_Univ_2();
  void test_Univ_3();
};
#endif /* TESTS_HH_8046 */
