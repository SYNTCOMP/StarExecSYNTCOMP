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
#include "auxiliary.hh"
#include "VarSet.hh"

VarSet::VarSet() 
  : _size(0)
{}

VarSet::VarSet(const VariableVector& variables)
: _size(0) 
{ FOR_EACH(vi, variables) add(*vi); }

VarSet::VarSet(const VarVector& variables) 
: _size(0)
{ add_all(variables); }

void   VarSet::add_all(const VarVector& variables) {
  FOR_EACH(vi, variables) add(*vi);
}

const_VarIterator::const_VarIterator(const VarSet& ls, size_t x) : ls(ls), i(x) {
  while ((i<ls.physical_size()) && !ls.get(i)) ++i;
}

