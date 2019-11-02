/* 
 * File:   qtypes.hh
 * Author: mikolas
 *
 * Created on January 12, 2011, 5:32 PM
 */

#ifndef Q2TYPES_HH
#define	Q2TYPES_HH
#include <utility>
#include <vector>
#include "core/SolverTypes.h" 
#include "LitSet.hh" 
#include "VarVector.hh" 
#include "minisat_auxiliary.hh"
#include <iostream>
#include <unordered_map>
#include <unistd.h>
using std::unordered_map;
using std::pair;
using std::vector;
using Minisat::Var;
using Minisat::Lit;
using std::cin;
using std::cout;
using std::cerr;
using std::endl;
using std::string;
extern int verbose;
extern int use_blocking;
extern int use_pure;
extern int use_universal_reduction;

typedef vector<Var>                           VariableVector;
enum    QuantifierType                        { UNIVERSAL, EXISTENTIAL };
typedef pair<QuantifierType,VarVector>        Quantification;
typedef vector<Quantification>                Prefix;

typedef unordered_map<int,int> Int2Int;
typedef unordered_map<int,Lit> Int2Lit;
typedef vector<LitSet> CNF;

typedef vector<QuantifierType>  Alts;

enum OpType { OR, AND };

struct QFla {
  Prefix     pref;
  CNF        cnf;
};

struct Fla {
  QuantifierType   q;
  VarVector        pref;
  OpType           op;
  vector<QFla>     flas;  
};

const Alts toAlts(const Prefix& pre);
const Alts mkAlts(QuantifierType q, size_t levels);
ostream & operator << (ostream& outs, const Alts& qs);
ostream & operator << (ostream& outs, QuantifierType q);
ostream & operator << (ostream& outs, const Fla& f);
ostream & operator << (ostream& outs, const QFla& f);
ostream & operator << (ostream& outs, const CNF& f);
ostream & operator << (ostream& outs, const Prefix& ls);
void build_fla(const Prefix& pref, const CNF& cnf, Fla& fla);
ostream & operator << (ostream& outs, const VariableVector& ls);
#endif	/* Q2TYPES_HH */

