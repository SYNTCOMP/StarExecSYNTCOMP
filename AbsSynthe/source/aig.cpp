/**************************************************************************
 * Copyright (c) 2015, Guillermo A. Perez, Universite Libre de Bruxelles
 * 
 * This file is part of the (Swiss) AbsSynthe tool.
 * 
 * AbsSynthe is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * AbsSynthe is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with AbsSynthe.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * 
 * Guillermo A. Perez
 * Universite Libre de Bruxelles
 * gperezme@ulb.ac.be
 *************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <string>
#include <vector>
#include <set>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <iterator>
#include <iostream>
#include <ctime>
#include "cudd.h"
#include "cuddObj.hh"

#include "aiger.h"
#include "aig.h"
#include "logging.h"


// A <= B iff A - B = empty
static bool setInclusion(std::set<unsigned>* A, std::set<unsigned>* B) {
    std::set<unsigned> diff;
    std::set_difference(A->begin(), A->end(), B->begin(), B->end(),
                        std::inserter(diff, diff.begin()));
    return diff.size() == 0;
}

// A & B is non-empty
static bool emptyIntersection(std::set<unsigned>* A,
                              std::set<unsigned>* B) {
    std::set<unsigned> intersection;
    std::set_intersection(A->begin(), A->end(), B->begin(), B->end(),
                          std::inserter(intersection, intersection.begin()));
    return intersection.size() == 0;
}

unsigned AIG::maxVar() {
    return this->spec->maxvar;
}

void AIG::writeToFile(const char* aiger_file_name) {
    aiger_open_and_write_to_file(this->spec, aiger_file_name);
}

void AIG::writeToFileAsCnf(const char* dimacs_file_name, unsigned* c_lits,
                           int c_lits_length) {
    if (this->spec->num_latches) 
        errMsg ("can not handle latches");
    if (this->spec->num_bad) 
        errMsg ("can not handle bad state properties (use 'aigmove')");
    if (this->spec->num_constraints) 
        errMsg ("can not handle environment constraints (use 'aigmove')");
    if (!this->spec->num_outputs) errMsg ("no output");
    if (this->spec->num_outputs > 1) errMsg ("more than one output");
    if (this->spec->num_justice) wrnMsg ("ignoring justice properties");
    if (this->spec->num_fairness) wrnMsg ("ignoring fairness constraints");
    // the two boolean parameters, prtmap and pg, are used to control whether
    // the output dimacs file has an initial comment block with the mapping from
    // original lits to new variables and, the latter, to control whether
    // non-referenced lits can be removed.
    aiger2dimacs(this->spec, dimacs_file_name, 1, 0, c_lits, c_lits_length);
}

void AIG::addInput(unsigned lit, const char* name) {
    //dbgMsg("Just before adding an input");
    //dbgMsg("The max var = " + std::to_string(this->spec->maxvar));
    aiger_add_input(this->spec, lit, name);
}

void AIG::addOutput(unsigned and_lit, const char* name) {
    aiger_add_output(this->spec, and_lit, name);
}

void AIG::addGate(unsigned res, unsigned rh0, unsigned rh1) {
    aiger_add_and(this->spec, res, rh0, rh1);
}

unsigned AIG::optimizedGate(unsigned a_lit, unsigned b_lit) {
    if (a_lit == 0 || b_lit == 0)
        return 0;
    if (a_lit == 1 && b_lit == 1)
        return 1;
    if (a_lit == 1)
        return b_lit;
    if (b_lit == 1)
        return a_lit;
    assert(a_lit > 1 && b_lit > 1);
    unsigned a_and_b_lit = (this->maxVar() + 1) * 2;
    this->addGate(a_and_b_lit, a_lit, b_lit);
    return a_and_b_lit;
}

unsigned AIG::copyGateFromAux(const AIG* other, unsigned lit,
                              std::map<std::pair<unsigned,
                                                 unsigned>,
                                       unsigned>* cache) {
    // the cache must be there or this method is not efficient
    assert(cache != NULL);
    unsigned result;
    unsigned stripped_lit = AIG::stripLit(lit);
    if (stripped_lit == 0) {  // return the true/false 
        result = 0;
    } else {
        aiger_and* and_gate = aiger_is_and(other->spec, stripped_lit);
        // is it a gate? then recurse
        if (and_gate) {
            std::map<std::pair<unsigned, unsigned>,
                     unsigned>::iterator cache_hit = 
                cache->find(std::make_pair(and_gate->rhs0, and_gate->rhs1));
            if (cache_hit != cache->end())
                result = cache_hit->second;
            else {
                result = 
                    this->optimizedGate(this->copyGateFromAux(other,
                                                              and_gate->rhs0,
                                                              cache),
                                        this->copyGateFromAux(other,
                                                              and_gate->rhs1,
                                                              cache));
                (*cache)[std::make_pair(and_gate->rhs0, and_gate->rhs1)] = result;
            }
        } else if (stripped_lit == other->error_fake_latch.lit) {
            assert(false); // this should never occur since it's FAKE
            result = this->error_fake_latch.lit;
        } else {
            // is it an input or latch? these are base cases
            aiger_symbol* symbol = aiger_is_input(other->spec, stripped_lit);
            if (!symbol)
                symbol = aiger_is_latch(other->spec, stripped_lit);
            assert(symbol);
            result = stripped_lit;
            // we also have to make sure that the input/latch exists here
            //dbgMsg("We have a base lit from 'other': " +
            //       std::to_string(stripped_lit));
            assert(aiger_is_input(this->spec, stripped_lit) ||
                   aiger_is_latch(this->spec, stripped_lit));
        }
    }
    // let us deal with the negation now
    if (AIG::litIsNegated(lit))
        result = AIG::negateLit(result);
    return result;
}

unsigned AIG::copyGateFrom(const AIG* other, unsigned and_lit) {
    // we first confirm that we have an and
    assert(other != NULL);
    // we now have to add referenced gates recursively
    std::map<std::pair<unsigned, unsigned>, unsigned> cache;
    unsigned result = copyGateFromAux(other, and_lit, &cache);
    dbgMsg("copied a gate into lit = " + std::to_string(result));
    return result;
}

void AIG::input2gate(unsigned input, unsigned rh0) {
    aiger_redefine_input_as_and(this->spec, input, rh0, rh0);
    dbgMsg("Gated input " + std::to_string(input) + " with val = " +
           std::to_string(rh0));
}

void AIG::pushErrorLatch() {
    this->error_fake_latch.name = this->error_fake_latch_name;
    this->error_fake_latch.lit = (this->maxVar() + 1) * 2;
    this->spec->maxvar++;
    this->error_fake_latch.next = this->spec->outputs[0].lit;
    dbgMsg(std::string("Error fake latch = ") + 
           std::to_string(this->error_fake_latch.lit));
    this->latches.push_back(&(this->error_fake_latch));
}

// CAREFUL: popping the error latch is not guarded by any checks
void AIG::popErrorLatch() {
    this->spec->maxvar--;
    this->latches.pop_back();
}

void AIG::defaultValues() {
    strcpy(this->error_fake_latch_name, "error");
    this->must_clean = true;
    this->spec = NULL;
    this->lit2deps_map = new std::unordered_map<unsigned, std::set<unsigned>>();
    this->lit2ninputand_map =
        new std::unordered_map<unsigned,
                               std::pair<std::vector<unsigned>,
                                         std::vector<unsigned>>>();
    this->spec = NULL;
}

AIG::AIG() {
    this->defaultValues();
    this->spec = aiger_init();
}

AIG::AIG(const char* aiger_file_name, bool intro_error_latch) {
    this->defaultValues();
    this->spec = aiger_init();
    const char* err = aiger_open_and_read_from_file (spec, aiger_file_name);
    if (err) {
        errMsg(std::string("Error ") + err +
               " encountered while reading AIGER file " +
               aiger_file_name);
        exit(1);
    }
    if (spec->num_outputs != 1) {
        errMsg(std::string() +
               std::to_string(spec->num_outputs) + " > 1 number of outputs in " +
               "AIGER file " +
               aiger_file_name);
        exit(1);
    }
    // let us now build the vector of latches, c_inputs, and u_inputs
    for (unsigned i = 0; i < spec->num_latches; i++)
        this->latches.push_back(spec->latches + i);
    // we now introduce a fake latch for the error function
    if (intro_error_latch) {
        this->pushErrorLatch();
    }
    for (unsigned i = 0; i < spec->num_inputs; i++) {
        aiger_symbol* symbol = spec->inputs + i;
        std::string name(symbol->name);
        if (name.find("controllable") == 0) // starts with "controllable"
            this->c_inputs.push_back(symbol);
        else
            this->u_inputs.push_back(symbol);
    }

#ifndef NDEBUG
    // print some debug information
    std::string litstring;
    for (std::vector<aiger_symbol*>::iterator i = this->latches.begin();
         i != this->latches.end(); i++)
        litstring += std::to_string((*i)->lit) + ", ";
    dbgMsg(std::to_string(this->latches.size()) + " Latches: " + litstring);
    litstring.clear();
    for (std::vector<aiger_symbol*>::iterator i = this->c_inputs.begin();
         i != this->c_inputs.end(); i++)
        litstring += std::to_string((*i)->lit) + ", ";
    dbgMsg(std::to_string(this->c_inputs.size()) + " C.Inputs: " + litstring);
    litstring.clear();
    for (std::vector<aiger_symbol*>::iterator i = this->u_inputs.begin();
         i != this->u_inputs.end(); i++)
        litstring += std::to_string((*i)->lit) + ", ";
    dbgMsg(std::to_string(this->u_inputs.size()) + " U.Inputs: " + litstring);
#endif
}

AIG::AIG(const AIG &other) {
    this->must_clean = false;
    this->spec = other.spec;
    this->latches = other.latches;
    this->c_inputs = other.c_inputs;
    this->u_inputs = other.u_inputs;
    this->error_fake_latch = other.error_fake_latch;
    this->lit2deps_map = other.lit2deps_map;
    this->lit2ninputand_map = other.lit2ninputand_map;
}

AIG::~AIG() {
    if (this->must_clean) {
        dbgMsg("AIG cleaning!");
        this->cleanCaches();
        aiger_reset(this->spec);
    }
}

void AIG::cleanCaches() {
    if (this->lit2deps_map != NULL)
        delete this->lit2deps_map;
    if (this->lit2ninputand_map != NULL)
        delete this->lit2ninputand_map;
}

void AIG::getLitDepsRecur(unsigned lit, std::set<unsigned> &result,
                          std::unordered_set<unsigned>* visited) {
    unsigned stripped_lit = AIG::stripLit(lit);

    // visit the lit and its complement
    visited->insert(lit);
    visited->insert(AIG::negateLit(lit));
    
    if (stripped_lit != 0) {
        aiger_and* and_gate = aiger_is_and(this->spec, stripped_lit);
        // is it a gate? then recurse
        if (and_gate) {
            if (visited->find(and_gate->rhs0) == visited->end()) {
                this->getLitDepsRecur(and_gate->rhs0, result, visited);
            }
            if (visited->find(and_gate->rhs1) == visited->end()) {
                this->getLitDepsRecur(and_gate->rhs1, result, visited);
            }
        } else if (stripped_lit == this->error_fake_latch.lit) {
            result.insert(stripped_lit);
        } else {
            aiger_symbol* symbol = aiger_is_input(this->spec, stripped_lit);
            if (!symbol) {
                symbol = aiger_is_latch(this->spec, stripped_lit);
                assert(symbol);
                // we are sure that we have a latch here, we have to recurse
                // on latch.next
                if (visited->find(symbol->next) == visited->end()) {
                    //dbgMsg("Recursing on latch " + std::to_string(symbol->lit));
                    this->getLitDepsRecur(symbol->next, result, visited);
                }
            }
            result.insert(stripped_lit);
        }
    }
}

std::set<unsigned> AIG::getLitDeps(unsigned lit) {
    std::set<unsigned> deps;

    // check cache
    std::unordered_map<unsigned, std::set<unsigned>>::iterator cache_hit =
        this->lit2deps_map->find(lit);
    if (cache_hit != this->lit2deps_map->end()) {
        deps.insert(cache_hit->second.begin(), cache_hit->second.end());
        return deps;
    }

    std::unordered_set<unsigned> visited;
    this->getLitDepsRecur(lit, deps, &visited);
    
    // cache the result
    (*this->lit2deps_map)[lit] = deps;

    return deps;
}

void AIG::getNInputAnd(unsigned lit, std::vector<unsigned>* A,
                       std::vector<unsigned>* B) {
    assert(!AIG::litIsNegated(lit));
    aiger_and* symbol = aiger_is_and(this->spec, lit);
    assert(symbol);

    // is the result in cache?
    std::unordered_map<unsigned,
                       std::pair<std::vector<unsigned>,
                                 std::vector<unsigned>>>::iterator cache_hit =
        this->lit2ninputand_map->find(lit);
    if (cache_hit != this->lit2ninputand_map->end()) {
        A->insert(A->end(), cache_hit->second.first.begin(),
                  cache_hit->second.first.end());
        B->insert(B->end(), cache_hit->second.second.begin(),
                  cache_hit->second.second.end());
        return;
    }

    std::vector<unsigned> waiting;
    waiting.push_back(lit);
    while (waiting.size() > 0) {
        unsigned cur_lit = waiting.back();
        waiting.pop_back();
        symbol = aiger_is_and(this->spec, cur_lit);
        // we now deal with the left side of the and
        unsigned stripped_left = AIG::stripLit(symbol->rhs0);
        aiger_and* left_sym = aiger_is_and(this->spec, stripped_left);
        if (!left_sym) { // not an AND gate
            A->push_back(symbol->rhs0);
        } else if (AIG::litIsNegated(symbol->rhs0)) { // negated AND gate
            A->push_back(symbol->rhs0);
            B->push_back(stripped_left);
        } else { // non-negated AND gate, this is a recursive step
            waiting.push_back(symbol->rhs0);
        }
        // we now deal with the right side symmetrically
        unsigned stripped_right = AIG::stripLit(symbol->rhs1);
        aiger_and* right_sym = aiger_is_and(this->spec, stripped_right);
        if (!right_sym) { // not an AND gate
            A->push_back(symbol->rhs1);
        } else if (AIG::litIsNegated(symbol->rhs1)) { // negated AND gate
            A->push_back(symbol->rhs1);
            B->push_back(stripped_right);
        } else { // non-negated AND gate, this is a recursive step
            waiting.push_back(symbol->rhs1);
        }

    }

    // cache the result
    (*this->lit2ninputand_map)[lit] = std::make_pair(*A, *B);
}

BDD BDDAIG::safeRestrict(BDD original, BDD rest_region) {
    BDD approx = original.Restrict(rest_region);
    assert((approx & rest_region) == (original & rest_region));
    if (approx.nodeCount() < original.nodeCount())
        return approx;
    else
        return original;
}

unsigned AIG::numLatches(){
    return latches.size();
}

void BDDAIG::defaultValues() {
    this->mgr = NULL;
    this->must_clean = true;
    this->lit2bdd_map = new std::unordered_map<unsigned, BDD>();
    this->bdd2deps_map = new std::unordered_map<unsigned long, std::set<unsigned>>();
    this->primed_latch_cube = NULL;
    this->cinput_cube = NULL;
    this->uinput_cube = NULL;
    this->latch_cube = NULL;
    this->next_fun_compose_vec = NULL;
    this->trans_rel = NULL;
    this->short_error = NULL;
}

BDDAIG::BDDAIG(const AIG &base, Cudd* local_mgr) : AIG(base) {
    this->defaultValues();
    this->mgr = local_mgr;
}

BDDAIG::BDDAIG(const BDDAIG &base,
               std::vector<std::pair<unsigned, BDD>> adam_strat) : AIG(base) {
    this->defaultValues();
    this->mgr = base.mgr;

    // compute the full strategy and make a cube for the u inputs
    BDD full_strat = mgr->bddOne();
    BDD u_input_cube = mgr->bddOne();
    for (std::vector<std::pair<unsigned, BDD>>::iterator i = adam_strat.begin();
         i != adam_strat.end(); i++) {
        BDD cur_var = mgr->bddVar((*i).first);
        BDD cur_bdd = (*i).second;
        u_input_cube &= cur_var;
        full_strat &= (~cur_var | cur_bdd) & (cur_var | ~cur_bdd);
    }

    // we will simplify the next fun vector
    this->next_fun_compose_vec = new std::vector<BDD>();
    // fill the vector with singleton bdds except for the latches
    std::vector<aiger_symbol*>::iterator latch_it = this->latches.begin();
    bool some_change = false;
    for (unsigned i = 0; ((int) i) < this->mgr->ReadSize(); i++) {
        if (latch_it != this->latches.end() && i == (*latch_it)->lit) {
            // since we allow for short_error to override the next fun...
            BDD next_fun;
            if (i == this->error_fake_latch.lit &&
                this->short_error != NULL) {
                next_fun = *this->short_error; 
                //dbgMsg("Latch " + std::to_string(i) + " is the error latch");
            } else if (this->short_error != NULL) { // simplify functions
                next_fun = this->lit2bdd((*latch_it)->next);
                next_fun = next_fun.AndAbstract(full_strat, u_input_cube);
                //next_fun = BDDAIG::safeRestrict(next_fun,
                //                                ~(*this->short_error));
                //dbgMsg("Restricting next function of latch " +
                //std::to_string(i));
            } else {
                next_fun = this->lit2bdd((*latch_it)->next);
                next_fun = next_fun.AndAbstract(full_strat, u_input_cube);
                some_change = some_change |
                              (next_fun != this->lit2bdd((*latch_it)->next));
                dbgMsg("Simplifying the next function of latch " +
                std::to_string(i));
            }
            this->next_fun_compose_vec->push_back(next_fun);
            latch_it++;
        } else {
            this->next_fun_compose_vec->push_back(this->mgr->bddVar(i));
        }
    }
    assert(some_change);
    //dbgMsg("done with the next_fun_compose_vec");
}

BDDAIG::BDDAIG(const BDDAIG &base, BDD error) : AIG(base) {
    this->mgr = base.mgr;
    this->must_clean = false;
    this->lit2bdd_map = base.lit2bdd_map;
    this->bdd2deps_map = base.bdd2deps_map;
    this->primed_latch_cube = NULL;
    this->cinput_cube = NULL;
    this->uinput_cube = NULL;
    this->latch_cube = NULL;
    this->next_fun_compose_vec = NULL;
    this->trans_rel = NULL;
    this->short_error = new BDD(error);
    // we are now going to reduce the size of the latches and inputs based on
    // error
    dbgMsg("Creating new game with less variables");
    std::set<unsigned> deps = this->getBddDeps(error);
    std::vector<aiger_symbol*> new_vector;
    unsigned c = 0;
    for (std::vector<aiger_symbol*>::iterator i = this->latches.begin();
        i != this->latches.end(); i++) {
        
        if ((*i)->lit != this->error_fake_latch.lit &&
            deps.find((*i)->lit) == deps.end()) {
            c++;
            continue; // skip latches not in the cone of error
        }
        new_vector.push_back(*i);
    }
    dbgMsg("Removed " + std::to_string(c) + " latches");
    this->latches = new_vector;
    // controllable inputs now
    new_vector.clear();
    c = 0;
    for (std::vector<aiger_symbol*>::iterator i = this->c_inputs.begin();
         i != this->c_inputs.end(); i++) {
        if (deps.find((*i)->lit) == deps.end()) {
            c++;
            continue; // skip cinputs not in the cone of error
        }
        new_vector.push_back(*i);
    }
    dbgMsg("Removed " + std::to_string(c) + " controllable inputs");
    this->c_inputs = new_vector;
    // uncontrollable inputs to finish
    new_vector.clear();
    c = 0;
    for (std::vector<aiger_symbol*>::iterator i = this->u_inputs.begin();
         i != this->u_inputs.end(); i++) {
        if (deps.find((*i)->lit) == deps.end()) {
            c++;
            continue; // skip uinputs not in the cone of error
        }
        new_vector.push_back(*i);
    }
    dbgMsg("Removed " + std::to_string(c) + " uncontrollable inputs");
    this->u_inputs = new_vector;
}

void BDDAIG::dump2dot(BDD b, const char* file_name) {
    std::vector<BDD> v;
    v.push_back(b);
    FILE* file = fopen(file_name, "w");
    this->mgr->DumpDot(v, 0, 0, file);
    fclose(file);
}

BDDAIG::~BDDAIG() {
    if (this->primed_latch_cube != NULL)
        delete this->primed_latch_cube;
    if (this->cinput_cube != NULL)
        delete this->cinput_cube;
    if (this->uinput_cube != NULL)
        delete this->uinput_cube;
    if (this->next_fun_compose_vec != NULL)
        delete this->next_fun_compose_vec;
    if (this->trans_rel != NULL)
        delete this->trans_rel;
    if (this->short_error != NULL)
        delete this->short_error;
    
    if (this->must_clean) {
        dbgMsg("BDDAIG cleaning!");
        delete this->lit2bdd_map;
        delete this->bdd2deps_map;
    }
}

BDD BDDAIG::initState() {
    BDD result = this->mgr->bddOne();
    for (std::vector<aiger_symbol*>::iterator i = this->latches.begin();
         i != this->latches.end(); i++)
        result &= ~this->mgr->bddVar((*i)->lit);
#ifndef NDEBUG
    this->dump2dot(result, "init_state.dot");
#endif
    assert(this->isValidLatchBdd(result));
    return result;
}

BDD BDDAIG::errorStates() {
    BDD result = this->mgr->bddVar(this->error_fake_latch.lit);
#ifndef NDEBUG
    this->dump2dot(result, "error_states.dot");
#endif
    assert(this->isValidLatchBdd(result));
    return result;
}

BDD BDDAIG::primeLatchesInBdd(BDD original) {
    std::vector<BDD> latch_bdds, primed_latch_bdds;
    for (std::vector<aiger_symbol*>::iterator i = this->latches.begin();
         i != this->latches.end(); i++) {
        latch_bdds.push_back(this->mgr->bddVar((*i)->lit));
        primed_latch_bdds.push_back(this->mgr->bddVar(AIG::primeVar((*i)->lit)));
    }
    BDD result = original.SwapVariables(latch_bdds, primed_latch_bdds);
    return result;
}

BDD BDDAIG::primedLatchCube() {
    if (this->primed_latch_cube == NULL) {
        BDD result = this->mgr->bddOne();
        for (std::vector<aiger_symbol*>::iterator i = this->latches.begin();
             i != this->latches.end(); i++)
            result &= this->mgr->bddVar(AIG::primeVar((*i)->lit));
        this->primed_latch_cube = new BDD(result);
    }
    return BDD(*this->primed_latch_cube);
}

BDD BDDAIG::cinputCube() {
    if (this->cinput_cube == NULL) {
        BDD result = this->mgr->bddOne();
        for (std::vector<aiger_symbol*>::iterator i = this->c_inputs.begin();
             i != this->c_inputs.end(); i++)
            result &= this->mgr->bddVar((*i)->lit);
        this->cinput_cube = new BDD(result);
    }
    return BDD(*this->cinput_cube);
}

BDD BDDAIG::latchCube() {
    if (this->latch_cube == NULL) {
        BDD result = this->mgr->bddOne();
        for (std::vector<aiger_symbol*>::iterator i = this->latches.begin();
             i != this->latches.end(); i++)
            result &= this->mgr->bddVar((*i)->lit);
        this->latch_cube = new BDD(result);
    }
    return BDD(*this->latch_cube);
}

BDD BDDAIG::uinputCube() {
    if (this->uinput_cube == NULL) {
        BDD result = this->mgr->bddOne();
        for (std::vector<aiger_symbol*>::iterator i = this->u_inputs.begin();
             i != this->u_inputs.end(); i++)
            result &= this->mgr->bddVar((*i)->lit);
        this->uinput_cube = new BDD(result);
    }
    return BDD(*this->uinput_cube);
}

BDD BDDAIG::toCube(std::set<unsigned> &vars) {
    BDD result = this->mgr->bddOne();
    for (std::set<unsigned>::iterator i = vars.begin(); i != vars.end(); i++)
        result &= this->mgr->bddVar((*i));
    return result;
}

BDD BDDAIG::lit2bdd(unsigned lit) {
    BDD result;
    // we first check the cache
    std::unordered_map<unsigned, BDD>* cache = this->lit2bdd_map;
    if (cache->find(lit) != cache->end()){
        return (*cache)[lit];
    }
    unsigned stripped_lit = AIG::stripLit(lit);
    if (stripped_lit == 0) { // return the true/false BDD
        result = ~this->mgr->bddOne();
    } else {
        aiger_and* and_gate = aiger_is_and(this->spec, stripped_lit);
        // is it a gate? then recurse
        if (and_gate) {
            //result = (this->lit2bdd_(and_gate->rhs0) &
            //          this->lit2bdd_(and_gate->rhs1));
            result = (this->lit2bdd(and_gate->rhs0) & this->lit2bdd(and_gate->rhs1));
        } else if (stripped_lit == this->error_fake_latch.lit) {
            result = this->mgr->bddVar(stripped_lit);
        } else {
            // is it an input or latch? these are base cases
            aiger_symbol* symbol = aiger_is_input(this->spec, stripped_lit);
            if (!symbol)
                symbol = aiger_is_latch(this->spec, stripped_lit);
            assert(symbol);
            result = this->mgr->bddVar(stripped_lit);
        }
    }
    // let us deal with the negation now
    if (AIG::litIsNegated(lit))
        result = ~result;
    // cache result if possible
    if (cache != NULL) {
        (*cache)[lit] = result;
        (*cache)[AIG::negateLit(lit)] = ~result;
    }
    return result;
}

std::vector<unsigned> AIG::getCInputLits(){
    std::vector<unsigned> v;
    std::vector<aiger_symbol*>::iterator it = this->c_inputs.begin();
    for(; it != this->c_inputs.end(); it++){
        v.push_back((*it)->lit);
    }
    return v;
}

std::vector<unsigned> AIG::getUInputLits(){
    std::vector<unsigned> v;
    std::vector<aiger_symbol*>::iterator it = this->u_inputs.begin();
    for(; it != this->u_inputs.end(); it++){
        v.push_back((*it)->lit);
    }
    return v;
}

std::vector<unsigned> AIG::getLatchLits(){
    std::vector<unsigned> v;
    std::vector<aiger_symbol*>::iterator it = this->latches.begin();
    for(; it != this->latches.end(); it++){
        v.push_back((*it)->lit);
    }
    return v;
}

std::vector<BDD> BDDAIG::getNextFunVec() {
    std::vector<BDD> result;
    for (std::vector<aiger_symbol*>::iterator latch_it = this->latches.begin();
         latch_it != this->latches.end(); latch_it++) {
        result.push_back(this->lit2bdd((*latch_it)->next));
    }
    return result;
}

std::vector<BDD> BDDAIG::nextFunComposeVec(BDD* care_region=NULL) {
    if (this->next_fun_compose_vec == NULL) {
        //dbgMsg("building and caching next_fun_compose_vec");
        this->next_fun_compose_vec = new std::vector<BDD>();
        // fill the vector with singleton bdds except for the latches
        std::vector<aiger_symbol*>::iterator latch_it = this->latches.begin();
        for (unsigned i = 0; ((int) i) < this->mgr->ReadSize(); i++) {
            if (latch_it != this->latches.end() && i == (*latch_it)->lit) {
                // since we allow for short_error to override the next fun...
                BDD next_fun;
                if (i == this->error_fake_latch.lit &&
                    this->short_error != NULL) {
                    next_fun = *this->short_error; 
                    //dbgMsg("Latch " + std::to_string(i) + " is the error latch");
                } else if (this->short_error != NULL) { // simplify functions
                    next_fun = this->lit2bdd((*latch_it)->next);
                    next_fun = BDDAIG::safeRestrict(next_fun,
                                                    ~(*this->short_error));
                    //dbgMsg("Restricting next function of latch " +
                    //std::to_string(i));
                } else {
                    next_fun = this->lit2bdd((*latch_it)->next);
                    //dbgMsg("Taking the next function of latch " +
                    //std::to_string(i));
                }
                this->next_fun_compose_vec->push_back(next_fun);
                latch_it++;
            } else {
                this->next_fun_compose_vec->push_back(this->mgr->bddVar(i));
            }
        }
        //dbgMsg("done with the next_fun_compose_vec");
    }

    std::vector<BDD> result = *this->next_fun_compose_vec;

    // We restrict if required
    if (care_region != NULL) {
        std::vector<aiger_symbol*>::iterator latch_it = this->latches.begin();
        unsigned i = 0;
        for (std::vector<BDD>::iterator bdd_it = result.begin();
             bdd_it != result.end(); bdd_it++) {
            if (latch_it != this->latches.end() && i == (*latch_it)->lit) {
                (*bdd_it) = BDDAIG::safeRestrict(*bdd_it, *care_region);
            }
            i++;
        }
    }
    return result;
}

BDD BDDAIG::transRelBdd() {
    if (this->trans_rel == NULL) {
        // take the conjunction of each primed var and its next fun
        BDD result = this->mgr->bddOne();
        for (std::vector<aiger_symbol*>::iterator i = this->latches.begin();
             i != this->latches.end(); i++) {
            // since we allow for short_error to override the next fun...
            if ((*i)->lit == this->error_fake_latch.lit &&
                this->short_error != NULL) {
                BDD error_next_fun = *this->short_error; 
                result &= (~this->mgr->bddVar(AIG::primeVar((*i)->lit)) |
                           error_next_fun) &
                          (this->mgr->bddVar(AIG::primeVar((*i)->lit)) |
                           ~error_next_fun);
            } else if (this->short_error != NULL) { // simplify functions
                BDD fun = this->lit2bdd((*i)->next);
                fun = BDDAIG::safeRestrict(fun,
                                           ~(*this->short_error));
                result &= (~this->mgr->bddVar(AIG::primeVar((*i)->lit)) |
                           fun) &
                          (this->mgr->bddVar(AIG::primeVar((*i)->lit)) |
                           ~fun);
            } else {
                BDD fun = this->lit2bdd((*i)->next);
                result &= (~this->mgr->bddVar(AIG::primeVar((*i)->lit)) | fun) &
                          (this->mgr->bddVar(AIG::primeVar((*i)->lit)) | ~fun);
            }
        }
        this->trans_rel = new BDD(result);
    }
#ifndef NDEBUG
    this->dump2dot(*this->trans_rel, "trans_rel.dot");
#endif
    return *this->trans_rel;
}

std::set<unsigned> BDDAIG::getBddDeps(BDD b) {
    unsigned long key = (unsigned long) b.getRegularNode();
    if (this->bdd2deps_map->find(key) != this->bdd2deps_map->end()) {
        return (*this->bdd2deps_map)[key];
    }
    dbgMsg("bdd deps cache miss");

    std::set<unsigned> one_step_deps = this->semanticDeps(b);
    std::vector<unsigned> latch_next_to_explore;
    for (std::set<unsigned>::iterator i = one_step_deps.begin();
         i != one_step_deps.end(); i++) {
        aiger_symbol* symbol = aiger_is_latch(this->spec, *i);
        if (symbol) {
            latch_next_to_explore.push_back(symbol->next);
        }
    }
    // once we have all latch deps in one step, we can call getLitDeps (which
    // is completely recursive) and get the full set
    std::set<unsigned> result = one_step_deps;
    for (std::vector<unsigned>::iterator i = latch_next_to_explore.begin();
         i != latch_next_to_explore.end(); i++) {
        std::set<unsigned> lit_deps = this->getLitDeps(*i);
        result.insert(lit_deps.begin(), lit_deps.end());
    }

    // cache the result
    (*this->bdd2deps_map)[key] = result;
    return result;
}

std::set<unsigned> BDDAIG::getBddLatchDeps(BDD b) {
    std::set<unsigned> deps = this->getBddDeps(b);
    std::vector<unsigned> latches = this->getLatchLits();
    std::set<unsigned> depLatches;
    std::set_intersection(deps.begin(), deps.end(), latches.begin(), latches.end(),
                          std::inserter(depLatches, depLatches.begin()));
    return depLatches;
}

std::vector<BDD> BDDAIG::mergeSomeSignals(BDD cube,
                                          std::vector<unsigned>* original,
                                          int n_folds) {
    logMsg(std::to_string(original->size()) + " sub-games originally");
    const std::set<unsigned> cube_deps = this->getBddDeps(cube);
#if false
    // print some debug information
    std::string litstring;
    for (std::set<unsigned>::iterator i = cube_deps.begin();
         i != cube_deps.end(); i++)
        litstring += std::to_string(*i) + ", ";
    dbgMsg("the cube deps: " + stringOfUnsignedSet(cube_deps));
#endif
    std::vector<std::set<unsigned>> dep_vector;
    std::vector<BDD> bdd_vector;

    for (std::vector<unsigned>::iterator i = original->begin();
         i != original->end(); i++) {
        //dbgMsg("Processing subgame...");
        std::set<unsigned> lit_deps = this->getLitDeps(*i);
        std::set<unsigned> deps;
        deps.insert(cube_deps.begin(), cube_deps.end());
        deps.insert(lit_deps.begin(), lit_deps.end());
#if false
        // print some debug information
        std::string litstring;
        for (std::set<unsigned>::iterator j = deps.begin(); j != deps.end(); j++)
            litstring += std::to_string(*j) + ", ";
        dbgMsg("the current subgame has in its cone... " + litstring);
        dbgMsg("We will compare with " + std::to_string(dep_vector.size()) +
               " previous subgames");
#endif
        std::vector<std::set<unsigned>>::iterator dep_it = dep_vector.begin();
        std::vector<BDD>::iterator bdd_it = bdd_vector.begin();
        bool found = false;
        while (dep_it != dep_vector.end()) {
            if (setInclusion(&deps, &(*dep_it))) {
                //dbgMsg("this subgame is subsumed by some previous subgame");
                (*bdd_it) &= this->lit2bdd(*i);
                found = true;
                break;
            } else if (setInclusion(&(*dep_it), &deps)) {
                //dbgMsg("this new subgame subsumes some previous subgame");
                (*bdd_it) &= this->lit2bdd(*i);
                assert((*bdd_it) == ((*bdd_it) & this->lit2bdd(*i)));
                // we also update the deps because the new one is bigger
                (*dep_it) = deps;
                // WARNING: this is a new idea, should we just not add the
                // subsuming game in the end?
                found = true;
            }
            dep_it++;
            bdd_it++;
        }
        if (!found) {
            //dbgMsg("Adding new");
            dep_vector.push_back(deps);
            bdd_vector.push_back(this->lit2bdd(*i));
        }
    }

    logMsg(std::to_string(dep_vector.size()) + " sub-games after incl. red.");
    
    /* We now try to `fold' some subgames depending on whether they share
     * some variables from their dependencies already. We make this check
     * so that we do not merge subgames that have nothing to do with each
     * other
     */
    for (int i = 0; i < n_folds; i++) {
        std::vector<std::set<unsigned>> new_dep_vector;
        std::vector<BDD> new_bdd_vector;

        std::vector<std::set<unsigned>>::iterator dep_it = dep_vector.begin();
        std::vector<std::set<unsigned>>::iterator dep_next = std::next(dep_it);
        std::vector<BDD>::iterator bdd_it = bdd_vector.begin();
        std::vector<BDD>::iterator bdd_next = std::next(bdd_it);
        while (dep_next != dep_vector.end()) {
            if (!emptyIntersection(&(*dep_next), &(*dep_it))) {
                std::set<unsigned> dep_union;
                std::set_union(dep_it->begin(), dep_it->end(),
                               dep_next->begin(), dep_next->end(),
                               std::inserter(dep_union, dep_union.begin()));
                new_dep_vector.push_back(dep_union);
                new_bdd_vector.push_back((*bdd_it) & (*bdd_next));
            } else {
                new_dep_vector.push_back(*dep_it);
                new_dep_vector.push_back(*dep_next);
                new_bdd_vector.push_back(*bdd_it);
                new_bdd_vector.push_back(*bdd_next);
            }
            dep_it += 2;
            bdd_it += 2;
            if (dep_it == dep_vector.end())
                break;
            dep_next += 2;
            bdd_next += 2;
        }
        // if dep_it is not dep_vector.end() and yet we're here, the number of
        // subgames was odd and we need to add the last game back into the list
        if (dep_it != dep_vector.end()) {
            new_dep_vector.push_back(*dep_it);
            new_bdd_vector.push_back(*bdd_it);
        }
        // now we update our vectors/lists
        dep_vector = new_dep_vector;
        bdd_vector = new_bdd_vector;
    }
    logMsg(std::to_string(dep_vector.size()) + " sub-games after folding.");
    
    // as a last step, we should take NOT x AND cube, for each bdd
    std::vector<BDD> bdd_vector_with_cube;
    for (std::vector<BDD>::iterator i = bdd_vector.begin();
         i != bdd_vector.end(); i++) {
        bdd_vector_with_cube.push_back(~(*i) & cube);
    }
    /*
    dbgMsg("- Summary of Execution Times -");
    std::cout << "lit2bdd: "
              << (getAccTime("lit2bdd") / (double)CLOCKS_PER_SEC) << "\n";
    std::cout << "getLitDeps: "
              << (getAccTime("getLitDeps") / (double)CLOCKS_PER_SEC) << "\n";
    std::cout << "getBddDeps: "
              << (getAccTime("getBddDeps") / (double)CLOCKS_PER_SEC) << "\n";
    std::cout << "intersect: "
              << (getAccTime("intersect") / (double)CLOCKS_PER_SEC) << "\n";
    */
    return bdd_vector_with_cube;
}

