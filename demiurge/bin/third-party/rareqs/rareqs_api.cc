#include "rareqs_api.hh"
#include "qtypes.hh"
#include "RASolverNoIt.hh"

using namespace std;

bool RAReQSisSatModel(const std::vector<std::pair<std::vector<int>, RAReQSQuantifierType> > &quantifier_prefix,
                      const std::list<std::vector<int> >& cnf,
                      std::vector<int> &model)
{
  
  int max_cnf_var = 0;
  Prefix p;
  for(size_t level = 0; level < quantifier_prefix.size(); ++level)
  {
    for(size_t var_cnt = 0; var_cnt < quantifier_prefix[level].first.size(); ++var_cnt)
    {
      int var = quantifier_prefix[level].first[var_cnt];
      if(var > max_cnf_var)
        max_cnf_var = var;
    }
    QuantifierType qt = QuantifierType::UNIVERSAL;
    if(quantifier_prefix[level].second == RAReQS_EXISTS)
      qt = QuantifierType::EXISTENTIAL;
    Quantification q(qt, quantifier_prefix[level].first);
    p.push_back(q);
  }

  vector< LitSet > clause_vector;
  for(list<vector<int> >::const_iterator it = cnf.begin(); it != cnf.end(); ++it)
  {
    vector<Lit> lits;
    lits.reserve(it->size());
    for(size_t lit_cnt = 0; lit_cnt < it->size(); ++lit_cnt)
    {
      int li = (*it)[lit_cnt];
      const Var v = abs(li);
      lits.push_back(li > 0 ? mkLit(v): ~mkLit(v));
    }
    clause_vector.push_back(LitSet(lits));
  }

  Fla fla;
  build_fla(p, clause_vector, fla);
  
  RASolverNoIt* ni =  new RASolverNoIt(max_cnf_var, fla, 2, 0);
  ni->set_hybrid(3);

  const bool win = ni->solve();
  const bool sat = fla.q==EXISTENTIAL ? win : !win;
  if (win) {
    model.clear();
    model.reserve(fla.pref.size());
    FOR_EACH(vi, fla.pref) {
      const Var var = *vi;
      const lbool v = var < ni->move().size() ? ni->move()[var] : l_Undef;
      if (v==l_True)
        model.push_back(var);
      else if (v==l_False)
        model.push_back(-var);
    }
  }

  delete ni;
  ni = NULL;
  
  return sat;
}