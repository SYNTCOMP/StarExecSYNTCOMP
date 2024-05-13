//
// Created by philipp on 4/30/24.
//

#include <iostream>
#include <fstream>

#include "ltlf_modelcheck.h"
#include "parseaut/public.hh"
#include "twaalgos/hoa.hh"

int main(int argc, char* argv[])
{
  if (argc != 4)
  {
    std::cout << "Usage: ltlfcheck NEGSPEC CTRL SEQDONE\n\n"
                 "ltlfcheck expects exactly three positional arguments.\n"
                 "NEGSPEC the path of a hoa file containing the NFA representing "
                 "the negation of the specification, encoded as state-based "
                 "Buechi automaton.\n"
                 "CTRL the path of a aag file containing the controller.\n"
                 "SEQDONE the name of the special AP corresponding "
                 "to the terminating signal.";
    return 0;
  }

  auto quit = [](sym_check_status s)
    {
      std::cout << "model checking result is " << s << '\n';
      return 0;
    };

  const std::string seqdone(argv[3]);

  auto dict = spot::make_bdd_dict();
  auto autp = spot::parse_aut(argv[1], dict);

  auto negspec = autp->aut;
  if (!negspec)
    throw std::runtime_error("ltlfcheck::main(): Parsing of hoa file failed.");

  // Parse the aiger
  auto ctrl_aig = spot::aig::parse_aag(argv[2], negspec->get_dict());

  // Transform the negspec into the desired form
  sym_check_status prep_res = prepare_negspec(negspec, seqdone);
  if (prep_res != sym_check_status::OK)
    return quit(prep_res);
  // Get the symbolic version
  auto sym_negspec = sym_aut::to_symbolic(negspec, "X", seqdone);
  my_out << "Symbolic negspec:\n" << sym_negspec;
  auto sym_ctrl = sym_aut::to_symbolic(ctrl_aig, "L", seqdone);
  my_out << "Symbolic controller:\n" << sym_ctrl;

  // Sanity check
  // The controller must define the same input/output propositions
  if (sanity_check(sym_negspec, sym_ctrl) == sym_check_status::STRUCTURALERROR)
    return quit(sym_check_status::STRUCTURALERROR);

  // Check and return
  return quit(symbolic_check(sym_negspec, sym_ctrl));
}
