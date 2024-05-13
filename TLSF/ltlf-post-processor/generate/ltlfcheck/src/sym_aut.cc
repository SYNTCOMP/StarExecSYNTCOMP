//
// Created by philipp on 5/6/24.
//
#include "sym_aut.h"

void sym_aut::setup(unsigned Nvars, const std::string& base)
{
  X = bddtrue;
  Xp = bddtrue;
  T = bddfalse;
  F = bddfalse;
  S = bddfalse;
  P = bddfalse;
  X2Xp.reset(bdd_newpair());
  Xp2X.reset(bdd_newpair());

  // If there is only one state / no latches
  // we create a single "dummy" variable
  Nvars = std::max(1u, Nvars);

  Xvec = std::vector<std::array<bdd, 2>>(Nvars);
  Xpvec = std::vector<std::array<bdd, 2>>(Nvars);

  int X0 = -1;
  int Xp0 = -1;

  {
  auto vars_v = std::vector<formula>(Nvars);
  auto vars_vp = std::vector<formula>(Nvars);

  for (unsigned i = 0; i < Nvars; ++i)
  {
    vars_v[i] = formula::ap(base+'_'+std::to_string(i));
    vars_vp[i] = formula::ap(base+"_p_"+std::to_string(i));
  }
  if (auto* aut = std::get_if<twa_graph_ptr>(&orig_))
  {
    X0 = (*aut)->get_dict()->register_propositions_as_block(vars_v, *aut);
    Xp0 = (*aut)->get_dict()->register_propositions_as_block(vars_vp, *aut);
  }
  else if (auto* aig = std::get_if<aig_ptr>(&orig_))
  {
    X0 = (*aig)->get_dict()->register_propositions_as_block(vars_v, *aig);
    Xp0 = (*aig)->get_dict()->register_propositions_as_block(vars_vp, *aig);
  }
  else
    SPOT_UNREACHABLE();
  }

  if (X0 == -1 || Xp0 == -1)
    throw std::runtime_error("sym_aut::setup(): Failed to alloc vars.");

  // Setup all the vectors
  for (unsigned i = 0; i < Nvars; ++i)
  {
    auto xi = X0 + i;
    auto xpi = Xp0 + i;
    auto x = bdd_ithvar(xi);
    auto xp = bdd_ithvar(xpi);
    Xvec[i] = {bdd_not(x), x};
    Xpvec[i] = {bdd_not(xp), xp};

    X &= x;
    Xp &= xp;

    internal_ltlf::safe_bdd_setpair(X2Xp.get(), xi, xpi);
    internal_ltlf::safe_bdd_setpair(Xp2X.get(), xpi, xi);
  }
}

sym_aut
sym_aut::to_symbolic(const twa_graph_ptr& g,
                     const std::string& base,
                     const std::string& termsig)
{
  const unsigned Nstates = g->num_states();
  const unsigned Nvars = nbits(Nstates);

  my_out << Nvars << " variables used for " << Nstates << " states.\n";

  bool is_finite = !termsig.empty();

  auto sy = sym_aut();
  sy.orig_ = g;

  // Setup
  sy.setup(Nvars, base);

  auto s2X = std::vector<std::array<bdd, 2>>(Nstates);

  for (unsigned s = 0; s < Nstates; ++s)
  {
    auto x = sy.encode_state(s);
    auto xp = bdd_replace(x, sy.X2Xp.get());
    s2X[s] = {x, xp};
    sy.S |= x;
    if (!is_finite && g->state_is_accepting(s))
      sy.F |= x;
  }
  sy.I = s2X[g->get_init_state_number()][0];

  // Encode the transitions
  for (const auto& e: g->edges())
  {
    const bdd& sx = s2X[e.src][0];
    const bdd& dxp = s2X[e.dst][1];
    bdd c = e.cond & sx & dxp;
    sy.T |= c;
  }

  // Deduce terminal state in the finite case
  if (is_finite)
  {
    bdd termbdd
        = bdd_ithvar(g->register_ap(termsig));
    // this also takes into account all states which work for
    // termsig maybe
    bdd fstatesp = sy.T & termbdd;
    fstatesp = bdd_existcomp(fstatesp, sy.Xp); // restrict to prime
    fstatesp = bdd_replace(fstatesp, sy.Xp2X.get()); // replace with non-prime
    // so get rid of them
    bdd fstatesm = sy.T & bdd_not(termbdd);
    fstatesm = bdd_existcomp(fstatesm, sy.Xp); // restrict to prime
    fstatesm = bdd_replace(fstatesm, sy.Xp2X.get()); // replace with non-prime

    bdd fstates = fstatesp - fstatesm;

    // Terminal states
    sy.F = fstates;
    // We do not allow the empty word
    if (bdd_have_common_assignment(sy.F, sy.I))
    {
      std::cerr << "Initial states:\n" << sy.I << '\n';
      std::cerr << "Final states:\n" << sy.F << '\n';
      std::cerr << "Warning, initial state is also final\n";
      //throw std::runtime_error("sym_aut::to_symbolic(twa_graph_ptr): "
      //                         "Initial and Final states overlap "
      //                         "but the empty word is forbidden.\n");
    }
  }

  // Set additional info if possible
  sy.P = g->ap_vars();
  if (auto outptr = g->get_named_prop<bdd>("synthesis-outputs"))
  {
    sy.out = *outptr;
    sy.in = bdd_exist(sy.P, sy.out);
  }

  return sy;
}

