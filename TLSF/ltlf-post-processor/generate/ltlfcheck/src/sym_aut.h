//
// Created by philipp on 24/07/23.
//

#pragma once

#include <algorithm>
#include <variant>

#include "twaalgos/aiger.hh"
#include "twa/twagraph.hh"
#include "twaalgos/synthesis.hh"

#include "misc/bddlt.hh"

#include "utils.h"

using namespace spot;

namespace internal_ltlf
{
  /**
   * \brief Helper to use smart_pointers with bddPairs
   */
  struct pair_deleter
  {
    void operator()(bddPair* p) const
    {
      if (p) bdd_freepair(p);
    }
  };

  /**
   * \brief Safeguard around bdd_setpair
   * \param p The bddPair
   * \param oldv
   * \param newv
   */
  inline void safe_bdd_setpair(bddPair* p, int oldv, int newv)
  {
    if (bdd_setpair(p, oldv, newv) != 0)
      throw std::runtime_error("bdd_setpair(): Failed.");
  }
}

/**
 * \brief Structure containing the definition of the symbolic automaton
 *
 * The primed and unprimed state variables are stored in consecutive blocks
 */
struct sym_aut
{

  using bddpair_ptr
      = std::unique_ptr<bddPair, internal_ltlf::pair_deleter>;

  bdd T = bddfalse;  // Transition system (X', X, AP)
  bdd F = bddfalse;  // Final states (X)
  bdd S = bddfalse;  // All states (X)
  bdd I = bddfalse;  // Initial state (X)
  bdd X = bddfalse;  // All X vars
  bdd Xp = bddfalse;  // All X' vars
  bdd P = bddfalse;  // All AP
  bddpair_ptr X2Xp = bddpair_ptr{}; // Replace X by X'
  bddpair_ptr Xp2X = bddpair_ptr{}; // Replace X' by X
  // Access to single APs
  std::vector<std::array<bdd, 2>> Xvec; // ~x, x in X
  std::vector<std::array<bdd, 2>> Xpvec; // ~x, x in Xp

  // Currently, a symbolic automata can represent either
  // a twa_graph or an aiger
  std::variant<twa_graph_ptr, aig_ptr> orig_;

  bdd in = bddfalse; // Propositions considered as input
  bdd out = bddfalse; // Propositions considered as output

  /**
   * \brief Setup \a Nvars new propositions with basename \a base
   * @param Nvars Number of propositions
   */
  void
  setup(unsigned Nvars, const std::string& base);

  /**
   * \brief Encode the state in binary over X or X'
   * (depending on \a useX)
   * @param s state number to encode
   * @param useX Whether to encode over X or X'
   * @return
   */
  inline bdd
  encode_state(unsigned s, bool useX = true) const
  {
    const unsigned Nvars = Xvec.size();

    const auto& XvecR = useX ? Xvec : Xpvec;

    unsigned idx = 0;
    bdd ret = bddtrue;

    for (; s; ++idx)
    {
      ret &= XvecR[idx][s & 1u];
      s >>= 1;
    }

    assert(idx <= Nvars);

    for (; idx < Nvars; ++idx)
      ret &= XvecR[idx][0];
    return ret;
  };

  /**
   * \brief Translate the symbolic automata into
   * an explicit (semi-symbolic) automaton
   * \return The corresponding automaton and possibly the
   * associated split version
   * \note Currently not implemented
   */
  std::pair<twa_graph_ptr, twa_graph_ptr>
  to_explicit() const {throw std::runtime_error("TBD"); };

  /**
   * \brief Convert an explicit (semi-symbolic) automaton into
   * a (fully) symbolic one
   * \param g The automaton to convert
   * \param base Prefix of the variables used for state encoding
   * \param termsig If termsig is given, the automaton is interpreted
   * as a nfa, all states which have incoming transitions labeled by
   * termsig are terminal
   * \return The corresponding symbolic one
   */
  static sym_aut
  to_symbolic(const twa_graph_ptr& g,
              const std::string& base,
              const std::string& termsig = "");

  /**
   * \brief Convert an aig instance into a fully symbolic automaton
   * \param aig The aig circuit to convert
   * \param base Prefix of the variables used for state encoding
   * \param termsig If given terminating semantics are used
   * \return The corresponding symbolic automaton
   * \note If terminating semantics is used, the the final states correspond
   * to the terminating states. Behaviour is undefined when asking
   * for the output or next_states of a terminal state. If non-terminating
   * semantics is used, final states are set to false.
   * \note If symbolic automaton is derived from a aig, then all states
   * has no meaning and will be set to false
   */
  static sym_aut
  to_symbolic(const aig_ptr& aig,
              const std::string& base,
              const std::string& termsig = "");

  friend std::ostream& operator<<(std::ostream& os, const sym_aut& sa);
};