std::set<unsigned> BDDAIG::semanticDeps(BDD b) {
    std::set<unsigned> result;
    for (unsigned i = 0; ((int) i) < this->mgr->ReadSize(); i++) {
        BDD simpler_b = b.ExistAbstract(this->mgr->bddVar(i));
        if (b != simpler_b) {
            result.insert(i);
            dbgMsg("Depends on var " + std::to_string(i));
        }
    }
    return result;
}

std::vector<BDDAIG*> BDDAIG::decompose(int n_folds) {
    std::vector<BDDAIG*> result;
    if (AIG::litIsNegated(this->error_fake_latch.next)) {
        logMsg("Decomposition possible (BIG OR case)");
        std::vector<unsigned> A, B;
        this->getNInputAnd(AIG::stripLit(this->error_fake_latch.next), &A, &B);
        std::vector<BDD> clean_signals = this->mergeSomeSignals(this->mgr->bddOne(),
                                                                &A, n_folds);
        for (std::vector<BDD>::iterator i = clean_signals.begin();
             i != clean_signals.end(); i++) {
            result.push_back(new BDDAIG(*this, *i));
        }
    } else {
        std::vector<unsigned> A, B;
        this->getNInputAnd(AIG::stripLit(this->error_fake_latch.next), &A, &B);
        if (B.size() == 0) {
            logMsg("No decomposition possible");
        } else {
            logMsg("Decomposition possible (A and [C or D] case)");
            dbgMsg(std::to_string(A.size()) + " AND leaves");
            // how do we choose an OR leaf to distribute?
            // the current heuristic is to choose the one with the most children
            unsigned b = B.back();
            B.pop_back();
            std::vector<unsigned> C, D;
            // getNInputAnd guarantees all of B is stripped
            // so we don't have to strip b
            this->getNInputAnd(b, &C, &D); 
            for (std::vector<unsigned>::iterator i = B.begin(); i != B.end(); i++) {
                std::vector<unsigned> C2, D2;
                this->getNInputAnd(*i, &C2, &D2);
                if (C2.size() > C.size()) {
                    b = (*i);
                    C = C2;
                }
            }
            logMsg("Chosen OR leaf: " + std::to_string(b));
            BDD and_leaves_cube = this->mgr->bddOne();
            for (std::vector<unsigned>::iterator i = A.begin(); i != A.end(); i++) {
                if (AIG::stripLit(*i) != b)
                    and_leaves_cube &= this->lit2bdd(*i);
            }
            std::vector<BDD> clean_signals = this->mergeSomeSignals(and_leaves_cube,
                                                                    &C, n_folds);
            for (std::vector<BDD>::iterator i = clean_signals.begin();
                 i != clean_signals.end(); i++) {
                result.push_back(new BDDAIG(*this, *i));
            }
        }
    }
    return result;
}

