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
 * File:  RASolverNoIt.cc
 * Author:  mikolas
 * Created on:  Mon Nov 14 16:05:42 EST 2011
 * Copyright (C) 2011, Mikolas Janota
 */
#include <algorithm>
#include "RASolverNoIt.hh"
#include "RASolverIt.hh"
#include "utils.hh"
#include "Unit.hh"

RASolverNoIt::RASolverNoIt(size_t _max_id, const Fla& _fla, int unit_val,  size_t _indent/*=0*/)
  : abs_max_id(_max_id)
  , prep(_fla, *this, _indent)
  , fla(use_pure ? prep.preprocess() : _fla)
  , Q(fla.q)
  , unit(unit_val)
  , hybrid(0)
  , indent(_indent)
  , iteration_counter(0)
  
{
  assert(closed(fla));
  if (verbose) {
    out() << "No incr solver at level: " << (indent/INDENTATION) <<endl;
    out() << "unit: " << unit <<endl;
    if (verbose > 2) out() << fla << endl;
  } 

  FOR_EACH(vi,fla.pref) abs_max_id = std::max(abs_max_id,*vi);
}

bool RASolverNoIt::solve() {
  const double t0 = read_cpu_time();
  const bool   r = _solve();
  prep.reconstruct(_move, _rmove);
  const double d = read_cpu_time()-t0;
  if (verbose) {
    out() << "RES: " << r << endl;
    out() << "t: " << d << endl;
    if (fla.flas.size() && fla.flas[0].pref.size()) out() << "#iterations: " << iteration_counter << endl;
    if (verbose > 2) { 
      if(r) { 
        print(out() << "Move: ", fla.pref, _move) << endl;
        print(out() << "RMove: ", prep.get_original_formula().pref, _rmove) << endl;
      }
    }
  }
  return r;
}

bool RASolverNoIt::_solve() {
  if ( (Q==EXISTENTIAL && fla.op==OR) || (Q==UNIVERSAL && fla.op==AND) ) {
    assert(fla.flas.size()==0);
  }

  size_t depth = 0;
  FOR_EACH(index,fla.flas) depth=std::max(depth, index->pref.size());
  ++depth;
  if ( hybrid && (depth<=hybrid)) {
    RASolverIt inc(abs_max_id, fla.pref, mkAlts(Q, depth), fla.flas, false, indent);
    const bool win = inc.solve();
    if (win) copy_values(_move, inc.move(), fla.pref);
    return win;
  }

  const size_t subformula_count=fla.flas.size();

  // identify subformulas with no quantifiers
  size_t leaf_count = 0;
  RANGE (index, 0, subformula_count) {
    if (fla.flas[index].pref.empty()) ++leaf_count;
  }

  if (subformula_count==leaf_count) return sat();

  Fla abs;
  abs.pref = fla.pref;
  abs.q    = Q;
  abs.op   = (Q==UNIVERSAL ? OR : AND);
  if (leaf_count) {
    FOR_EACH (index,fla.flas) {
      const QFla& f = *index;
      if (f.pref.empty()) abs.flas.push_back(f);
    }
  }

  iteration_counter = 0;
  while (true) {
    // play abstract game 
    if (verbose>2) out() << "abs"  << endl;
    RASolverNoIt abs_solver(abs_max_id, abs, unit, indent+INDENTATION);
    abs_solver.set_hybrid(hybrid);
    const bool c = abs_solver.solve();
    if (!c) return false;
    vec<lbool> abs_move;
    copy_values (abs_move, abs_solver.move(), fla.pref); //TODO is this needed?

    // let opponents play
    if (verbose>2) out() << "ops"  << endl;
    // -- create game for the opponent
    vec<lbool> opponent_move;
    size_t winning_opponent = -1;
    VarSet used_values;
    const bool opponent_win=play_opponents(abs_move, opponent_move, used_values, winning_opponent);
    if (!opponent_win) {
      _move.clear();
      abs_move.copyTo(_move);
      return true;
    }
    // refine
    if (verbose>2) out() << "refine"  << endl;
    if (use_blocking) {
      block(abs_move, used_values, abs);
    }
    const bool refined = unit ? refine_unit(opponent_move, fla.flas[winning_opponent], abs) 
                              : refine(opponent_move, fla.flas[winning_opponent], abs);
    if (!refined) {
      return false;
    }
    ++iteration_counter;
  }
}


bool RASolverNoIt::play_opponents(const vec<lbool>& abs_move, vec<lbool>& opponent_move, VarSet& used_values, size_t& winning_opponent) {
  winning_opponent=-1;
  for (size_t ri=0; ri<fla.flas.size(); ++ri) {
    winning_opponent=fla.flas.size()-ri-1;
    if (fla.flas[winning_opponent].pref.empty()) continue;
    Fla oponnent_game;
    used_values.clear();
    build_oponnent_game(fla.flas[winning_opponent], abs_move, used_values, oponnent_game);
    RASolverNoIt opponent_solver(abs_max_id, oponnent_game, unit, indent+2);
    opponent_solver.set_hybrid(hybrid);
    const bool o = opponent_solver.solve();
    if (o) {
      const VarVector& ovs = fla.flas[winning_opponent].pref[0].second;
      copy_values(opponent_move, opponent_solver.move(), ovs);
      return true; 
    }
  }
  return false;
}

