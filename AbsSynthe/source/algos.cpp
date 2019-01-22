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

#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include <string>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <ctime>
#include <list>
#include <set>
#include <cfloat>
#include "cuddObj.hh"

#include "abssynthe.h"
#include "logging.h"
#include "aig.h"


using namespace std;

static struct {
    bool operator()(const pair<BDD, BDD> &u, const pair<BDD, BDD> &v){
        int u_ = u.second.nodeCount();
        int v_ = v.second.nodeCount();
        return u_ < v_;
    }
} bdd_pair_compare;

// TODO: the two structures below should go somewhere else... maybe this
// whole thing should be a class? The thing is: they should be set and
// cleaned before and after using this library.
struct shared_data {
    bool done;
    bool result;
    pthread_mutex_t synth_mutex;
};
// TODO: this is never cleaned after usage
static shared_data* data = NULL;

struct synthesis_data {
    vector<pair<unsigned, BDD>> c_functions;
};
static synthesis_data synth_data;


static bool outputExpected() {
    return settings.out_file != NULL ||
        settings.win_region_out_file != NULL ||
        settings.ind_cert_out_file != NULL;
}

// TODO: should this be part of the BDDAIG class?
/* I'm going to play a dangerous game here...
 * Since Cudd keeps a unique table with DdNodes that are currently referrenced
 * and the BDD class is in fact a wrapper for a pointer to such a node, I will
 * cache the actual address of the Nodes with their corresponding aig
 */
static unsigned bdd2aig(Cudd* mgr, AIG* spec, BDD a_bdd, 
                        unordered_map<unsigned long, unsigned>* cache) {
    unordered_map<unsigned long, unsigned>::iterator cache_hit =
        cache->find((unsigned long) a_bdd.getRegularNode());
    if (cache_hit != cache->end()) {
        unsigned res = (*cache_hit).second;
        if (Cudd_IsComplement(a_bdd.getNode()))
            res = AIG::negateLit(res);
        return res;
    }

    if (Cudd_IsConstant(a_bdd.getNode()))
        return (a_bdd == mgr->bddOne()) ? 1 : 0;

    unsigned a_lit = a_bdd.NodeReadIndex();

    BDD then_bdd(*mgr, Cudd_T(a_bdd.getNode()));
    BDD else_bdd(*mgr, Cudd_E(a_bdd.getNode()));
    /* We are performing the following operation
     *
     * ite(a_bdd, then_bdd, else_bdd)
     * = a^then v ~a^else
     * = ~(~(a^then) ^ ~(~a^else))
     *
     * so we need 3 more ANDs
     */
    unsigned then_lit = bdd2aig(mgr, spec, then_bdd, cache);
    unsigned else_lit = bdd2aig(mgr, spec, else_bdd, cache);
    unsigned a_then_lit = spec->optimizedGate(a_lit, then_lit);
    unsigned na_else_lit = spec->optimizedGate(AIG::negateLit(a_lit), else_lit);
    unsigned n_a_then_lit = AIG::negateLit(a_then_lit);
    unsigned n_na_else_lit = AIG::negateLit(na_else_lit);
    unsigned ite_lit = spec->optimizedGate(n_a_then_lit, n_na_else_lit);
    unsigned res = AIG::negateLit(ite_lit);

    (*cache)[(unsigned long) a_bdd.getRegularNode()] = res;

    if (Cudd_IsComplement(a_bdd.getNode()))
        res = AIG::negateLit(res);

    return res;
}

static vector<pair<unsigned, BDD>> synthAlgo(Cudd* mgr, BDDAIG* spec,
                                             BDD non_det_strategy, BDD care_set) {
    BDD strategy = non_det_strategy;
#ifndef NDEBUG
    spec->dump2dot(strategy, "strategy.dot");
    set<unsigned> deps = spec->semanticDeps(strategy);
    string litstring;
    for (set<unsigned>::iterator i = deps.begin(); i != deps.end(); i++)
        litstring += to_string(*i) + ", ";
    dbgMsg("Semantic deps of the non-det strat: " + litstring);
#endif
    vector<aiger_symbol*> c_inputs = spec->getCInputs();
    vector<unsigned> c_input_lits;
    vector<BDD> c_input_funs;
#ifndef NDEBUG
    dbgMsg("non-det strategy BDD size: " +
           to_string(non_det_strategy.nodeCount()));
#endif

    // as a first step, we compute a single bdd per controllable input
    for (vector<aiger_symbol*>::iterator i = c_inputs.begin();
         i != c_inputs.end(); i++) {
        BDD c = mgr->bddVar((*i)->lit);
        BDD others_cube = mgr->bddOne();
        unsigned others_count = 0;
        for (vector<aiger_symbol*>::iterator j = c_inputs.begin();
             j != c_inputs.end(); j++) {
            //dbgMsg("CInput " + to_string((*j)->lit));
            if ((*i)->lit == (*j)->lit)
                continue;
            others_cube &= mgr->bddVar((*j)->lit);
            //dbgMsg("Other cube has lit " + to_string((*j)->lit));
            others_count++;
        }
        BDD c_arena;
        if (others_count > 0)
            c_arena = strategy.ExistAbstract(others_cube);
        else {
            //dbgMsg("No need to abstract other cinputs");
            c_arena = strategy;
        }
#ifndef NDEBUG
        spec->dump2dot(c_arena, "c_arena.dot");
        spec->dump2dot(c_arena.Cofactor(c), "c_arena_true.dot");
#endif
        // pairs (x,u) in which c can be true
        BDD can_be_true = c_arena.Cofactor(c);
        //dbgMsg("Can be true BDD size: " + to_string(can_be_true.nodeCount()));
        // pairs (x,u) in which c can be false
        BDD can_be_false = c_arena.Cofactor(~c);
        //dbgMsg("Can be false BDD size: " + to_string(can_be_false.nodeCount()));
        BDD must_be_true = (~can_be_false) & can_be_true;
        //dbgMsg("Must be true BDD size: " + to_string(must_be_true.nodeCount()));
        BDD must_be_false = (~can_be_true) & can_be_false;
        //dbgMsg("Must be false BDD size: " + to_string(must_be_false.nodeCount()));
        BDD local_care_set = care_set & (must_be_true | must_be_false);
        // on care set: must_be_true.restrict(care_set) <-> must_be_true
        // or         ~(must_be_false).restrict(care_set) <-> ~must_be_false
        BDD opt1 = BDDAIG::safeRestrict(must_be_true, local_care_set);
        BDD opt2 = BDDAIG::safeRestrict(~must_be_false, local_care_set);
        // choose the smallest
        BDD res;
        if (opt1.nodeCount() < opt2.nodeCount())
            res = opt1;
        else
            res = opt2;
        // there are two other possibilities we could consider using a trick
        // from Vardi's RSynth tool
        if (settings.use_rsynth) {
            // self-substitute using can_be_true and not can_be_false
            opt1 = BDDAIG::safeRestrict(can_be_true, local_care_set);
            opt2 = BDDAIG::safeRestrict(~can_be_false, local_care_set);
            if (opt1.nodeCount() < opt2.nodeCount() &&
                    opt1.nodeCount() < res.nodeCount())
                res = opt1;
            else if (opt2.nodeCount() < res.nodeCount())
                res = opt2;
        }
#ifndef NDEBUG
        dbgMsg("Size of function for " + to_string(c.NodeReadIndex()) + " = " +
               to_string(res.nodeCount()));
#endif
        //strategy &= (~c | res) & (~res | c);
        strategy = strategy.Compose(res, (*i)->lit);
        c_input_funs.push_back(res);
        c_input_lits.push_back((*i)->lit);
    }

    vector<pair<unsigned, BDD>> result;
    vector<unsigned>::iterator i = c_input_lits.begin();
    vector<BDD>::iterator j = c_input_funs.begin();
    for (; i != c_input_lits.end();) {
        result.push_back(make_pair(*i, *j));
        i++;
        j++;
    }

    return result;
}

