/**************************************************************************
 * Copyright (c) 2015, Guillermo A. Perez, Universite Libre de Bruxelles
 * 
 * This file is part of the (Swiss) AbsSynthe tool.
 * 
 * AbsSynthe is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * AbsSynthe is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with AbsSynthe.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * 
 * Guillermo A. Perez
 * Universite Libre de Bruxelles
 * gperezme@ulb.ac.be
 *************************************************************************/

#ifndef ABSSYNTHE_H
#define ABSSYNTHE_H

#include "cuddObj.hh"
#include "aig.h"

struct settings_struct {
    bool use_trans;
    bool use_abs;
    bool use_rsynth;
    bool parallel;
    bool ordering_strategies;
    bool final_reordering;
    int comp_algo;
    int n_folds;
    int abs_threshold;
    const char* spec_file;
    const char* out_file;
    const char* win_region_out_file;
    const char* ind_cert_out_file;
};

extern struct settings_struct settings;

bool solve(AIG*,Cudd_ReorderingType reordering=CUDD_REORDER_SIFT);
bool solveParallel();

#endif