void RASolverNoIt::block(const vec<lbool>& abs_move, const VarSet& used_values, Fla& abs) {
  if (verbose>1) out() << "brd: "<<abs.pref.size()<<"->"<<used_values.size()<<" , "<<((float)used_values.size() / (float)abs.pref.size()) << endl;
  abs.flas.resize(abs.flas.size()+1);
  QFla& block = abs.flas.back();
  if (Q==EXISTENTIAL) {
    vector<Lit> ls;
    FOR_EACH(vi, used_values) {
      const Var v = *vi;
      assert (v < abs_move.size());
      const lbool val = abs_move[v];
      assert (val != l_Undef);
      ls.push_back(val == l_True? ~mkLit(v) : mkLit(v));
    }
    block.cnf.push_back(LitSet(ls));
  } else {
    assert(Q==UNIVERSAL);
    vector<Lit> ls(1);
    FOR_EACH(vi, used_values) {
      const Var v = *vi;
      assert (v < abs_move.size());
      const lbool val = abs_move[v];
      assert (val != l_Undef);
      ls[0] = val == l_True ? mkLit(v) : ~mkLit(v);
      block.cnf.push_back(LitSet(ls));
    }
  }
}

void RASolverNoIt::build_oponnent_game(const QFla& qfla, const vec<lbool>& cand_move, VarSet& used_values, Fla& oponnent_game) {
  assert(qfla.pref.size()>0);
  oponnent_game.q = qfla.pref[0].first;
  oponnent_game.pref = qfla.pref[0].second;
  oponnent_game.op = (qfla.pref[0].first==EXISTENTIAL) ? AND : OR;
  oponnent_game.flas.resize(1);
  QFla& meat=oponnent_game.flas[0];
  for (size_t i=1; i< qfla.pref.size(); ++i) {
    meat.pref.push_back(qfla.pref[i]);
  }
  if (unit>1) {
    Unit u(qfla.cnf,cand_move,used_values);
    const bool p=u.propagate();
    u.eval(meat.cnf);
    if (p) FOR_EACH (qi, qfla.pref) fix_unit(u, qi->second, meat.cnf);
    if (verbose>4) out() << "up: " << meat.cnf << endl;
  } else {
    subst(qfla.cnf,cand_move,used_values,meat.cnf);
  }
}

Var RASolverNoIt::new_abstraction_var() { return ++abs_max_id; }

bool RASolverNoIt::refine(const vec<lbool>& move, const QFla& winning_fla, Fla& abs) {
  assert(!winning_fla.pref.empty());
  // alloc new vars for first block in winning_fla in abstr
  vec<Var> oldToPrime;
  if (winning_fla.pref.size()>1) {
    VariableVector new_abstraction_variables;
    FOR_EACH (vi, abs.pref) new_abstraction_variables.push_back(*vi);
    FOR_EACH (vari, winning_fla.pref[1].second) {
      const Var var = *vari;
      assert (var>0);
      if (oldToPrime.size()<=var) oldToPrime.growTo(var+1,0);
      const Var pv = new_abstraction_var();
      oldToPrime[var] = pv;
      new_abstraction_variables.push_back(pv);
    }
    abs.pref = VarVector(new_abstraction_variables);
  }

  vector<Lit> ls;
  abs.flas.resize(abs.flas.size()+1);
  QFla& disjunct = abs.flas[abs.flas.size()-1];
  for (size_t i=2; i < winning_fla.pref.size(); i++) disjunct.pref.push_back(winning_fla.pref[i]);

  FOR_EACH(cli,winning_fla.cnf) {
    LitSet cl = *cli; 
    ls.clear();
    bool taut_cl=false;
    bool change=false;
    FOR_EACH(li,cl) {
      const Lit& l = *li;
      const Var  v = var(l);
      const bool lsign = sign(l);
      if (v >= move.size() || move[v]==l_Undef) {
        if (v >= oldToPrime.size() || oldToPrime[v]==0) ls.push_back(l);  // no change
        else {
          ls.push_back(mkLit(oldToPrime[v],lsign));                       // primed variable
          change=true;
        }
      } else { 
        // substituted variable
        if (lsign == (move[v]==l_False)) {
          taut_cl=true;
          break;
        }
        change=true; // omit lit
      }
      }
    if (taut_cl) continue;
    if (change) disjunct.cnf.push_back(LitSet(ls));
    else disjunct.cnf.push_back(cl);
  }
  if (verbose>3) {
    out() << "Refined with:"  << endl;
    out() << disjunct  << endl;
  }
  return true;
}

