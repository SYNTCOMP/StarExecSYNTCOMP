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

#include <iostream>
#include <string>
#include <stdlib.h>
#include <getopt.h>

#include "abssynthe.h"
#include "logging.h"
#include "aig.h"

const char* ABSSYNTHE_VERSION = "Swiss-Abssynthe 2.0";
const int EXIT_STATUS_REALIZABLE = 10;
const int EXIT_STATUS_UNREALIZABLE = 20;

struct settings_struct settings;


static struct option long_options[] = {
    {"verbose_level", required_argument, NULL, 'v'},
    {"use_trans", no_argument, NULL, 't'},
    {"use_abs", optional_argument, NULL, 'a'},
    {"min_ordering", optional_argument, NULL, 'm'},
    {"use_rsynth", optional_argument, NULL, 'r'},
    {"parallel", no_argument, NULL, 'p'},
    {"ordering_strategies", no_argument, NULL, 's'},
    {"help", no_argument, NULL, 'h'},
    {"comp_algo", required_argument, NULL, 'c'},
    {"fold", required_argument, NULL, 'f'},
    {"out_file", required_argument, NULL, 'o'},
    {"win_region", required_argument, NULL, 'w'},
    {"ind_cert", required_argument, NULL, 'i'},
    {NULL, 0, NULL, 0}
};

void usage() {
    std::cout << ABSSYNTHE_VERSION << std::endl
<< "usage:" << std::endl
<< "./abssynthe [-h] [-t] [-a] [-r] [-p] [-s] [-c {1,2,3,4}] "
<< "[-f N_FOLDS] [-m] [-v VERBOSE_LEVEL] [-o OUT_FILE] spec"
<< std::endl
<< "positional arguments:" << std::endl
<< "spec                               input specification in extended AIGER format"
<< std::endl
<< "optional arguments:" << std::endl
<< "-h, --help                         show this help message and exit"
<< std::endl
<< "-t, --use_trans                    compute a transition relation"
<< std::endl
<< "-a[THRESHOLD], --use_abs[THRESHOLD]"
<< std::endl
<< "                                   use abstraction when possible, and try"
<< std::endl
<< "                                   to keep BDD sizes below THRESHOLD"
<< std::endl
<< "-r, --use_rsynth                   use RSynth's self-substitution"
<< std::endl
<< "                                   to generate output strategy"
<< std::endl
<< "-p, --parallel                     launch all solvers in parallel"
<< std::endl
<< "-s, --strat_ordering               launch solvers in parallel with different"
<< std::endl
<< "                                   strategies for the reorderings"
<< std::endl
<< "-c {1,2,3,4}, --comp_algo {1,2,3,4}" << std::endl
<< "                                   choice of compositional algorithm"
<< std::endl
<< "-f N_FOLDS, --fold N_FOLDS         merge subgames with non-empty dependency"
<< std::endl
<< "                                   set (N_FOLDS rounds of folding)"
<< std::endl
<< "-m, --min_ordering                 manual reordering just before generating"
<< std::endl
<< "                                   output, to obtain a smaller circuit"
<< std::endl
<< "-v VERBOSE_LEVEL, --verbose_level VERBOSE_LEVEL" << std::endl
<< "                                   Verbose level string, i.e. (D)ebug,"
<< std::endl
<< "                                   (W)arnings, (L)og messages" << std::endl
<< "-o OUT_FILE, --out_file OUT_FILE   Output file path. If the file extension"
<< std::endl
<< "                                   is .aig, binary output format will be used."
<< std::endl
<< "                                   With file extension .aag, ASCII output will"
<< std::endl
<< "                                   be used. The argument is ignored if spec"
<< std::endl
<< "                                   is not realizable." << std::endl
<< "-w WIN_REGION_OUT_FILE, --win_region WIN_REGION_OUT_FILE" << std::endl
<< "                                   Output winning region file path. Same "
<< std::endl
<< "                                   file extension rules as for OUT_FILE."
<< std::endl
<< "-i IND_CERT_OUT_FILE, --ind_cert IND_CERT_OUT_FILE" << std::endl
<< "                                   Output a certificate of the winning region "
<< std::endl
<< "                                   being inductive (.aig .aag or .qdimacs)."
<< std::endl;
}

void parse_arguments(int argc, char** argv) {
#ifndef NDEBUG
    std::cout << "AbsSynthe called: ";
    for (int i = 0; i < argc; i++)
        std::cout << argv[i] << " ";
    std::cout << std::endl;
#endif
    // default values
    settings.use_trans = false;
    settings.use_abs = false;
    settings.use_rsynth = false;
    settings.parallel = false;
    settings.ordering_strategies = false;
    settings.final_reordering = false;
    settings.comp_algo = 0;
    settings.n_folds = 0;
    settings.abs_threshold = 0;
    settings.spec_file = NULL;
    settings.out_file = NULL;
    settings.win_region_out_file = NULL;
    settings.ind_cert_out_file = NULL;

    // read values from argv
    int opt_key;
    int opt_index;
    while (true) {
        opt_key = getopt_long(argc, argv, "v:ta::prsmc:f:o:w:i:", long_options,
                              &opt_index);
        if (opt_key == -1)
            break;
        if (long_options[opt_index].flag != NULL)
            break;
        switch (opt_key) {
            case 'h':
                usage();
                exit(0);
            case 'v':
                parseLogLevelString(optarg);
                break;
            case 't':
                settings.use_trans = true;
                break;
            case 'r':
                settings.use_rsynth = true;
                break;
            case 'm':
                settings.final_reordering = true;
                break;
            case 'a':
                settings.use_abs = true;
                settings.abs_threshold = 0;
                if (optarg) {
                    settings.abs_threshold = atoi(optarg);
                    if (settings.abs_threshold < 0)
                        errMsg("Expected a non-negative integer as "
                               "threshold");
                }
                break;
            case 'p':
                settings.parallel = true;
                break;
            case 's':
                settings.ordering_strategies = true;
                break;
            case 'c':
                settings.comp_algo = atoi(optarg);
                if (settings.comp_algo < 1 || settings.comp_algo > 4)
                    errMsg("Expected comp_algo to be in {1,2,3,4} "
                           "instead of " + std::string(optarg));
                break;
            case 'f':
                settings.n_folds = atoi(optarg);
                if (settings.comp_algo < 1)
                    errMsg("Expected number of desired folds to be at least 1.");
                break;
            case 'o':
                settings.out_file = optarg;
                break;
            case 'w':
                settings.win_region_out_file = optarg;
                break;
            case 'i':
                settings.ind_cert_out_file = optarg;
                break;
            default:
                usage();
                exit(1);
        }
    }
    argc -= optind;
    argv += optind;
    if (argc > 1) {
        errMsg("Too many arguments");
    } else if (argc != 1) {
        errMsg("Too few arguments");
    }
    settings.spec_file = argv[0];
}

int main(int argc, char** argv) {
    parse_arguments(argc, argv);
    // solve the synthesis problem
    bool result;
    if (settings.parallel) {
        result = solveParallel();
    } else {
        // try to open the spec now
        AIG aig(settings.spec_file);
        result = solve(&aig);
    }
    // return the realizability test result
    logMsg("Realizable? " + std::to_string(result));
    exit(result ? EXIT_STATUS_REALIZABLE : EXIT_STATUS_UNREALIZABLE);
}
