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
#include <signal.h>
#include <iostream>
#include <fstream>
#include "OptionsR.hh"
#include "ReadQ.hh"
#include "RASolverNoIt.hh"
#include "RASolverIt.hh"
#include "utils.hh"
#include "Tests.hh"
#include "ObjectCounter.hh"
using std::endl;
using std::cerr;
using std::cin;
int verbose=0;
int use_blocking=0; // should go into something that is passed on to to solvers
int use_pure=0; // should go into something that is passed on to to solvers
int use_universal_reduction=0; 

static void SIG_handler(int signum);
void run(const OptionsR& options);
ostream& print_usage(ostream& out);

int main(int argc, char** argv) {
  signal(SIGHUP, SIG_handler);
  signal(SIGTERM, SIG_handler);
  signal(SIGINT, SIG_handler);
  signal(SIGABRT, SIG_handler);
  signal(SIGUSR1, SIG_handler);
  cerr<<"c RAReQS, v1.1"<<std::endl;
  cerr<<"c (C) 2012 Mikolas Janota, mikolas.janota@gmail.com"<<std::endl;
#ifndef EXPERT
  // prepare nonexpert options
  // rareqs -uupb -h3 FILENAME
  char* nargv[4];
  char o1[6];
  char o2[4];
  char hs[2];
  strcpy(o1, "-uupb");
  strcpy(o2, "-h3");
  strcpy(hs, "-");
  nargv[0]=argv[0];
  nargv[1]=o1;
  nargv[2]=o2;
  nargv[3]=argc>=2 ? argv[1] : hs;
  if (argc<=1) { cerr<<"c INFO: reading from std in"<<std::endl;}
  if (argc>2) { cerr<<"c WARNING: ingoring some options after FILENAME"<<std::endl;}
  argv=nargv;
  argc=4;
#else
  cerr<<"c WARNING: running in the EXPERT mode, I'm very stupid without any options."<<std::endl;
#endif

  OptionsR options;
  if (!options.parse(argc,argv)) {
    print_usage(cerr);
    exit(100);
  }
  if (options.help) {
    print_usage(cout);
    exit(0);
  }
  verbose=options.verbose;
  use_blocking = options.blocking;
  use_pure = options.pure;
  use_universal_reduction = options.universal_reduction;
  cout << "c " << "hybrid " << options.hybrid << endl;
  cout << "c " << "pure " << options.pure << endl;
  cout << "c " << "blocking " << options.blocking << endl;
  cout << "c " << "unit " << options.unit << endl;
  cout << "c " << "universal_reduction " << options.universal_reduction << endl;
  run(options);
}

void print_exit (bool sat) {
  cout << "s cnf " << (sat ? '1' :'0') << endl;
  exit(sat ? 10 : 20);
}

void run(const OptionsR& options) {
    if (options.test) {
      Tests ts;
      bool r = ts.test_all();
      cerr << (r ? "OK" : "FAIL") << endl;
      exit (r ? 0 : 100);
    }
    const string flafile=options.file_name;
    gzFile ff=Z_NULL;
    Reader* fr;

    if (flafile.size()==1 && flafile[0]=='-') {
      fr=new Reader(cin);
    } else {
      ff = gzopen(flafile.c_str(), "rb");
      if (ff== Z_NULL) {
          cerr << "Unable to open file: " << flafile << endl;
          exit(100);
      }
      fr = new Reader(ff);
    }

    ReadQ rq(*fr, options.qube_input!=0); 
    try {
      rq.read();
    } catch (ReadException& rex) {
       cerr << rex.what() << endl;
       exit(100);
    }

    if (options.qube_input!=0 && rq.get_qube_output() >= 0) {
      print_exit(rq.get_qube_output()==1);
    }

    if (!rq.get_header_read()) {
      if (rq.get_prefix().size()==0) { 
        cerr << "ERROR: Formula has empty prefix and no problem line." << endl;
        exit(100);
      }
      cerr << "WARNING: Missing problem line in the input file." << endl;
    }

    if (rq.get_prefix().size()==0) {
      print_exit(rq.get_clauses().empty());
    }

    const auto incremental = options.incremental;
    Fla fla;
    build_fla(rq.get_prefix(),rq.get_clauses(), fla);
    RASolverIt*    i = NULL;
    RASolverNoIt*  ni = NULL;
    if (incremental) {
      i = new RASolverIt(rq.get_max_id(), rq.get_prefix(), rq.get_clauses(),  (options.unit != 0));
    } else {
      ni =  new RASolverNoIt(rq.get_max_id(), fla, options.unit, 0);
      ni->set_hybrid((size_t)options.hybrid);
    }
    QSolver& s = incremental  ?  *(QSolver*)i : *(QSolver*)ni;

    const bool win = s.solve();
    const bool sat = fla.q==EXISTENTIAL ? win : !win;
    if (win) print(cout << "V ", fla.pref, s.move()) << endl;
#ifdef DBG
    if (ni) delete ni;
    if (i)  delete i;
    cerr << "#oc " << ObjectCounter::count() << endl; 
#endif
    print_exit(sat);
}

static void SIG_handler(int signum) {
  cerr << "# received external signal " << signum << endl;
  cerr << "Terminating ..." << endl;
  exit(0);
}

ostream& print_option (CONSTANT char* option_name, CONSTANT char* description,
        ostream& out) {
    out << "\t" << option_name;
    for (int i=10-strlen(option_name); i>0;--i) out << ' ';
    return out << description << endl;
}

ostream& print_usage(ostream& out){
    out << "USAGE: ";
#ifdef EXPERT
    out << "[options] INSTANCE"  << endl;
    print_option("INSTANCE","QDIMACS file, if - then read from standard input", out);
    print_option("--help","print help", out);
    print_option("--test","self test", out);
    print_option("-v","increase verbosity", out);
    print_option("-i", "incremental solver",out);
    print_option("-u", "Increase unit propagation level. -u for nonincremental solver, -uu for both incremental an nonincremental.",out);
    print_option("-h ARG", "Start incremental when fla has only ARG levels left (hybrid approach).",out);
    print_option("-b", "blocking",out);
    print_option("-p", "pure lits",out);
    print_option("-r", "universal reduction",out);
#else
    out << "[instance name]"  << endl;
    print_option("[instance name]","QDIMACS file, if - then read from standard input", out);
#endif
    return out;
}