static void finalizeSynth(Cudd* mgr, AIG* spec,
                          vector<pair<unsigned, BDD>> result=
                          synth_data.c_functions) {
    // we now get rid of all controllable inputs in the aig spec by replacing
    // each one with an and computed using bdd2aig...
    if (settings.final_reordering) {
        mgr->ReduceHeap(CUDD_REORDER_SIFT_CONVERGE, 0);
    }
    // NOTE: because of the way bdd2aig is implemented, we must ensure that BDDs are
    // no longer operated on after this point!
    unordered_map<unsigned long, unsigned> cache;
    vector<unsigned> cinputs = spec->getCInputLits();
    for (vector<pair<unsigned, BDD>>::iterator i = result.begin();
         i != result.end(); i++) {
        spec->input2gate(i->first, bdd2aig(mgr, spec, i->second, &cache));
#ifndef NDEBUG
        dbgMsg("final function BDD size: " +
               to_string((i->second).nodeCount()));
#endif
        cinputs.erase(remove(cinputs.begin(), cinputs.end(), i->first),
                      cinputs.end());
    }
    for (vector<unsigned>::iterator i = cinputs.begin();
         i != cinputs.end(); i++) {
        logMsg("Setting unused cinput " + to_string(*i));
        spec->input2gate(*i, bdd2aig(mgr, spec, ~mgr->bddOne() , &cache));
    }
    // Finally, we write the modified spec to file
    spec->writeToFile(settings.out_file);
    // Cleaning
    synth_data.c_functions.clear();
}

static void outputWinRegion(Cudd* mgr, BDDAIG* spec, BDD winning_region) {
    // we will work on a clean spec
    AIG blank_spec;
    vector<aiger_symbol*> latches = spec->getLatches();
    unordered_map<unsigned long, unsigned> cache;
    dbgMsg("Adding latches to the new AIG instance.");
    for (vector<aiger_symbol*>::iterator i = latches.begin();
         i != latches.end(); i++) {
        //dbgMsg("Adding latch as input, lit = " + to_string((*i)->lit));
        blank_spec.addInput((*i)->lit, (*i)->name);
    }
    blank_spec.addOutput(bdd2aig(mgr, &blank_spec,
                                 winning_region, &cache), "winning region");
    // Finally, we write the file
    dbgMsg("About to write the winning region");
    blank_spec.writeToFile(settings.win_region_out_file);
    dbgMsg("Winning region done!");
}

static void outputIndCertificate(Cudd* mgr, BDDAIG* spec, BDD winning_region) {
    // we will work on a clean spec
    AIG blank_spec;
    vector<aiger_symbol*> latches = spec->getLatches();
    vector<aiger_symbol*> uinputs = spec->getUInputs();
    vector<aiger_symbol*> cinputs = spec->getCInputs();
    vector<BDD> original_latch;
    unordered_map<unsigned long, unsigned> cache;
    map<pair<unsigned, unsigned>, unsigned> copy_cache;
    dbgMsg("Adding latches to the new AIG instance.");
    for (vector<aiger_symbol*>::iterator i = latches.begin();
         i != latches.end(); i++) {
        //dbgMsg("Adding latch as input, lit = " + to_string((*i)->lit));
        blank_spec.addInput((*i)->lit, (*i)->name);
        original_latch.push_back(mgr->bddVar((*i)->lit));
    }
    dbgMsg("Adding uncontrollable inputs.");
    for (vector<aiger_symbol*>::iterator i = uinputs.begin();
         i != uinputs.end(); i++) {
        dbgMsg("Adding uncontrollable input, lit = " + to_string((*i)->lit));
        blank_spec.addInput((*i)->lit, (*i)->name);
    }
    dbgMsg("Adding controllable inputs.");
    unsigned c_lits[cinputs.size()];
    int m = 0;
    for (vector<aiger_symbol*>::iterator i = cinputs.begin();
         i != cinputs.end(); i++) {
        blank_spec.addInput((*i)->lit, (*i)->name);
        c_lits[m++] = (*i)->lit;
    }
    // for each latch we also need to add a boolean function to determine its
    // next value...
    // in the process we also make sure that there are no trivial latches
    vector<unsigned> latch_next_lits;
    dbgMsg("Adding latch next functions.");
    BDD simple_winning_region = winning_region;
    for (vector<aiger_symbol*>::iterator i = latches.begin();
         i != latches.end(); i++) {
        dbgMsg("working on function for latch " + to_string((*i)->lit));
        unsigned var = blank_spec.copyGateFromAux(spec, (*i)->next,
                                                  &copy_cache);
        dbgMsg("copied a gate into lit = " + to_string(var));
        latch_next_lits.push_back(var);
        //blank_spec.addOutput(var, NULL);
    }
    dbgMsg("Creating BDDs from the new and-gate lits");
    vector<BDD> latch_next;
    vector<BDD>::iterator l = original_latch.begin();
    vector<BDD> clean_original_latch;
    vector<unsigned>::iterator j = latch_next_lits.begin();
    for (vector<aiger_symbol*>::iterator k = latches.begin();
         k != latches.end(); k++) {
        unsigned var = *j;
        if (var == 1) {
            dbgMsg("Positive cofactor of lit " + to_string((*k)->lit));
            simple_winning_region =
                simple_winning_region.Cofactor(mgr->bddVar((*k)->lit));
        } else if (var == 0) {
            dbgMsg("Negative cofactor of lit " + to_string((*k)->lit));
            simple_winning_region =
                simple_winning_region.Cofactor(~mgr->bddVar((*k)->lit));
        } else {
            BDD var_bdd = mgr->bddVar(*j); //AIG::stripLit(*j));
            //if (AIG::litIsNegated(*j)) {
            //    dbgMsg("We have a negated lit!");
            //    var_bdd = ~var_bdd;
            //}
            latch_next.push_back(var_bdd);
            clean_original_latch.push_back(*l);
        }
        j++;
        l++;
    }
    // at this point we should have a max var smaller than that in the original
    // spec because we only have as much logic as there
    dbgMsg("max var so far = " + to_string(blank_spec.maxVar()));
    dbgMsg("max var originally = " + to_string(spec->maxVar()));
    assert(blank_spec.maxVar() <= spec->maxVar());
    // we now take the winning region BDD and replace latches for the variables
    // in latch_next, since we are changing BDDs we should flush the cache
    dbgMsg("Swapping variables");
    assert(clean_original_latch.size() == latch_next.size());
#ifndef NDEBUG
    spec->semanticDeps(winning_region);
#endif
    BDD primed_winning_region =
        simple_winning_region.SwapVariables(clean_original_latch,
                                            latch_next);
#ifndef NDEBUG
    spec->semanticDeps(primed_winning_region);
#endif
    // we need to add a boolean function to determine if the latch config
    // corresponds to a winning state
    // IMPORTANT: no more BDD manipulation after this point since I am caching
    // the numbering of the nodes to do the bdd 2 aig translation
    unsigned w = bdd2aig(mgr, &blank_spec, winning_region, &cache);
    dbgMsg("W = " + to_string(w));
    //blank_spec.addOutput(w, NULL);
    dbgMsg("Creating the and for the output signal");
    unsigned w_primed = bdd2aig(mgr, &blank_spec, primed_winning_region, &cache);
    dbgMsg("W' = " + to_string(w_primed));
    //blank_spec.addOutput(w_primed, NULL);
    // we want W => W'(L<-f_l) so we will do ~(w & ~w')
    blank_spec.addOutput(AIG::negateLit(
                         blank_spec.optimizedGate(w, AIG::negateLit(w_primed))),
                         "inductivity check");
    // output certificate in different formats
    string file_string(settings.ind_cert_out_file);
    string file_ext = file_string.substr(file_string.find_last_of(".") + 1);
    if (file_ext == "qdimacs") {
        blank_spec.writeToFileAsCnf(settings.ind_cert_out_file, c_lits,
                                    cinputs.size());
    } else if ((file_ext == "aig") || (file_ext == "aag")) {
        blank_spec.writeToFile(settings.ind_cert_out_file);
    } else {
        errMsg("Inductive certificate file extension not known");
    }
}

