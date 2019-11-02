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
/*
 * File:  OptionsR.cc
 * Author:  mikolas
 * Created on:  Fri Nov 18 14:06:08 CAST 2011
 * Copyright (C) 2011, Mikolas Janota
 */
#include "OptionsR.hh"
#include <iostream>
#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>

OptionsR::OptionsR()
: verbose(0)
, incremental(0)
, qube_input (0)
, test(0)
, unit(0)
, hybrid(0)
, blocking(0)
, pure(0)
, universal_reduction(0)
, help(0)
{}

bool OptionsR::parse(int argc,char **argv) {
    static struct option long_options[] = {
      {"qube", no_argument,  &qube_input, 1}
      ,{"test", no_argument,  &test, 1}
      ,{"help", no_argument,  &help, 1}
      ,{0, 0, 0, 0}
     };

    int c;
    bool return_value = true;
    while (1) {
       /* getopt_long stores the option index here. */
       int option_index = 0;
       c = getopt_long(argc, argv, "rpbh:iuv", long_options, &option_index);
       opterr = 0;
       /* Detect the end of the options. */
       if (c == -1) break;
       switch (c) {
       case 'v': ++verbose; break;
       case 'i': ++incremental; break;
       case 'u': ++unit; break;
       case 'h': hybrid=atoi(optarg); break;
       case 'b': ++blocking; break;
       case 'p': ++pure; break;
       case 'r': ++universal_reduction; break;
       case '?':
         if ( (optopt == 'h') ) fprintf (stderr, "Option -%c requires an argument.\n", optopt);
         else if (isprint(optopt)) fprintf (stderr, "Unknown option `-%c'.\n", optopt);
         else fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
         return_value = false;
         break;
       }

     }

    if (!test && !help) {
      if (optind < argc) {
        file_name = argv[optind];
      } else {
        std::cerr << "Filename missing." << std::endl;
        return_value=false;
      }
    }


    return return_value;
}
