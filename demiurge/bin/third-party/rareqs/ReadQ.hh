/* 
 * File:   Read2Q.hh
 * Author: mikolas
 *
 * Created on Tue Jan 11 15:08:14
 */
#ifndef READ2Q_HH
#define	READ2Q_HH
#include <zlib.h>
#include <utility>
#include <stdio.h>
#include "qtypes.hh"
#include "Reader.hh"
#include "ReadException.hh"
#include "LitSet.hh"
#include "VarSet.hh"
using Minisat::Lit;
using Minisat::Var;

class ReadQ {
public:
    ReadQ(Reader& r, bool qube_input=false);
    ~ReadQ();
    void                           read();
    Var                            get_max_id() const;
    const vector<Quantification>&  get_prefix() const;
    const vector< LitSet >&        get_clauses() const;
    bool                           get_header_read() const;
    int                            get_qube_output() const;
private:
    Reader&                 r;
    bool                    qube_input;
    int                     qube_output;
    Var                     max_id;
    bool                    _header_read;
    vector< LitSet >        clause_vector;
    vector<Quantification>  quantifications;
    VarSet                  quantified_variables;
    VarSet                  unquantified_variables;
    void                    read_header();
    void                    read_quantifiers();
    void                    read_clauses();
    void                    read_cnf_clause(Reader& in, vector<Lit>& lits);
    void                    read_quantification(Reader& in, Quantification& quantification);
    Var                     parse_variable(Reader& in);
    int                     parse_lit(Reader& in);
    void                    read_word(const char* word, size_t length);
};
#endif	/* READ2Q_HH */