static BDD substituteLatchesNext(BDDAIG* spec, BDD dst, BDD* care_region=NULL) {
    BDD result;
    if (settings.use_trans) {
        BDD trans_rel_bdd = spec->transRelBdd();
        if (care_region != NULL)
            trans_rel_bdd = BDDAIG::safeRestrict(trans_rel_bdd, *care_region);
        BDD primed_dst = spec->primeLatchesInBdd(dst);
        BDD primed_latch_cube = spec->primedLatchCube();
        result = trans_rel_bdd.AndAbstract(primed_dst,
                                           primed_latch_cube);
    } else {
        vector<BDD> next_funs = spec->nextFunComposeVec(care_region);
        result = dst.VectorCompose(next_funs);
#if false
        BDD trans_rel_bdd = spec->transRelBdd();
        BDD primed_dst = spec->primeLatchesInBdd(dst);
        BDD primed_latch_cube = spec->primedLatchCube();
        BDD result2 = trans_rel_bdd.AndAbstract(primed_dst,
                                                primed_latch_cube);
        if (result != result2) {
            errMsg("Vector compose resulted in the wrong BDD");
        }
#endif
    }
    return result;
}

// ABSTRACTION ALGORITHM (adapted by Romain Brenguier from simpleBDDSolver)
static BDD abstractSafeCpreAux(BDDAIG* spec, BDD safe, BDD untracked_actions,
                               BDD cache) {
    BDD trans_bdd = cache;
    BDD cinput_tracked_cube = spec->cinputCube();
    BDD temp_bdd = trans_bdd.ExistAbstract(cinput_tracked_cube);
    BDD uinput_tracked_cube = spec->uinputCube().ExistAbstract(untracked_actions);
    return temp_bdd.UnivAbstract(uinput_tracked_cube);
}

static BDD abstractSafeCpre(BDDAIG* spec, BDD safe,
                            BDD untracked_latches,
                            BDD untracked_actions,
                            BDD &cache_trans_bdd,
                            BDD &cache_non_absd_result) {
    BDD care = ~safe;
    cache_trans_bdd = substituteLatchesNext(spec, safe, &care);
    BDD cinput_tracked_cube = spec->cinputCube();
    BDD temp_bdd = cache_trans_bdd.ExistAbstract(cinput_tracked_cube);
    BDD uinput_tracked_cube = spec->uinputCube().ExistAbstract(untracked_actions);
    cache_non_absd_result = temp_bdd.UnivAbstract(uinput_tracked_cube);
    if (untracked_latches.IsZero())
        return cache_non_absd_result;
    else
        return cache_non_absd_result.ExistAbstract(untracked_latches);
}

static bool internalSolveAbstract(Cudd* mgr, BDDAIG* spec, const BDD* cpre_init,
                                  bool do_synth=false) {
    unsigned cnt = 0;
    dbgMsg("Internal solve abstract");
    BDD init_state = spec->initState();
    BDD error_states;
    if (cpre_init != NULL)
        error_states = *cpre_init;
    else
        error_states = spec->errorStates();
    BDD cache_trans, cache_non_absd_result;
    BDD tracked_latches = error_states.Support();
    BDD untracked_latches = spec->latchCube().ExistAbstract(tracked_latches);
    BDD untracked_actions = mgr->bddOne();//spec->uinputCube();
    bool includes_init = (init_state & error_states).IsZero();
  
    BDD mayWin = ~error_states;
  
    bool cont = true;
    BDD cache;
  
    while (cont) {
        dbgMsg("Algos.cpp: Refinement step " + to_string(cnt));
        dbgMsg("Number of untracked latches = " + 
               to_string(untracked_latches.SupportSize()));
        dbgMsg("Number of untracked actions = " + 
               to_string(untracked_actions.SupportSize()));
  
        // Compute the fixpoint of Safe_CPRE in mayWin
        int cnt2 = 0;
        includes_init = false;
        bool cont2 = true;
        while (cont2 && ! includes_init) {
  	        dbgMsg("Fixpoint step " + to_string(cnt2));
  	        BDD res = abstractSafeCpre(spec, mayWin, untracked_latches,
                                       untracked_actions,
                                       cache_trans, cache_non_absd_result);
  	        BDD mayWin1 = mayWin;
            mayWin = mayWin & res;
            if (mayWin1 == mayWin) {
                cont2 = false;
                cache = cache_trans;
            }
            includes_init = (init_state & mayWin).IsZero();
            cnt2++;
        }
  
        cache = ~abstractSafeCpreAux(spec, mayWin, untracked_actions, 
                                     cache_trans);
        BDD mayLose = mayWin & cache;
  
        if (includes_init || mayLose.IsZero()) 
            cont = false;
        else {
            cache = mgr->bddOne();
            dbgMsg("Promoting");
            BDD implicant = mayLose.LargestCube();
            dbgMsg("Size of the implicant = " + to_string(implicant.SupportSize()));
  
            untracked_latches = untracked_latches.ExistAbstract(implicant.Support());
            BDD new_untracked = untracked_actions.ExistAbstract(implicant.Support());
            mayWin = mayWin & ~mayLose.ExistAbstract(untracked_actions)
                                      .UnivAbstract(untracked_latches);
            untracked_actions = new_untracked;
            cnt++;
        }
    }
    
    BDD bad_transitions = ~cache_trans; 
  
    dbgMsg("Early exit? " + to_string(includes_init) + 
  	       ", after " + to_string(cnt) + " iterations.");
  
    if (!includes_init && do_synth && outputExpected()) {
        dbgMsg("acquiring lock on synth mutex");
        if (data != NULL) pthread_mutex_lock(&data->synth_mutex);
        BDD clean_winning_region = (mayWin).Cofactor(~spec->errorStates());
        // let us clean the AIG before we start introducing new stuff
        spec->popErrorLatch();
        if (settings.out_file != NULL) {
            dbgMsg("Starting synthesis");
            synth_data.c_functions = synthAlgo(mgr, spec,
                                               ~bad_transitions,
                                               clean_winning_region);
        }
        if (settings.win_region_out_file != NULL) {
            dbgMsg("Starting output of winning region");
            outputWinRegion(mgr, spec, clean_winning_region);
        }
        if (settings.ind_cert_out_file != NULL) {
            dbgMsg("Starting output of inductive certificate");
            outputIndCertificate(mgr, spec, clean_winning_region);
        }
    }
    return !includes_init;
}

