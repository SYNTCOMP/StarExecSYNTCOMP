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
#include "minisat_auxiliary.hh"

char get_sign(lbool l) {
    if (l==l_True) return '+';
    else if (l==l_False) return '-';
    else if (l==l_Undef) return '?';
    assert (false);
    return -1;
}

ostream& print(ostream& out, Lit l) {return out << (sign(l) ? "-" : "+") << var(l); }

ostream& print(ostream& out, const vec<Lit>& lv) {for (int i=0;i<lv.size();++i) out <<  lv[i] << " "; return out;}

ostream& print(ostream& out, const vector<Lit>& lv) {for (size_t i=0;i<lv.size();++i) out << lv[i] << " "; return out;}

ostream& print_model(ostream& out,const vec<lbool>& lv) { return print_model(out, lv, 1, lv.size()-1); }

ostream& print_model(ostream& out, const vec<lbool>& lv, int l, int r) {
    for (int i=l;i<=r;++i) {
        int v=0;
        if (lv[i]==l_True) v=i;
        else if (lv[i]==l_False) v=-i;
        else if (lv[i]==l_Undef) v=0;
        else assert (false);
        out << v << " ";
    }
    return out;
}

ostream& operator << (ostream& outs, Lit lit) { return print(outs, lit); }

ostream& operator << (ostream& outs, lbool lb) {
  if (lb==l_Undef) return outs << "l_Undef";
  if (lb==l_True) return outs << "l_True";
  if (lb==l_False) return outs << "l_False";
  assert(0);
  return outs << "ERROR";
}