bool BDDAIG::isValidLatchBdd(BDD b) {
#ifndef NDEBUG
    std::set<unsigned> vars_in_cone = this->semanticDeps(b);
    unsigned hits = 0;
    for (std::vector<aiger_symbol*>::iterator i = this->latches.begin();
         i != this->latches.end(); i++) {
        if (vars_in_cone.find((*i)->lit) != vars_in_cone.end())
            hits++;
    }
    if (hits != vars_in_cone.size()) {
        std::string litstring;
        for (std::vector<aiger_symbol*>::iterator i = this->latches.begin();
             i != this->latches.end(); i++)
            litstring += std::to_string((*i)->lit) + ", ";
        dbgMsg(std::to_string(this->latches.size()) + " Latches: " + litstring);
        litstring.clear();
        for (std::vector<aiger_symbol*>::iterator i = this->c_inputs.begin();
             i != this->c_inputs.end(); i++)
            litstring += std::to_string((*i)->lit) + ", ";
        dbgMsg(std::to_string(this->c_inputs.size()) + " C.Inputs: " + litstring);
        litstring.clear();
        for (std::vector<aiger_symbol*>::iterator i = this->u_inputs.begin();
             i != this->u_inputs.end(); i++)
            litstring += std::to_string((*i)->lit) + ", ";
        dbgMsg(std::to_string(this->u_inputs.size()) + " U.Inputs: " + litstring);
        litstring.clear();
        for (std::set<unsigned>::iterator i = vars_in_cone.begin();
             i != vars_in_cone.end(); i++)
            litstring += std::to_string(*i) + ", ";
        dbgMsg(std::to_string(vars_in_cone.size()) + " Vars in cone: " + litstring);
        return false;
    } else {
        return true;
    }
#else
    return true;
#endif
}