static bool internalSolveAbstractBackAndForth(Cudd* mgr, BDDAIG* spec,
                                              const BDD* cpre_init,
                                              bool do_synth=false) {
    int threshold = settings.abs_threshold;
    unsigned cnt = 0;
    dbgMsg("Internal solve abstract, threshold = " + to_string(threshold));
    BDD init_state = spec->initState();
    BDD error_states;
    if (cpre_init != NULL)
        error_states = *cpre_init;
    else
        error_states = spec->errorStates();
    BDD mayWin = ~error_states;
    BDD cache_trans_bdd;
    BDD cache_non_abstracted_result;
    BDD untracked_latches= spec->latchCube();
    bool fixpoint = false;
    bool abstract_algo = false;
  
    while ((!(init_state & mayWin).IsZero()) && (!fixpoint)) {
        dbgMsg("Number of untracked latches = " + 
               to_string(untracked_latches.SupportSize()));    
        if (mayWin.nodeCount() < threshold) { 
            abstract_algo = false;
            dbgMsg("Algos.cpp: Size < Threshold ; Fixpoint step " + 
                   to_string(++cnt) + " Bdd size : " +
                   to_string(mayWin.nodeCount()));
            BDD mayWin1 = mayWin & abstractSafeCpre(spec, mayWin,
                                                    mgr->bddOne(),
                                                    mgr->bddOne(),
                                                    cache_trans_bdd,
                                                    cache_non_abstracted_result);
            fixpoint = (mayWin1 == mayWin);
            mayWin = mayWin1;
        } else {
            dbgMsg("Algos.cpp: Threshold <= Size; Fixpoint step " +
                   to_string(++cnt) + " Bdd size : " +
                   to_string(mayWin.nodeCount()));
            dbgMsg("Number of untracked latches = " +
                   to_string(untracked_latches.SupportSize()));
            if (!abstract_algo) { 
                if(untracked_latches== spec->latchCube()) {
                    BDD implicant = mayWin.LargestCube();
                    untracked_latches = untracked_latches.
                        ExistAbstract(implicant.Support());
                }
                mayWin = mayWin.ExistAbstract(untracked_latches);
                abstract_algo = true;
            }
            BDD mayWin1 = mayWin & abstractSafeCpre(spec, mayWin,
                                                    untracked_latches,
                                                    mgr->bddOne(),
                                                    cache_trans_bdd,
                                                    cache_non_abstracted_result);
            if (mayWin1 == mayWin) {
                dbgMsg("Promoting");
                BDD mayLose = mayWin & ~cache_non_abstracted_result;
                BDD implicant = mayLose.LargestCube();
                dbgMsg("Size of the implicant = " +
                       to_string(implicant.SupportSize()));
                BDD new_untracked = untracked_latches.
                    ExistAbstract(implicant.Support());
                dbgMsg("Number of newly untracked latches = " +
                       to_string(new_untracked.SupportSize()));	  
                if (new_untracked == untracked_latches) {
                    dbgMsg("fixpoint");
                    fixpoint = true;
                } else
                   untracked_latches = new_untracked;
            }
            mayWin = mayWin1;
        }
    }

    BDD bad_transitions = ~cache_trans_bdd;
                          //~abstractSafeCpreAux(spec, mayWin, 
                          //                     mgr->bddOne(), cache_trans_bdd);

    bool includes_init = (init_state & mayWin).IsZero();
    dbgMsg("Early exit? " + to_string(includes_init) + 
           ", after " + to_string(cnt) + " iterations.");

    if (!includes_init && do_synth && outputExpected()) {
        dbgMsg("acquiring lock on synth mutex");
        if (data != NULL) pthread_mutex_lock(&data->synth_mutex);
        BDD clean_winning_region = (mayWin).Cofactor(~spec->errorStates());
        // let us clean the AIG before we start introducing new stuff
        spec->popErrorLatch();
        if (settings.out_file != NULL) {
            dbgMsg("Starting synthesis");
            synth_data.c_functions = synthAlgo(mgr, spec,
                                               ~bad_transitions,
                                               clean_winning_region);
        }
        if (settings.win_region_out_file != NULL) {
            dbgMsg("Starting output of winning region");
            outputWinRegion(mgr, spec, clean_winning_region);
        }
        if (settings.ind_cert_out_file != NULL) {
            dbgMsg("Starting output of inductive certificate");
            outputIndCertificate(mgr, spec, clean_winning_region);
        }
    }
    return !includes_init;
}

// NOTE: trans_bdd contains the set of all transitions going into bad
// states after the upre step (the complement of all good transitions)
static BDD upre(BDDAIG* spec, BDD dst, BDD &trans_bdd) {
    BDD not_dst = ~dst;
    trans_bdd = substituteLatchesNext(spec, dst, &not_dst);
    BDD cinput_cube = spec->cinputCube();
    BDD uinput_cube = spec->uinputCube();
    BDD temp_bdd = trans_bdd.UnivAbstract(cinput_cube);
    return temp_bdd.ExistAbstract(uinput_cube);
}

