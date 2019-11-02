/* 
 * File:   MiniSatExt.hh
 * Author: mikolas
 *
 * Created on November 29, 2010, 5:40 PM
 */

#ifndef MINISATEXT_HH
#define	MINISATEXT_HH
#include "core/Solver.h"
#include "auxiliary.hh"
#include "VarVector.hh"

namespace Minisat {
  class MiniSatExt : public Solver {
  public:
    inline void bump(const Var var)        { varBumpActivity(var); }
    inline bool is_decision(Var v)         { return reason(v)==CRef_Undef; }
    inline void new_variables(Var max_id);
    inline void new_variables(const std::vector<Var>& variables);
  };

  inline void MiniSatExt::new_variables(Var max_id) {
    const int target_number = (int)max_id+1;
    while (nVars() < target_number) newVar();
  }

  inline void MiniSatExt::new_variables(const std::vector<Var>& variables) {
    Var max_id = 0;
    FOR_EACH(variable_index, variables) {
      const Var v = *variable_index;
      if (max_id < v) max_id = v;
    }
    new_variables(max_id);
  }
}
#endif	/* MINISATEXT_HH */

