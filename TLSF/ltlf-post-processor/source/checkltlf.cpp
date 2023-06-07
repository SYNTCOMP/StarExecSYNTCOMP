/*
AUTHORS: Philipp Schlehuber-Caissier
DESCRIPTION: Implements the actual model checking for synthesis tools on
             .tlsf benchmarks in the LTLf track.
             Builds on spot for model checking.
arg1 = the implementation of the controller given as aag-file
arg2 = the original specification
arg3 = the output APs, as comma separated list
*/
#include <iostream>
#include <string>
#include <algorithm>
#include <vector>
#include <sstream>

#include <spot/misc/timer.hh>
#include <spot/twa/twagraph.hh>
#include <spot/tl/parse.hh>
#include <spot/tl/ltlf.hh>
#include <spot/tl/formula.hh>
#include <spot/twaalgos/translate.hh>
#include <spot/twaalgos/remprop.hh>
#include <spot/twaalgos/isdet.hh>
#include <spot/twaalgos/dualize.hh>
#include <spot/twaalgos/hoa.hh>
#include <spot/twaalgos/aiger.hh>

namespace
{
  auto str_tolower = [] (std::string s)
    {
      std::transform(s.begin(), s.end(), s.begin(),
                     [](unsigned char c){ return std::tolower(c); });
      return s;
    };

  bool TIMEIT;
  const char* ALIVE = "__xx_alivedummy_xx__";
  auto GLOBTIMER = spot::stopwatch();
  auto HEADER = std::ostringstream();
  auto VALUES = std::ostringstream();
}

static void
split_aps(const std::string& arg, std::vector<std::string>& where)
{
  std::istringstream aps(arg);
  std::string ap;
  while (std::getline(aps, ap, ',')){
    ap.erase(remove_if(ap.begin(), ap.end(), isspace), ap.end());
    where.push_back(str_tolower(ap));
  }
}

static spot::twa_graph_ptr
to_dfa(const spot::formula& f,
       spot::bdd_dict_ptr bdddict){
  /// Construct the complete, deterministic DFA corresponding
  /// to Not(f)
  /// \param f: The formula
  /// \param bdddict: The bdddict to use
  /// \return: The corresponding automaton

  auto ffn = spot::from_ltlf(spot::formula::Not(f), ALIVE);
  spot::twa_graph_ptr DFA;

  auto tr = spot::translator(std::move(bdddict));
  tr.set_type(spot::postprocessor::output_type::Buchi);
  tr.set_pref(2|8); // State based; complete; deterministic
  DFA = tr.run(ffn);
  DFA = spot::to_finite(DFA, ALIVE);
  // Make it complete

  auto pp = spot::postprocessor();
  tr.set_type(spot::postprocessor::output_type::Buchi);
  tr.set_pref(2|4|8); // State based; complete; deterministic
  DFA = pp.run(DFA);

  return DFA;
}

static spot::twa_graph_ptr
to_dba(const spot::twa_graph_ptr& DFA){
  /// Transform a DFA into a DBA with sinks
  /// \param DFA: The automaton to be modified
  /// \returns: A pointer to itself

  // 2 DFA -> DBA with sink
  bool has_cut = false;
  const unsigned N = DFA->num_states();
  for (unsigned s = 0; s < N; ++s){
    if (!DFA->state_is_accepting(s))
      continue;

    // SBA -> There is at least one transition
    bool first = true;
    auto kill_edge = DFA->out_iteraser(s);
    while (kill_edge){
      if (first){
        kill_edge->cond = bddtrue;
        kill_edge->dst = s;
        ++kill_edge;
        first = false;
      } else {
        kill_edge.erase();
        has_cut = true;
      }
    }
  }

  if (has_cut)
    DFA->purge_dead_states();
  return DFA;
}

static spot::twa_graph_ptr
prefix_aut(const spot::formula& f, const std::vector<std::string>& outs,
           spot::bdd_dict_ptr bdddict){
  /// Build the automaton that accepts an infinite length word w iff
  /// every finite prefix is accepted
  /// \param f: formula as string
  /// \param outs: The output propositions
  /// \param bdddict: bdd_dict to use
  /// \return: the automaton, a DBA
  /// """

  // It is for sbacc, but I think the algorithm should work transition based just as well

  // 1 Create complete DFA for not f
  // This needs to be state-based, deterministic and complete
  // Note that completeness EXCLUDES alive
  auto DFA = to_dfa(f, bdddict);
  auto DBA = to_dba(DFA); // Same nomenclature as in paper

  // Swap around the acceptance
  // The NBA is actually det, I do not like the nomenclature
  // in the paper here
  auto NBA = spot::dualize(DBA);

  return NBA;
}

static bool
prefix_check(const spot::twa_graph_ptr& ctrl,
             const spot::formula& f,
             const std::vector<std::string>& outs){

  /// Test intersection ctrl and prefix_aut(not(f))
  /// \param ctrl: Controller as twa_graph
  /// \param f: specification formula
  /// \return: bool true if controller verifies spec

  GLOBTIMER.start();
  auto spec_neg
      = prefix_aut(spot::formula::Not(f), outs,
                   ctrl->get_dict());
  if (TIMEIT){
    HEADER << "ltlfcheck_prefixCstr,";
    VALUES << GLOBTIMER.stop() << ',';
  }


  GLOBTIMER.start();
  auto r = ctrl->intersecting_run(spec_neg);
  if (TIMEIT){
    HEADER << "ltlfcheck_emptinessCheck,";
    VALUES << GLOBTIMER.stop() << ',';
  }

  if (r)
    spot::print_hoa(std::cerr, r->as_twa());

  return (bool) r;
}

int main(int argc, char** argv){
  // For LTLf model checking we follow the approach described in
  // Suguman et al., On Strategies in Synthesis Over Finite Traces,
  // https://arxiv.org/abs/2305.08319
  // It consists of checking emptiness of the product of the controller
  // and the prefix language of the negated specification

  if (argc != 5){
    std::cerr << "Expects 4 arguments: controller file, "\
      "formula, outs, timeit but got\n";
    for (int i = 0; i < argc; ++i)
      std::cerr << i << " : " << argv[i] << '\n';
    std::cerr << std::endl;
    throw std::runtime_error("Wrong number of arguments");
  }


  HEADER.clear();
  VALUES.clear();

  auto f = spot::parse_formula(argv[2]);

  std::vector<std::string> outs;
  split_aps(argv[3], outs);
  TIMEIT = std::stoi(argv[4]);


  // The common bdd_dict
  auto bdddict = spot::make_bdd_dict();
  // Get the controller from aag file
  GLOBTIMER.start();
  auto ctrl
    = spot::aig::parse_aag(std::string(argv[1]),
                           bdddict)->as_automaton(false);
  if (TIMEIT){
    HEADER << "ltlfcheck_aag2aut,";
    VALUES << GLOBTIMER.stop() << ',';
  }

  bool failed = prefix_check(ctrl, f, outs);

  std::cout << "PostProc_PrefixCheckOk="
            << (not failed) << '\n';
  if (TIMEIT){
    std::cerr << "c checkltlfcsvheaders," << HEADER.str() << '\n';
    std::cerr << "c checkltlfcsvvalues," << VALUES.str() << '\n';
  }

  return failed;
}