static bool internalSolveExact(Cudd* mgr, BDDAIG* spec, const BDD* upre_init, 
                               BDD* losing_region, BDD* losing_transitions,
                               bool do_synth=false) {
    //static int occ_counter = 1;
    dbgMsg("Computing fixpoint of UPRE.");
    //cout << "Visited " << occ_counter++ << endl;
    bool includes_init = false;
    unsigned cnt = 0;
    BDD bad_transitions;
    BDD init_state = spec->initState();
    BDD error_states;
    if (upre_init != NULL)
        error_states = *upre_init;
    else
        error_states = spec->errorStates();
    BDD prev_error = ~mgr->bddOne();
    includes_init = ((init_state & error_states) != ~mgr->bddOne());
    while (!includes_init && error_states != prev_error) {
        prev_error = error_states;
        error_states = prev_error | upre(spec, prev_error, bad_transitions);
        includes_init = ((init_state & error_states) != ~mgr->bddOne());
        cnt++;
    }
    
    dbgMsg("Early exit? " + to_string(includes_init) + 
           ", after " + to_string(cnt) + " iterations.");

#ifndef NDEBUG
    spec->dump2dot(error_states & init_state, "uprestar_and_init.dot");
#endif
    if (losing_region != NULL) {
        *losing_region = error_states;
    }
    if (losing_transitions != NULL){
        *losing_transitions = bad_transitions;
    } 
    // if !includes_init == true, then ~bad_transitions is the set of all
    // good transitions for controller (Eve)
    if (!includes_init && do_synth && outputExpected()) {
        dbgMsg("acquiring lock on synth mutex");
        if (data != NULL) pthread_mutex_lock(&data->synth_mutex);
        BDD clean_winning_region = (~error_states).Cofactor(~spec->errorStates());
#ifndef NDEBUG
        // we can check that the bdd represents an inductive winning region as
        // follows
        BDD temp_bad_trans;
        assert(((~clean_winning_region |
                 upre(spec, ~clean_winning_region, temp_bad_trans)) &
                clean_winning_region) == ~mgr->bddOne());
#endif
        // let us clean the AIG before we start introducing new stuff
        spec->popErrorLatch();
        if (settings.out_file != NULL) {
            dbgMsg("Starting synthesis");
            synth_data.c_functions = synthAlgo(mgr, spec,
                                               ~bad_transitions,
                                               ~error_states);
        }
        if (settings.win_region_out_file != NULL) {
            dbgMsg("Starting output of winning region");
            outputWinRegion(mgr, spec, clean_winning_region);
        }
        if (settings.ind_cert_out_file != NULL) {
            dbgMsg("Starting output of inductive certificate");
            outputIndCertificate(mgr, spec, clean_winning_region);
        }
    }
    return !includes_init;
}

static bool internalSolve(Cudd* mgr, BDDAIG* spec, const BDD* upre_init, 
                          BDD* losing_region, BDD* losing_transitions,
                          bool do_synth=false) {
    // if the caller does not care about the losing region or losing
    // transitions, then we can use abstraction if the use_abs flag was
    // set
    if (settings.use_abs &&
        (losing_region == NULL) &&
        (losing_transitions == NULL)) {
        dbgMsg("Using an internal abstract solver");

        if (!settings.abs_threshold)
            return internalSolveAbstract(mgr, spec, upre_init, do_synth);
        else
            return internalSolveAbstractBackAndForth(mgr, spec,
                                                     upre_init, do_synth);
    }

    // otherwise, just use the exact version
    return internalSolveExact(mgr, spec, upre_init, losing_region,
                              losing_transitions, do_synth);
}

static bool compSolve1(Cudd* mgr, BDDAIG* spec) {
    vector<BDDAIG*> subgames = spec->decompose(settings.n_folds);
    unsigned gamecount = 0;
    if (subgames.size() == 0) return internalSolve(mgr, spec, NULL, NULL,
                                                   NULL, true);
    vector<pair<BDD,BDD> > subgame_results;
    for (vector<BDDAIG*>::iterator i = subgames.begin(); i != subgames.end(); i++) {
        gamecount++;
        BDD error_states;
        BDD bad_transitions;
        dbgMsg("Solving subgame " + to_string(gamecount) + " (" +
               to_string((*i)->numLatches()) + " latches)");
        if (!internalSolve(mgr, *i, NULL, &error_states, &bad_transitions)) {
            return false;
        } else {
            subgame_results.push_back(pair<BDD,BDD>(error_states,
                                                         bad_transitions));
        }
    }

    bool includes_init = false;
    BDD bad_transitions;

    // Check if subgames are cinput-independent
    bool cinput_independent = true;
    set<unsigned> total_cinputs;
    for (vector<BDDAIG*>::iterator i = subgames.begin(); i != subgames.end(); i++) {
        vector<unsigned> ic = (*i)->getCInputLits();
        set<unsigned> intersection;
        set_intersection(ic.begin(), ic.end(), total_cinputs.begin(), 
                         total_cinputs.end(), 
                         inserter(intersection,intersection.begin()));
        if (intersection.size() > 0 ) {
            cinput_independent = false;
            break;
        } else {
            total_cinputs.insert(ic.begin(), ic.end());
        }
    }   
    // dbgMsg("Are we cinput-independent? " + to_string(cinput_independent));

    if (!cinput_independent) { // we still have one game to solve
        // release cache memory and other stuff used in BDDAIG instances
        //cout << ("Not cinput independent\n");
        //cout.flush();
        for (vector<BDDAIG*>::iterator i = subgames.begin();
             i != subgames.end(); i++)
            delete *i;
        BDD losing_states = spec->errorStates();
        BDD losing_transitions = ~mgr->bddOne();
        vector<pair<BDD, BDD> >::iterator sg = subgame_results.begin();
        sort(subgame_results.begin(), subgame_results.end(), bdd_pair_compare);
        for (sg = subgame_results.begin(); sg != subgame_results.end(); sg++) {
            losing_states |= sg->first;
            losing_transitions |= sg->second;
        }
        dbgMsg("Solving the aggregated game");
        // TODO: try out using losing_states instead of losing_transition here
        BDDAIG aggregated_game(*spec, losing_transitions);
        BDD error_states;
        includes_init = !internalSolve(mgr, &aggregated_game, &losing_states,
                                       &error_states, &bad_transitions);

        // if !includes_init == true, then ~bad_transitions is the set of all
        // good transitions for controller (Eve)
        if (!includes_init && outputExpected()) {
            dbgMsg("acquiring lock on synth mutex");
            if (data != NULL) pthread_mutex_lock(&data->synth_mutex);
            BDD clean_winning_region = (~error_states).Cofactor(
                    ~spec->errorStates());
            // let us clean the AIG before we start introducing new stuff
            spec->popErrorLatch();
            if (settings.out_file != NULL) {
                dbgMsg("Starting synthesis");
                synth_data.c_functions = synthAlgo(mgr, spec,
                                                   ~bad_transitions,
                                                   ~error_states);
            }
            if (settings.win_region_out_file != NULL) {
                dbgMsg("Starting output of winning region");
                outputWinRegion(mgr, spec, clean_winning_region);
            }
            if (settings.ind_cert_out_file != NULL) {
                dbgMsg("Starting output of inductive certificate");
                outputIndCertificate(mgr, spec, clean_winning_region);
            }
        }

    } else if (outputExpected()) {
        dbgMsg("acquiring lock on synth mutex");
        if (data != NULL) pthread_mutex_lock(&data->synth_mutex);
        dbgMsg("Synthesis via comp 1");
        vector<pair<unsigned, BDD>> all_cinput_strats;
        vector<pair<BDD, BDD>>::iterator sg = subgame_results.begin();
        BDD global_lose = ~mgr->bddOne();
        for (vector<BDDAIG*>::iterator i = subgames.begin();
             i != subgames.end(); i++) {
            vector<pair<unsigned, BDD>> temp;
            temp = synthAlgo(mgr, *i, ~sg->second, ~sg->first);
            global_lose |= sg->first;
            // logMsg("Found " + to_string(temp.size()) + " cinputs here");
            all_cinput_strats.insert(all_cinput_strats.end(), 
                                     temp.begin(), temp.end());
            sg++;
            delete *i;
        }
        BDD clean_winning_region = (~global_lose).Cofactor(~spec->errorStates());
        // let us clean the AIG before we start introducing new stuff
        spec->popErrorLatch();
        if (settings.out_file != NULL) {
            dbgMsg("Starting synth");
            synth_data.c_functions = all_cinput_strats;
        }
        if (settings.win_region_out_file != NULL) {
            dbgMsg("Starting output of winning region");
            outputWinRegion(mgr, spec, clean_winning_region);
        }
        if (settings.ind_cert_out_file != NULL) {
            dbgMsg("Starting output of inductive certificate");
            outputIndCertificate(mgr, spec, clean_winning_region);
        }
    }

    return !includes_init;
}

