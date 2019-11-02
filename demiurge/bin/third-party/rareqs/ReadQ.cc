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
 * File:   Read2Q.cc
 * Author: mikolas
 *
 * Created on Tue Jan 11 15:08:14
 */
#include "ReadQ.hh"
#include "ReadException.hh"
#include <stdio.h>
#include <algorithm>
#include "parse_utils.hh"
using std::max;
using Minisat::mkLit;

ReadQ::ReadQ(Reader& r, bool _qube_input)
: r(r) 
, qube_input(_qube_input)
, qube_output(-1)
, max_id(0)
, _header_read(false)
{}

ReadQ::~ReadQ() {
}

bool ReadQ::get_header_read() const  {return _header_read;}
Var ReadQ::get_max_id() const {return max_id;}
const vector<Quantification>& ReadQ::get_prefix() const {return quantifications;}
const vector<LitSet>& ReadQ::get_clauses() const          {return clause_vector;}
int ReadQ::get_qube_output() const { assert(qube_input); return qube_output; }

void ReadQ::read_cnf_clause(Reader& in, vector<Lit>& lits) {
  int parsed_lit;
  lits.clear();
  for (;;){
      skipWhitespace(in);
      parsed_lit = parse_lit(in);
      if (parsed_lit == 0) break;
      const Var v = abs(parsed_lit);
      max_id = max(max_id, v);
      if (!quantified_variables.get(v)) unquantified_variables.add(v);
      lits.push_back(parsed_lit>0 ? mkLit(v): ~mkLit(v));
  }
}

void ReadQ::read_quantification(Reader& in, Quantification& quantification)  {
  const char qchar = *in;
  ++in;
  if (qchar == 'a') quantification.first=UNIVERSAL;
  else if (qchar == 'e') quantification.first=EXISTENTIAL;
  else throw ReadException("unexpeceted quantifier");

  VariableVector variables;
  while (true) {
    skipWhitespace(in);
    if (*in==EOF) throw ReadException("quantifier not terminated by 0");
    const Var v = parse_variable(in);
    if (v==0) {
      do { skipLine(in); } while (*in=='c');
      if (*in!=qchar) break;
      ++in;
    } else {
      max_id = max(max_id,v);
      variables.push_back(v);
      quantified_variables.add(v);
    }
  }
  quantification.second=VarVector(variables);
}

Var ReadQ::parse_variable(Reader& in) {
    if (*in < '0' || *in > '9') {
       string s("unexpected char in place of a variable: ");  
       s+=*in;
       throw ReadException(s);
    }
    Var return_value = 0;
    while (*in >= '0' && *in <= '9') {
        return_value = return_value*10 + (*in - '0');
        ++in;
    }
    return return_value;
}

int ReadQ::parse_lit(Reader& in) {
    Var  return_value = 0;
    bool neg = false;
    if (*in=='-') { neg=true; ++in; }
    else if (*in == '+') ++in;
    if ((*in < '0') || (*in > '9')) {
        string s("unexpected char in place of a literal: ");  s+=*in;
        throw ReadException(s);
    }
    while (*in >= '0' && *in <= '9') {
        return_value = return_value*10 + (*in - '0');
        ++in;
    }
    if (neg) return_value=-return_value;
    return return_value;
}


void  ReadQ::read_header() {
    while (*r == 'c') skipLine(r);      
    if (*r == 'p') {
      _header_read = true;
      skipLine(r);
    }
}

void  ReadQ::read_quantifiers() {
  for (;;){
    if (*r == 'c') {
      skipLine(r);
      continue;
    }      
    if (*r != 'e' && *r != 'a') break;
    Quantification quantification;
    read_quantification(r, quantification);
    quantifications.push_back(quantification);
  }
}

void ReadQ::read_clauses() {
  Reader& in=r;  
  vector<Lit> ls;
  for (;;){
    skipWhitespace(in);
    if (*in == EOF) break;
    if (*r == 'c') {
      skipLine(in);
      continue;
    }      
    ls.clear();
    read_cnf_clause(in, ls);
    clause_vector.push_back(LitSet(ls));
  }
}


void ReadQ::read_word(const char* word, size_t length) {
  while (length) {
    if (word[0] != *r) {
        string s("unexpected char in place of: ");  s+=word[0];
        throw ReadException(s);
    }
    ++r; 
    --length;
    ++word;
  }
}


void ReadQ::read() {
  read_header();

  if (qube_input && (*r == 's'))  {
    ++r;
    skipWhitespace(r);
    read_word("cnf", 3);
    skipWhitespace(r);
    cerr << "code: " << *r << endl;
    if (*r == '0') qube_output = 0;
    else if (*r == '1') qube_output = 1;
    else {
      string s("unexpected char in place of 0/1");
      throw ReadException(s);
    }   
    return; 
  }

  read_quantifiers();
  read_clauses();

  if (!unquantified_variables.empty ()) {
    if (!quantifications.empty() && quantifications[0].first==EXISTENTIAL) {
      VariableVector variables;
      FOR_EACH (vi, quantifications[0].second) variables.push_back(*vi);
      const vector<bool>& unquantified = unquantified_variables.bs();
      for (size_t index = 0; index < unquantified.size(); ++index) {
        if (unquantified[index]) variables.push_back((Var)index);
      }
      quantifications[0].second = VarVector(variables);
    } else {
      Quantification quantification;
      quantification.first=EXISTENTIAL;
      VariableVector variables;
      const vector<bool>& unquantified = unquantified_variables.bs();
      for (size_t index = 0; index < unquantified.size(); ++index) {
        if (unquantified[index]) variables.push_back((Var)index);
      }

      quantification.second=VarVector(variables);
      quantifications.insert(quantifications.begin(), quantification);
    }

  }
}