bool BDDAIG::isValidBdd(BDD b) {
#ifndef NDEBUG
    std::set<unsigned> vars_in_cone = this->semanticDeps(b);
    unsigned hits = 0;
    for (std::vector<aiger_symbol*>::iterator i = this->latches.begin();
         i != this->latches.end(); i++) {
        if (vars_in_cone.find((*i)->lit) != vars_in_cone.end())
            hits++;
    }
    for (std::vector<aiger_symbol*>::iterator i = this->c_inputs.begin();
         i != this->c_inputs.end(); i++) {
        if (vars_in_cone.find((*i)->lit) != vars_in_cone.end())
            hits++;
    }
    for (std::vector<aiger_symbol*>::iterator i = this->u_inputs.begin();
         i != this->u_inputs.end(); i++) {
        if (vars_in_cone.find((*i)->lit) != vars_in_cone.end())
            hits++;
    }
    if (hits != vars_in_cone.size()) {
        std::string litstring;
        for (std::vector<aiger_symbol*>::iterator i = this->latches.begin();
             i != this->latches.end(); i++)
            litstring += std::to_string((*i)->lit) + ", ";
        dbgMsg(std::to_string(this->latches.size()) + " Latches: " + litstring);
        litstring.clear();
        for (std::vector<aiger_symbol*>::iterator i = this->c_inputs.begin();
             i != this->c_inputs.end(); i++)
            litstring += std::to_string((*i)->lit) + ", ";
        dbgMsg(std::to_string(this->c_inputs.size()) + " C.Inputs: " + litstring);
        litstring.clear();
        for (std::vector<aiger_symbol*>::iterator i = this->u_inputs.begin();
             i != this->u_inputs.end(); i++)
            litstring += std::to_string((*i)->lit) + ", ";
        dbgMsg(std::to_string(this->u_inputs.size()) + " U.Inputs: " + litstring);
        litstring.clear();
        for (std::set<unsigned>::iterator i = vars_in_cone.begin();
             i != vars_in_cone.end(); i++)
            litstring += std::to_string(*i) + ", ";
        dbgMsg(std::to_string(vars_in_cone.size()) + " Vars in bdd: " + litstring);
        litstring.clear();
        std::set<unsigned> all_deps = this->getBddDeps(*this->short_error);
        for (std::set<unsigned>::iterator i = all_deps.begin();
             i != all_deps.end(); i++)
            litstring += std::to_string(*i) + ", ";
        dbgMsg(std::to_string(vars_in_cone.size()) + " Vars in cone: " + litstring);
        litstring.clear();
        return false;
    } else {
        return true;
    }
#else
    return true;
#endif
}
