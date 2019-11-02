/*
 * File:  VarVector.hh
 * Author:  mikolas
 * Created on:  Sun Jan 15 14:53:20 GMTST 2012
 * Copyright (C) 2012, Mikolas Janota
 */
#ifndef VARVECTOR_HH_29407
#define VARVECTOR_HH_29407
#include <vector>
#include <iostream>
#include <iterator>
#include "minisat_auxiliary.hh" 
using std::iterator;
using std::ostream;
using std::vector;
using Minisat::Var;

#define DBG_LITSET(t)

class const_VarVectorIterator;

class VarVector {
public:
  typedef const_VarVectorIterator const_iterator;
  static const VarVector mk(const vector<Var>& vs) { VarVector r(vs); return r; }
  inline VarVector() {
    _variables=NULL;
    _size=0;
    _hash_code = EMPTY_HASH;
  }

  inline VarVector(const VarVector& ls) { 
    _hash_code=ls._hash_code; _size=ls._size; _variables=ls._variables;
    if (_variables!=NULL) ++(_variables[0]);
  }

  VarVector& operator= ( const VarVector& ls ) { 
    decrease();
    _hash_code=ls._hash_code; _size=ls._size; _variables=ls._variables;
    if (_variables!=NULL) ++(_variables[0]);
    return *this;
  }

  VarVector(const vector<Var>& vars);
  virtual ~VarVector();
  bool                  equal(const VarVector& other) const;
  ostream&              print(ostream& out) const;
  inline size_t         hash_code() const;
  inline size_t         size() const;
  inline bool           empty() const;
  inline const_iterator begin() const;
  inline const_iterator end()   const;
  inline const Var      operator [] (size_t index) const;
private:
  static const size_t          EMPTY_HASH=1313;
  size_t                       _hash_code;
  size_t                       _size;
  Var*                         _variables;   // first literal as reference counter
  inline int decrease(); // decrease reference counter, deallocat if possible
};



ostream & operator << (ostream& outs, const VarVector& ls);


class const_VarVectorIterator : public iterator<std::forward_iterator_tag, Var>
{
public:
  const_VarVectorIterator(const VarVector& ls, size_t x) : ls(ls), i(x) {}
  const_VarVectorIterator(const const_VarVectorIterator& mit) : ls(mit.ls), i(mit.i) {}
  const_VarVectorIterator& operator++() {++i; return *this;}
  bool operator==(const const_VarVectorIterator& rhs) { assert(&ls==&(rhs.ls)); return i==rhs.i;}
  bool operator!=(const const_VarVectorIterator& rhs) { assert(&ls==&(rhs.ls)); return i!=rhs.i;}
  const Var  operator*() const {return ls[i];}
private:
  const VarVector&               ls;
  size_t                          i;
};

class VarVector_equal {
public:
  inline bool operator () (const VarVector& ls1,const VarVector& ls2) const { return ls1.equal(ls2); }
};

class VarVector_hash {
public:
  inline size_t operator () (const VarVector& ls) const { return ls.hash_code(); }
};

inline int VarVector::decrease() {
  if (_variables==NULL) return 0;
  assert (_variables[0]);
  DBG_LITSET( const int ov= _variables[0]; )
  const int nv = --(_variables[0]);
  DBG_LITSET( cerr << "dec " << ov << " to " << nv << endl; )
  if ((_variables[0])==0) {
    DBG_LITSET( cerr << "mem rel" << endl; )
    delete[] _variables;
  }
  _variables=NULL;
  return nv;
}


inline size_t VarVector::hash_code() const { return _hash_code;}
inline size_t VarVector::size() const          { return _size; }
inline bool VarVector::empty() const          { return _size==0; }
inline const Var VarVector::operator [] (size_t index) const {
  assert(index < _size);
  return _variables[index+1]; }

inline VarVector::const_iterator VarVector::begin() const { return const_VarVectorIterator(*this, 0); }
inline VarVector::const_iterator VarVector::end()   const { return const_VarVectorIterator(*this, _size); }
#endif /* VARVECTOR_HH_29407 */
