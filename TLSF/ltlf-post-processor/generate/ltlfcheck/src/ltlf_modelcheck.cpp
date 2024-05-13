//
// Created by philipp on 4/30/24.
//

#include <array>

#include "ltlf_modelcheck.h"

sym_check_status prepare_negspec(const twa_graph_ptr& aut,
                                 const std::string& seqdone)
{

  if (!aut)
    throw std::runtime_error("prepare_negspec(): aut is empty!\n");

  auto& autr = *aut;

  if (autr.acc().is_t())
  {
    std::cerr << "prepare_negspec(): Negation of the specification is a "
              << "monitor, CTRL should be UNSAT.\n";
    return sym_check_status::NOK;
  }

  if (!autr.is_sba()
      || (!autr.acc().is_t() && !autr.acc().is_buchi()))
  {
    std::cerr << "prepare_negspec(): Automaton needs to have state-based "
              << "acceptance and has to be either a monitor or "
              << "a buechi automaton.";
    return sym_check_status::STRUCTURALERROR;
  }

  bdd sd[2] = {bdd_nithvar(autr.register_ap(seqdone)), bddfalse};
  sd[1] = bdd_not(sd[0]);

  // Add seqdone to the controlled props
  set_synthesis_outputs(aut,
                        get_synthesis_outputs(aut)&sd[1]);


  // We duplicate the accepting states
  // We can only go to accepting states if we terminate
  // todo improve
  // acc -> non acc duplicate
  auto acc_state = std::unordered_map<unsigned, unsigned>();
  const unsigned No = autr.num_states();
  for (unsigned s = 0; s < No; ++s)
    if (autr.state_is_accepting(s))
      acc_state[s] = autr.new_state();
  // Edges that need duplication
  // 0 -> replace src
  // 1 -> replace dst
  // 2 -> replace both
  auto used_e = std::array<std::vector<unsigned>, 3>{};
  for (auto& e: autr.edges())
  {
    auto accsrc = autr.state_is_accepting(e.src);
    auto accdst = autr.state_is_accepting(e.dst);
    if (accsrc && accdst)
      used_e[2].push_back(autr.edge_number(e));
    else if (accsrc)
      used_e[0].push_back(autr.edge_number(e));
    else if (accdst)
      used_e[1].push_back(autr.edge_number(e));
  }
  // Now modify
  for (unsigned eidx: used_e[0])
  {
    // replace src
    const auto& eo = autr.edge_storage(eidx);
    autr.new_edge(acc_state[eo.src], eo.dst, eo.cond);
  }
  for (unsigned eidx: used_e[1])
  {
    // replace dst, modify cond
    auto eo = autr.edge_storage(eidx);
    autr.new_edge(eo.src, acc_state[eo.dst], eo.cond & sd[0]);
    // The original edge is terminating
    autr.edge_storage(eidx).cond &= sd[1];
  }
  for (unsigned eidx: used_e[2])
  {
    // replace src and dst, modify cond
    auto eo = autr.edge_storage(eidx);
    autr.new_edge(acc_state[eo.src], acc_state[eo.dst], eo.cond & sd[0]);
    // We can also hop between them if it is a self-loop
    if (eo.src == eo.dst)
    {
      autr.new_edge(eo.src, acc_state[eo.dst], eo.cond & sd[0]);
      autr.new_edge(acc_state[eo.src], eo.dst, eo.cond & sd[1], {0});
    }
    // The original edge is terminating
    autr.edge_storage(eidx).cond &= sd[1];
  }
  autr.prop_state_acc(trival::maybe());
  return sym_check_status::OK;
}

sym_check_status sanity_check(const sym_aut& negspec,
                              const sym_aut& ctrl)
{
  auto errstr = std::stringstream();

  if (negspec.P != ctrl.P)
    errstr << "negspec and ctrl do not agree on propositions!\n"
           << "negspec: " << negspec.P << '\n'
           << "ctrl   : " << ctrl.P << '\n';
  if (negspec.in != ctrl.in)
    errstr << "negspec and ctrl do not agree on input propositions!\n"
           << "negspec: " << negspec.in << '\n'
           << "ctrl   : " << ctrl.in << '\n';
  if (negspec.out != ctrl.out)
    errstr << "negspec and ctrl do not agree on output propositions!\n"
           << "negspec: " << negspec.out << '\n'
           << "ctrl   : " << ctrl.out << '\n';
  if (ctrl.I == bddfalse)
    errstr << "Controller has no valid initialization!\n";

  if (auto err = errstr.str(); !err.empty())
  {
    std::cerr << err;
    return sym_check_status::STRUCTURALERROR;
  }
  else
    return sym_check_status::OK;
}

sym_check_status symbolic_check(const sym_aut& negspec,
                                const sym_aut& ctrl)
{
  // Product transition system
  bdd T = negspec.T & ctrl.T;
  // All state variables
  bdd X = negspec.X & ctrl.X;
  bdd Xp = negspec.Xp & ctrl.Xp;
  // Current state (unprimed)
  bdd C = negspec.I & ctrl.I;
  // Joint final states
  bdd F = negspec.F & ctrl.F;
  // Non-terminal states for controller
  bdd NFc = bdd_not(ctrl.F);
  // Ensure loop-free
  // What is the actual bound here??
  // Assume states of the prod aut
  const unsigned nmax
      = (std::pow(2, ctrl.Xvec.size()) + 1u)
        * (std::pow(2, negspec.Xvec.size()) + 1u);

  auto Xp2X = [&](const bdd& fXp)
  {
    bdd fX = bdd_replace(fXp, negspec.Xp2X.get());
    fX = bdd_replace(fX, ctrl.Xp2X.get());
    return fX;
  };

  unsigned i = 0;
  auto ret = [&i](sym_check_status s) -> sym_check_status
  {
    my_out << "MC took " << i << " iterations.\n";
    return s;
  };
  for (; ; ++i)
  {
    // Check if guaranteed loop-free
    if (i > nmax)
      return ret(sym_check_status::NOTERMINATION);
    // Take the transition
    bdd Cp = T & C;
    // Project onto primed states
    Cp = bdd_existcomp(Cp, Xp);
    // Map to unprimed
    C = Xp2X(Cp);
    // Check if violating
    if (bdd_have_common_assignment(C, F))
      return ret(sym_check_status::NOK);
    // Restrict to non-terminated sequences
    C &= NFc;
    if (C == bddfalse)
      return ret(sym_check_status::OK);
    // Done for this iteration
  }
  SPOT_UNREACHABLE(); // We always terminate in the loop
}

std::ostream&
operator<<(std::ostream& out, const sym_check_status value){
  static const auto scs2str
      = std::map<int, std::string>{
          {-2, "STRUCTURALERROR"},
          {-1, "NOTERMINATION"},
          {0, "NOK"},
          {1, "OK"},
      };

  return out << scs2str.at((int)value);
}
