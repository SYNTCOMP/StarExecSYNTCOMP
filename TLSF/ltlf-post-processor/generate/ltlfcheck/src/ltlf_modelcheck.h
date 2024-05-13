//
// Created by philipp on 4/30/24.
//

#pragma once

#include "sym_aut.h"
#include <map>

/**
 * \brief Enum to indicate the different model checking outcomes.
 */
enum class sym_check_status{
  STRUCTURALERROR = -2,  //Structural error of the controller:
  // No initial state, wrong atomic properties
  NOTERMINATION = -1,  // Environment can force to loop
  NOK = 0,  // A violation was found
  OK = 1  // The given controller implements the spec
};

std::ostream&
operator<<(std::ostream& out, const sym_check_status value);

/**
 * \brief Prepares an automaton representing the negation of the spec
 * to be converted into a symbolic version.
 * In particular, this means that accepting states are duplicated.
 * All transitions leading to the accepting states must imply
 * the special seqdone AP to be True (If the controller decides to
 * terminate, the language is in the complement of the spec -> violation),
 * the non-accepting duplicate must imply that seqdone is False.
 * All outgoing transitions are copied to the duplicated state.
 * This guarantees that runs not terminated by the controller continue
 * as usual.
 * \param aut The automaton representing the negation of the spec
 * which needs to be modified
 * \param seqdone special AP signaling termination
 * \return -2 if some error occurred, 1 if monitor, 0 otherwise
 */
sym_check_status prepare_negspec(const twa_graph_ptr& aut,
                                 const std::string& seqdone);

/**
 * \Brief Sanity check of negated specification and controller pair
 * \param negspec State based buechi automaton for the negated spec
 * @param ctrl Aiger for the controller
 * @return 0 if ok, -2 if failed
 */
sym_check_status sanity_check(const sym_aut& negspec,
                              const sym_aut& ctrl);

/**
 * \brief Performs the actual model checking
 *
 * The idea is the following:
 * We construct a new transition system that is
 * the synchronous product of the negation of the
 * specification and the controller.
 * It gets initialized with their respective initial states.
 * Then at each iteration
 * - we compute the successors
 * - we swap the unprimed and primed variables
 * - If we can reach a state that is at the same time final/terminating
 *   for the controller and final for the negation of the spec (as an nfa)
 *   then we have found a violation
 * - Otherwise we exclude all the states that are final in the controller
 *   as these sequences are terminated
 * We loop until either all sequences have (successfully) terminated
 * or we have iterated so many times that looping is inevitable.
 * That is the case after 2^|latches| * 2^|negspec|
 * \todo Find a better bound for this
 * \param negspec The negation of the specification as symbolic automaton
 * \param ctrl The controler in its symbolic version
 * \return The model checking result
 */
sym_check_status symbolic_check(const sym_aut& negspec,
                                const sym_aut& ctrl);