static bool compSolve2(Cudd* mgr, BDDAIG* spec) {
    typedef pair<BDD, set<unsigned>> subgame_info;
    vector<BDDAIG*> subgames = spec->decompose(settings.n_folds);
    if (subgames.size() == 0) return internalSolve(mgr, spec, NULL, NULL,
                                                   NULL, true);
    // Solving now the subgames
    BDD losing_transitions;
    int gamecount = 0;
    list<subgame_info> subgame_results;
    int total_bdd_size = 0;
    int total_cinp_size = 0;
    for (vector<BDDAIG*>::iterator i = subgames.begin();
         i != subgames.end(); i++) {
        gamecount++;
        dbgMsg("Solving subgame " + to_string(gamecount) + " (" +
               to_string((*i)->numLatches()) + " latches)");
        if (!internalSolve(mgr, *i, NULL, NULL, &losing_transitions))
            return false;
        vector<unsigned> cinput_vect = (*i)->getCInputLits();
        set<unsigned> cinput_set(cinput_vect.begin(), cinput_vect.end());
        subgame_results.push_back(subgame_info(losing_transitions, cinput_set));
        total_bdd_size += losing_transitions.nodeCount();
        total_cinp_size += cinput_set.size();
        // we have to release the memory used for the caches and stuff
        delete *i;
    }
    double mean_bdd_size = total_bdd_size / subgame_results.size();
    double mean_cinp_size = total_cinp_size / subgame_results.size();
    double cinp_factor = 0.5 * mean_bdd_size / mean_cinp_size;
    while (subgame_results.size() >= 2) {
        // Get the pair min_i,min_j that minimizes the score
        // The score is defined as: 
        // b.countNode() + cinp_factor * cinp_union.size()
        // where b is the disjunction of the error functions, and cinp_union
        // is the union of the cinputs of the subgames.
        int min_i, min_j; // the indices of the selected games
        list<subgame_info>::iterator min_it, min_jt; // iterators to selected games
        double best_score = DBL_MAX;  // the score of the selected pair
        BDD joint_err; // the disjunction of the error function of the pair
        set<unsigned> joint_cinp; // union of the cinp dependencies
        list<subgame_info>::iterator it, jt;
        int i, j;
        for (i = 0, it = subgame_results.begin(); 
             it != subgame_results.end(); i++, it++) {
            jt = it;
            jt++;
            j = i + 1;
            for (; jt != subgame_results.end(); j++, jt++) {
                BDD b = it->first | jt->first;
                set<unsigned> cinp_union;
                set_union(it->second.begin(), it->second.end(), 
                          jt->second.begin(), jt->second.end(),
                          inserter(cinp_union,cinp_union.begin()));
                double score = b.nodeCount() + cinp_factor * cinp_union.size();
                if (score < best_score) {
                    min_i = i;
                    min_j = j;
                    min_it = it;
                    min_jt = jt;
                    joint_err = b;
                    joint_cinp = cinp_union;
                    best_score = score;
                }
            }
        }
        dbgMsg("Selected subgames " + to_string(min_i) + " and " + to_string(min_j));
        set<unsigned> intersection;
        set_intersection(min_it->second.begin(), min_it->second.end(),
                         min_jt->second.begin(), min_jt->second.end(), 
                         inserter(intersection, intersection.begin()));
        BDD losing_transitions;
        if (intersection.size() == 0) {
            losing_transitions = joint_err;
        } else { 
            BDDAIG subgame(*spec, joint_err);
            if (!internalSolve(mgr, &subgame, NULL, NULL, &losing_transitions))
                return false;
        }
        subgame_results.erase(min_it);
        subgame_results.erase(min_jt);
        subgame_results.push_back(subgame_info(losing_transitions, joint_cinp));
    }

    assert(subgame_results.size() == 1);
    // Finally, we synth a circuit if required
    if (outputExpected()) {
        dbgMsg("acquiring lock on synth mutex");
        if (data != NULL) pthread_mutex_lock(&data->synth_mutex);
        dbgMsg("synthesis via comp 2");
        BDD clean_winning_region =
            (~subgame_results.back().first).Cofactor(~spec->errorStates());
        clean_winning_region = clean_winning_region
                                           .ExistAbstract(spec->cinputCube() &
                                                          spec->uinputCube());
        // let us clean the AIG before we start introducing new stuff
        spec->popErrorLatch();
        if (settings.out_file != NULL) {
            dbgMsg("Starting synthesis");
            synth_data.c_functions = synthAlgo(mgr, spec,
                                               ~subgame_results.back().first,
                                               mgr->bddOne());
        }
        // TODO: it seems that synthesizing and generating a winning region
        // at the same time is not possible with this buggy code!
        if (settings.win_region_out_file != NULL) {
            dbgMsg("Starting output of winning region");
            outputWinRegion(mgr, spec, clean_winning_region);
        }
        if (settings.ind_cert_out_file != NULL) {
            dbgMsg("Starting output of inductive certificate");
            outputIndCertificate(mgr, spec, clean_winning_region);
        }
    }
    return true;
}

