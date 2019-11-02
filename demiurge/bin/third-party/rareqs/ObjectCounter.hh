/*
 * File:  ObjectCounter.hh
 * Author:  mikolas
 * Created on:  Wed Jan 11 11:18:09 GMTST 2012
 * Copyright (C) 2012, Mikolas Janota
 */
#ifndef OBJECTCOUNTER_HH_7210
#define OBJECTCOUNTER_HH_7210
#include <cstring>
#include <assert.h>
class ObjectCounter {
protected:
  ObjectCounter()       { ++c; }
public:
  ~ObjectCounter()      { assert(c>0); --c; }
  static size_t count() { return c; }
private:
  static size_t c;
};
#endif /* OBJECTCOUNTER_HH_7210 */