bool RASolverNoIt::refine_unit(const vec<lbool>& move, const QFla& winning_fla, Fla& abs) {
  abs.flas.resize(abs.flas.size()+1);
  QFla& disjunct = abs.flas.back();
  CNF  temp;
  CNF& cnf = (winning_fla.pref.size()>1) ? temp : disjunct.cnf; // use temporary when freshening is needed at the end

  // perform unit propagation with the given move
  Unit u(winning_fla.cnf,move);
  bool ok=true;
  ok &= u.propagate();
  if (!ok) {
    disjunct.cnf.push_back(LitSet());
    goto FINISH;
  }
  u.eval(cnf);
  if (verbose>4) {
     out() << "unit propagation result: " << cnf << endl;
     ostream& o = out();
     for (Var v=1; v < u.size(); ++v) if (u.value(v)!=l_Undef) {
         o  << mkLit(v,  u.value(v)==l_False) << " ";
     }
     o << endl;
  }

  // add unit clauses
  if (abs.q==UNIVERSAL) fix_unit(u, abs.pref, cnf);

  for (size_t i=1; i < winning_fla.pref.size(); i++) {
    if (winning_fla.pref[i].first==UNIVERSAL) {
      if (any_fixed(u, winning_fla.pref[i].second)) {
        cnf.clear(); 
        fix_unit(u, winning_fla.pref[i].second, cnf);
        break;
      }} else {
      fix_unit(u, winning_fla.pref[i].second, cnf);
    }
  }

  
  if (winning_fla.pref.size()>1) {// freshen vars for first block in winning_fla
    VarSet      q1(winning_fla.pref[1].second);
    vec<Var>    oldToPrime;
    freshen(cnf, q1, disjunct.cnf, oldToPrime, *this); 
    // insert fresh variables into the prefix of the abstraction
    VariableVector new_abstraction_variables;
    for (int i=0; i<oldToPrime.size(); ++i) { 
      const Var v = oldToPrime[i];
      if (v!=0) new_abstraction_variables.push_back(v);
    }
    if (new_abstraction_variables.size()) {
      FOR_EACH (vi, abs.pref) new_abstraction_variables.push_back(*vi);
      abs.pref = VarVector(new_abstraction_variables);
    }
  }

 FINISH:

  if (!ok) {
    if (!ok && verbose>3) out() << "refinement false by unit propagation"  << endl;
  }

  for (size_t i=2; i < winning_fla.pref.size(); i++) {
    disjunct.pref.push_back(winning_fla.pref[i]);
  }

  if (verbose>3) {
    out() << "Refined with:"  << endl;
    out() << disjunct  << endl;
  }

  if (ok && abs.q==EXISTENTIAL && any_fixed(u, abs.pref)) {
     assert(abs.op==AND);
     abs.flas.resize(abs.flas.size()+1);
     fix_unit(u, abs.pref, abs.flas.back().cnf);
  }
  return true;
}

bool RASolverNoIt::sat() {
  if ( (Q==EXISTENTIAL && fla.op==OR) || (Q==UNIVERSAL && fla.op==AND) ) {
    assert(fla.flas.size()==0);
  }

  MiniSatExt s;
  //TODO: unnecessary
  for (size_t fi=0; fi<fla.flas.size(); ++fi) {
    const CNF& cnf=fla.flas[fi].cnf;
    FOR_EACH(cli,cnf) {
      FOR_EACH(li,*cli) {
        const Var v = var(*li);
        s.new_variables(v);
      }
    }
  }

  bool ok = true;
  if (Q==EXISTENTIAL) {
    vec<Lit> ls;
    for (size_t fi=0; fi<fla.flas.size(); ++fi) {
      const CNF& cnf=fla.flas[fi].cnf;
      FOR_EACH(cli,cnf) {
        ls.clear();
        FOR_EACH(li,*cli) ls.push(*li);
        ok &= s.addClause_(ls);
      }
    }
  } else {
    LitSet2Lit tseitin;
    for (size_t fi=0; fi<fla.flas.size(); ++fi) {
      vec<Lit> ts;
      const CNF& cnf=fla.flas[fi].cnf;
      FOR_EACH(cli,cnf) {
        const LitSet& cl=*cli;
        const LitSet2Lit::const_iterator tsi=tseitin.find(cl);
        if (tsi!=tseitin.end()) {
          ts.push(tsi->second);
        } else {
          Lit tsl;
          if (cl.size()==1) tsl=~cl[0];
          else {
            const Var tv = s.newVar();
            tsl= mkLit(tv);
            FOR_EACH(li,cl) ok&=s.addClause( ~tsl, ~(*li) );
          }
          tseitin[cl]=tsl;
          ts.push(tsl);
        }
      }
      ok&=s.addClause(ts);
    }
  }

  if (!ok) return false;
  const bool r = s.solve();
  if (!r) return false;
  _move.clear();
  FOR_EACH(vi,fla.pref) {
    const Var v = *vi;
    if (v>=_move.size())  _move.growTo(v+1,l_Undef);
    _move[v]= ((v < s.model.size()) && (s.model[v]==l_True)) ? l_True : l_False;
  }
  return true;
}

Var RASolverNoIt::new_var() {
  return new_abstraction_var();
}

bool RASolverNoIt::set_hybrid(size_t hybrid_val)  {
  if (verbose>1) out() << "hybrid: " << hybrid_val <<endl;
  return hybrid=hybrid_val;
}