static bool compSolve3(Cudd* mgr, BDDAIG* spec) {
    typedef pair<BDD, BDD> subgame_info;
    vector<BDDAIG*> subgames = spec->decompose(settings.n_folds);
    if (subgames.size() == 0) return internalSolve(mgr, spec, NULL, NULL, NULL,
                                                   true);
    // Solving now the subgames
    BDD losing_transitions;
    BDD losing_region;
    BDD global_lose = mgr->bddZero();
    vector<subgame_info> subgame_results;
    for (unsigned i = 0; i < subgames.size(); i++) {
        // check if another thread has won the race
        dbgMsg("Solving subgame " + to_string(i) + " (" +
               to_string(subgames[i]->numLatches()) + " latches)");
        if (!internalSolve(mgr, subgames[i], NULL, &losing_region,
                           &losing_transitions))
            return false;
        subgame_results.push_back(subgame_info(~losing_region, ~losing_transitions));
        global_lose |= losing_region;
        BDDAIG* old_sg = subgames[i];
        subgames[i] = new BDDAIG(*subgames[i], losing_transitions);
        delete old_sg;
    }

    addTime("decompose");
    dbgMsg("");
    dbgMsg("Now refining the aggregate game");

    BDD prev_lose = mgr->bddOne();
    BDD tmp_lose;
    BDD tmp_losing_trans;
    BDD global_losing_trans;
    int count = 1;
    BDD init_state = spec->initState();
    bool includes_init = ((init_state & global_lose) != ~mgr->bddOne());
    vector<unsigned> latches_u = spec->getLatchLits();

    while (!includes_init && prev_lose != global_lose) {
        prev_lose = global_lose;
        dbgMsg("Refinement iterate: " + to_string(count++));
        for (unsigned i = 0; i < subgames.size(); i++) {
            BDDAIG* subgame = subgames[i];
            subgame_info &sg_info = subgame_results[i];
            set<unsigned> latches_sg = spec->getBddLatchDeps(sg_info.second);
            set<unsigned> rem_latches;
            set_difference(latches_u.begin(), latches_u.end(), 
                           latches_sg.begin(), latches_sg.end(), 
                           inserter(rem_latches, rem_latches.begin()));
            BDD local_lose = global_lose.UnivAbstract(spec->toCube(rem_latches));
            BDD &local_win_over = sg_info.first;
            // if local_lose intersects local_win_over
            if ((local_lose & local_win_over) != ~mgr->bddOne()) {
                dbgMsg("\tActually refining subgame " + to_string(i));
                if (!internalSolve(mgr, subgame, &local_lose, &tmp_lose,
                                   &tmp_losing_trans))
                    return false;
                // subgames[i] = new BDDAIG(*subgame, tmp_losing_trans);
                // delete(subgame);
                sg_info.first = ~tmp_lose;
                sg_info.second = ~tmp_losing_trans;
                global_lose |= tmp_lose;
                if (global_lose == prev_lose){
                    //dbgMsg("\tLocal losing region didn't change");
                } else {
                    //dbgMsg("\n\tLocal losing region *DID* change");
                }
            }
        }
        addTime("localstep");
        if (global_lose == prev_lose) {
            dbgMsg("Global Upre");
            global_lose = global_lose | upre(spec, global_lose,
                                             global_losing_trans);
            addTime("globalstep");
        } else {
            dbgMsg("Skipping global Upre");
        }
        includes_init = ((init_state & global_lose) != ~mgr->bddOne());
    }
  
    // release memory of current subgames
    for (unsigned i = 0; i < subgames.size(); i++)
        delete subgames[i];

    // if !includes_init == true, then ~bad_transitions is the set of all
    // good transitions for controller (Eve)
    if (!includes_init && outputExpected()) {
        dbgMsg("acquiring lock on synth mutex");
        if (data != NULL) pthread_mutex_lock(&data->synth_mutex);
        BDD clean_winning_region = (~global_lose).Cofactor(~spec->errorStates());
        // let us clean the AIG before we start introducing new stuff
        spec->popErrorLatch();
        if (settings.out_file != NULL) {
            dbgMsg("Starting synthesis");
            synth_data.c_functions = synthAlgo(mgr, spec, ~global_losing_trans,
                                               ~global_lose);
        }
        if (settings.win_region_out_file != NULL) {
            dbgMsg("Starting output of winning region");
            outputWinRegion(mgr, spec, clean_winning_region);
        }
        if (settings.ind_cert_out_file != NULL) {
            dbgMsg("Starting output of inductive certificate");
            outputIndCertificate(mgr, spec, clean_winning_region);
        }
    }

    return !includes_init;
}