sym_aut
sym_aut::to_symbolic(const aig_ptr& aig,
                     const std::string& base,
                     const std::string& termsig)
{
  const unsigned Nlatches = aig->num_latches();
  const unsigned Nout = aig->num_outputs();
  const unsigned Nin = aig->num_inputs();

  auto sy = sym_aut();

  sy.orig_ = aig;

  // Setup
  // Latches act as states
  sy.setup(Nlatches, base);

  // Current (unprimed) values for latches already
  // have a bdd associate -> we need to swap them against X variables to
  // be contiguous

  bddpair_ptr l2X;
  l2X.reset(bdd_newpair());
  for (unsigned li = 0; li < Nlatches; ++li)
  {
    internal_ltlf::safe_bdd_setpair(l2X.get(),
                                    bdd_var(aig->latch_bdd(li)),
                                    bdd_var(sy.Xvec[li][1]));
  }

  // Initially, all latches are set to false
  sy.I = bddtrue;
  for (const auto& xp : sy.Xvec)
    sy.I &= xp[0];
  // all_states has no meaning in this context
  sy.S = bddfalse;

  // Build the transition system over
  // X, Xp and P
  // the transition system is the conjunction over all latch functions
  sy.T = bddtrue;
  for (unsigned li = 0; li < Nlatches; ++li)
  {
    // The function for the ith
    bdd fi = aig->aigvar2bdd(aig->next_latches()[li]);
    // Swap latches -> X
    fi = bdd_replace(fi, l2X.get());
    // ith next latch is true if and only if fi is true
    sy.T &= (sy.Xpvec[li][1] & fi) | (sy.Xpvec[li][0] & bdd_not(fi));
  }
  // This constructed the state transitions
  // so T is now a function over input, X and Xp
  // Now we do the same for outputs
  // We compute all outs at the same time
  sy.out = bddtrue;
  for (unsigned lo = 0; lo < Nout; ++lo)
  {
    // The function for the ith output
    bdd fo = aig->aigvar2bdd(aig->output(lo));
    // Swap latches -> X
    fo = bdd_replace(fo, l2X.get());
    // The aig does not need outputs in bdd form,
    // so we have to get them first
    bdd obdd
        = bdd_ithvar(aig->get_dict()->register_proposition(
            formula::ap(aig->output_names()[lo]), aig));
    sy.out &= obdd;
    // ith output is true if and only if fo is true
    sy.T &= (obdd & fo) | (bdd_not(obdd) & bdd_not(fo));
  }

  // Determine the final states/terminal
  // Terminal states all those that can be reached via
  // a transition with termsig high
  if (!termsig.empty())
  {
    if (!std::count(aig->output_names().cbegin(),
                    aig->output_names().cend(),
                    termsig))
      throw std::runtime_error("sym_aut::to_symbolic(aig_ptr): "
                               "Given termsig not found in outputs.\n");

    bdd termbdd
        = bdd_ithvar(aig->get_dict()->register_proposition(
            formula::ap(termsig), aig));

    bdd fstates = sy.T & termbdd;
    // Existence away everything except primed states
    fstates = bdd_existcomp(fstates, sy.Xp);
    // Terminal states
    sy.F = bdd_replace(fstates, sy.Xp2X.get());
    // We do not allow the empty word
    if (bdd_have_common_assignment(sy.F, sy.I))
    {
      std::cerr << "Initial states:\n" << sy.I << '\n';
      std::cerr << "Final states:\n" << sy.F << '\n';
      std::cerr << "Warning, initial state is also final\n";
      //throw std::runtime_error("sym_aut::to_symbolic(aig_ptr): "
      //                         "Initial and Final states overlap "
      //                         "but the empty word is forbidden.\n");
    }
  }

  // Finally compute the inputs
  sy.in = bddtrue;
  for (unsigned li = 0; li < Nin; ++li)
    sy.in &= aig->input_bdd(li);
  // And all the APs used
  sy.P = sy.in & sy.out;
  return sy;
  // Done
}

std::ostream& operator<<(std::ostream& os, const sym_aut& sa)
{
  os << "All states:\n " << sa.S << '\n'
     << "Initial state:\n " << sa.I << '\n'
     << "Final states:\n " << sa.F << '\n'
     << "Transitions:\n " << sa.T << '\n'
     << "All X:\n " << sa.X << '\n'
     << "All X prime:\n " << sa.Xp << '\n'
     << "All AP:\n " << sa.P << '\n';
  return os;
}