static bool compSolve4(Cudd* mgr, BDDAIG* spec) {
    typedef pair<BDD, BDD> subgame_info;
    vector<BDDAIG*> subgames = spec->decompose(settings.n_folds);
    if (subgames.size() == 0) return internalSolve(mgr, spec, NULL, NULL, NULL,
                                                   true);
    // Solving now the subgames
    BDD losing_transitions;
    BDD losing_region;
    BDD global_win_strats = mgr->bddOne();
    BDD global_lose = mgr->bddZero();
    BDD global_losing_trans;
    vector<subgame_info> subgame_results;
    for (unsigned i = 0; i < subgames.size(); i++) {
        // check if another thread has won the race
        dbgMsg("Solving subgame " + to_string(i) + " (" +
               to_string(subgames[i]->numLatches()) + " latches)");
        if (!internalSolve(mgr, subgames[i], NULL, &losing_region,
                           &losing_transitions))
            return false;
        subgame_results.push_back(subgame_info(~losing_region,
                                               (~losing_transitions &
                                                ~losing_region)));
        global_win_strats &= ~losing_transitions & ~losing_region;
        global_lose |= losing_region;
    }

    addTime("decompose");
    dbgMsg("");
    dbgMsg("Now refining the aggregate game");

    BDD tmp_losing_trans;
    BDD tmp_lose;
    int count = 1;
    vector<unsigned> latches_u = spec->getLatchLits();
    bool something_changed = true;
    BDD init_state = spec->initState();

    while (something_changed) {
        something_changed = false;
        dbgMsg("Refinement iterate: " + to_string(count++));
        for (unsigned i = 0; i < subgames.size(); i++) {
            BDDAIG* subgame = subgames[i];
            subgame_info &sg_info = subgame_results[i];
            BDD &winning_states = sg_info.first;
            BDD &winning_strats = sg_info.second;
            // if safe actions locally and globally do not coincide
            if ((winning_strats & global_win_strats) != winning_strats) {
                assert((~global_win_strats | winning_strats) == mgr->bddOne());
                dbgMsg("Local and global safe actions are not the same");
                set<unsigned> latches_sg = spec->getBddLatchDeps(sg_info.second);
                set<unsigned> rem_latches;
                set_difference(latches_u.begin(), latches_u.end(), 
                               latches_sg.begin(), latches_sg.end(), 
                               inserter(rem_latches, rem_latches.begin()));
                BDD joint_safe_trans = winning_strats & global_win_strats;
                BDD nu_local_win_strats =
                    joint_safe_trans.ExistAbstract(spec->toCube(rem_latches));
                
                if (nu_local_win_strats != winning_strats) {
                    dbgMsg("Even after getting rid of latches not present, "
                           "this is new info!");
                    assert((~nu_local_win_strats | ~winning_states) ==
                           ~nu_local_win_strats);
                    assert((~nu_local_win_strats | winning_strats) == mgr->bddOne());
                    // now we should solve a new game with the complement of
                    // nu_ocal_safe_trans as the error signal function
                    something_changed = true;
                    dbgMsg("\tActually refining subgame " + to_string(i));
                    BDDAIG* old_sg = subgame;
                    subgames[i] = new BDDAIG(*spec, ~nu_local_win_strats);
                    delete old_sg;
                    subgame = subgames[i];
                    BDD local_lose = ~winning_states;
                    if (!internalSolve(mgr, subgame, &local_lose, &tmp_lose,
                                       &tmp_losing_trans)) {
                        dbgMsg("RETURNED FALSE!");
                        return false;
                    }
                    // subgames[i] = new BDDAIG(*subgame, tmp_losing_trans);
                    // delete(subgame);
                    sg_info.first = ~tmp_lose;
                    sg_info.second = ~tmp_losing_trans & ~tmp_lose;
                    global_win_strats &= ~tmp_losing_trans & ~tmp_lose;
                    // if the winning strategy is no longer defined for the
                    // initial state we are already toasted
                    if ((init_state & global_win_strats) == mgr->bddZero()) {
                        dbgMsg("The global w. strategy is no longer defined "
                               "for the initial state");
                        return false;
                    }
                    global_lose |= tmp_lose;
                    if (winning_states == ~tmp_lose){
                        dbgMsg("\tLocal losing region didn't change");
                    } else {
                        dbgMsg("\n\tLocal losing region *DID* change");
                    }
                }
            }
        }
        addTime("localstep");
        if (!something_changed) {
            dbgMsg("Global Upre");
            BDD nu_global_lose = global_lose | upre(spec, global_lose,
                                                    global_losing_trans);
            global_win_strats = ~global_losing_trans & ~global_lose;
            addTime("globalstep");
            something_changed = (nu_global_lose != global_lose);
            global_lose = nu_global_lose;
        } else {
            dbgMsg("Skipping global Upre");
        }
        if ((init_state & global_lose) != ~mgr->bddOne()) {
            dbgMsg("The global step revealed we lose.");
            return false;
        }
    }
  
    // release memory of current subgames
    for (unsigned i = 0; i < subgames.size(); i++)
        delete subgames[i];

    // if !includes_init == true, then ~bad_transitions is the set of all
    // good transitions for controller (Eve)
    if (outputExpected()) {
        dbgMsg("acquiring lock on synth mutex");
        if (data != NULL) pthread_mutex_lock(&data->synth_mutex);
        BDD clean_winning_region = (~global_lose).Cofactor(~spec->errorStates());
        // let us clean the AIG before we start introducing new stuff
        spec->popErrorLatch();
        if (settings.out_file != NULL) {
            dbgMsg("Starting synthesis");
            synth_data.c_functions = synthAlgo(mgr, spec, global_win_strats,
                                               ~global_lose);
        }
        if (settings.win_region_out_file != NULL) {
            dbgMsg("Starting output of winning region");
            outputWinRegion(mgr, spec, clean_winning_region);
        }
        if (settings.ind_cert_out_file != NULL) {
            dbgMsg("Starting output of inductive certificate");
            outputIndCertificate(mgr, spec, clean_winning_region);
        }
    }

    return true;
}

bool solve(AIG* spec_base, Cudd_ReorderingType reordering) {
    Cudd mgr(0, 0);
    mgr.AutodynEnable(reordering);
    bool result;
    // we want spec to get garbage collected before we finalize
    // the synthesis step
    {
        BDDAIG spec(*spec_base, &mgr);
        if (settings.comp_algo == 1) {
                result = compSolve1(&mgr, &spec);
        } else if (settings.comp_algo == 2){
                result = compSolve2(&mgr, &spec);
        } else if (settings.comp_algo == 3){
                result = compSolve3(&mgr, &spec);
        } else if (settings.comp_algo == 4){
                result = compSolve4(&mgr, &spec);
        } else { // traditional fixpoint computation
            result = internalSolve(&mgr, &spec, NULL, NULL, NULL, true);
        }
    }
    // deal with the synthesis step if needed
    if (result && settings.out_file != NULL) {
        dbgMsg("Starting circuit generation");
        finalizeSynth(&mgr, spec_base);
    }
    return result;
}

static void pWorker(AIG* spec_base, int solver) {
    assert(data != NULL);
    bool result;
    dbgMsg("Calling parallel solver " + to_string(solver));
    switch (solver) {
        case 0:
            result = solve(spec_base);
            break;
        case 1:
            settings.use_abs = true;
            result = solve(spec_base);
            break;
        case 2:
            settings.use_abs = true;
            settings.abs_threshold = 2048;
            settings.comp_algo = 1;
            result = solve(spec_base);
            break;
        case 3:
            settings.use_abs = true;
            settings.abs_threshold = 2048;
            result = solve(spec_base);
            break;
        case 4:
            result = solve(spec_base, CUDD_REORDER_SIFT);
            break;
        case 5:
            result = solve(spec_base, CUDD_REORDER_WINDOW2);
            break;
        case 6:
            result = solve(spec_base, CUDD_REORDER_WINDOW3);
            break;
        case 7:
            result = solve(spec_base, CUDD_REORDER_WINDOW4);
            break;
        default:
            result = 0;
            errMsg("Unknown solver algo: " + to_string(solver));
    }
    dbgMsg("We have an answer from parallel solver " + to_string(solver));
    data->result = result;
    data->done = true;
    exit(0);
}

bool solveParallel() {
    bool ordering_strategies = settings.ordering_strategies;
    dbgMsg("Using ordering_strategies? " + to_string(ordering_strategies));
    // place our shared data in shared memory
    data = (shared_data*) mmap(NULL, sizeof(shared_data), PROT_READ | PROT_WRITE,
                               MAP_SHARED | MAP_ANON, -1, 0);
    assert(data);
    data->done = false;

    // initialize synth mutex
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&data->synth_mutex, &attr);

    // lets spawn all the required children
    pid_t children[4];
    int solver;
    for (int i = 0; i < 4; i++) {
        pid_t kiddo = fork();
        if (kiddo) {
            children[i] = kiddo;
        } else {
            solver = i;
            AIG spec(settings.spec_file);
	        if (ordering_strategies)
	            pWorker(&spec, solver + 4);
	        else
	            pWorker(&spec, solver);
        }
    }

    // the parent waits for one child to finish
    int status;
    pid_t kiddo;
    while (!data->done)
        kiddo = wait(&status);
    dbgMsg("Answer from process " + to_string(kiddo));
    dbgMsg("With status " + to_string(status));
    // then the parent kills all its children
    for (int i = 0; i < 4; i++)
        kill(children[i], SIGKILL);
    if (!data->done) {
        errMsg("Parallel solvers stopped unexpectedly. "
               "Synthesis/realizability test inconclusive", 55);
        // personal code for: "FUCK, children stopped unexpectedly"
    }
    // recover the answer
    bool result = data->result;

    // release shared memory
    munmap(data, sizeof(shared_data));
    return result;
}
