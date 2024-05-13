import sys, subprocess, os, time, argparse
from typing import List
try:
    import spot, buddy
except:
    sys.path.append("/usr/local/lib/python3.11/site-packages/")
    import spot, buddy

ALIVE = "__alive__"
SEQONE = "__seqone__"

SPOTPY = "python3"
LTLFCHECK = "ltlfcheck"
SYFCO = "syfco"

def save_neg_spec(fpath:str, f:str, outs:List[str]):
    """
    Create a nfa for the negation of the spec
    :param fpath: file path to store hoa file
    :param f: The spec to negate
    :param outs: controlled APs
    :return:
    """
    # Extra alive prop
    neg_ffin = spot.from_ltlf(spot.formula_Not(spot.formula(f)), ALIVE)
    # Get the corresponding terminal SBA, can be non-det
    neg_aut = spot.translate(neg_ffin, "sba")
    # Transform it into an nfa (this removes alive)
    neg_nfa = spot.to_finite(neg_aut, ALIVE)

    outbdd = buddy.bddtrue
    for aout in outs:
        neg_nfa.register_ap(aout)
        outbdd &= buddy.bdd_ithvar(neg_nfa.register_ap(aout))
    spot.set_synthesis_outputs(neg_nfa, outbdd)

    with open(fpath, "w") as of:
        of.write(neg_nfa.to_str("hoa"))
        of.write("\n")
    return

def save_ctrl(fpath:str, f:str, outs:List[str]) -> bool:
    """
    Synthesize the terminating controller for the given spec
    :param fpath: File path to store aiger file
    :param f: The spec
    :param outs: controlled APs
    :return:
    """

    ffin = spot.from_ltlf(f, ALIVE)
    aut = spot.translate(ffin, "deterministic", "sba")
    dfa = spot.to_finite(aut, ALIVE)

    # Making the "end" controllable
    # We want a special AP SEQDONE that is true iff it is the last "letter" of the word.
    # After this is true, the sequence is terminated, i.e. further requests to the controller are invalid.
    # Get an "accepting" sink state, Duplicate the edge with "done", all other edge get "not done"
    done = buddy.bdd_ithvar(dfa.register_ap(SEQONE))  # sd = sequence_done; True on the last letter
    ndone = buddy.bdd_not(done)
    sink = dfa.new_state()

    # List all currently active edges
    ee = [dfa.edge_number(e) for e in dfa.edges()]

    for eidx in ee:
        if (dfa.state_is_accepting(dfa.edge_storage(eidx).dst)):
            e = dfa.edge_storage(eidx)
            s = e.src
            c = e.cond & done
            dfa.new_edge(s, sink, c)
        dfa.edge_storage(eidx).cond &= ndone

    # Mark all as non-accepting
    for e in dfa.edges():
        e.acc = spot.mark_t([])

    # Mark sink as accepting
    dfa.new_edge(sink, sink, done, [0])
    # Transform into SEQDONE controlled game
    outbdd = buddy.bddtrue
    for aout in outs + [SEQONE]:
        outbdd &= buddy.bdd_ithvar(dfa.register_ap(aout))
    dfas = spot.split_2step(dfa, outbdd, True)
    #print("dfa")
    #print(dfas.to_str("hoa"))

    won = spot.solve_game(dfas)
    if not won:
        with open(fpath, "w") as of:
            of.write("UNSATISFIABLE\n");
            return

    mms = spot.solved_game_to_mealy(dfas)
    #print("mms")
    #print(mms.to_str("hoa"))
    aig = spot.mealy_machine_to_aig(mms, "both")
    with open(fpath, "w") as of:
        of.write(aig.to_str())
        of.write("\n")

def tlsf2f(fpath:str):
    """
    Translate a tlsf file into a ltlf formula, input, output propositions
    and knowledge about the instance
    :param fpath: Input file
    :return: (input, output, formula, allspec)
    """
    if (sys.version_info.minor >= 7):
        rins = subprocess.run(f"{SYFCO} -ins {fpath}", shell=True, check=True, capture_output=True)
        routs = subprocess.run(f"{SYFCO} -outs {fpath}", shell=True, check=True, capture_output=True)
        fs = subprocess.run(f"{SYFCO} -f ltlxba-fin -m fully {fpath}", shell=True, check=True, capture_output=True)
    else:
        rins = subprocess.run(f"{SYFCO} -ins {fpath}", shell=True, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        routs = subprocess.run(f"{SYFCO} -outs {fpath}", shell=True, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        fs = subprocess.run(f"{SYFCO} -f ltlxba-fin -m fully {fpath}", shell=True, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    ins = rins.stdout.decode("utf-8").replace(" ", "").strip("\n").split(",")
    outs = routs.stdout.decode("utf-8").replace(" ", "").strip("\n").split(",")
    f = fs.stdout.decode("utf-8").replace(" ", "").strip("\n")

    # Check for status
    with open(fpath, "r") as tlsffile:
        while True:
            l = tlsffile.readline()
            if l.startswith("""//#!SYNTCOMP"""):
                break
        allspec = dict()
        while True:
            l = tlsffile.readline()
            if l.startswith("//#."):
                break
            lp = [x.lower() for x in l[2:-1].split(" : ")]
            if len(lp) != 2:
                allspec = dict()
                break
            allspec[lp[0]] = lp[1]

    return ins, outs, f, allspec

def gen1(fpath:str, repath:str):
    """
    Run on one instance, generate controller and negated specification
    :param fpath: Input file
    :param repath: Folder to store results
    :return:
    """

    ins, outs, f = tlsf2f(fpath)

    save_neg_spec(os.path.join(repath, f"n_{os.path.split(fpath)[-1]}.hoa"), f, outs)
    save_ctrl(os.path.join(repath, os.path.split(fpath)[-1]+".aag"), f, outs)

def modelcheck1(instancepath:str, logfile:str, timeout:int) -> None:
    """
    Check if the intersection controller x negation of specification is empty
    :param instancepath: Name of the aag file for the controller
    :param logfile: Logfile path
    :return:
    """
    ctrl = instancepath
    negspec = os.path.split(instancepath)
    negspec = os.path.join(*negspec[:-1], "n_" + negspec[-1][:-4]+".hoa")
    subprocess.run(f"echo {negspec} >> {logfile}", shell=True)
    with open(ctrl, "r") as fc:
        l = fc.readline()
        print(l)
        if l.startswith("UNSATISFIABLE"):
            subprocess.run(f"echo UNSATISFIABLE >> {logfile}", shell=True)
            return

    # Ctrl is present, actual verification
    res = subprocess.run(f"""timeout {timeout} {LTLFCHECK} {negspec} {ctrl} "{SEQONE}" >> {logfile}""", shell=True, capture_output=True)
    if res.returncode != 0:
        s = f"Returned {res.returncode} with {res.stdout.decode('utf-8')} and {res.stderr.decode('utf-8')}"
        subprocess.run(f"echo {s} >> {logfile}", shell=True)
    return


args = argparse.ArgumentParser(
    prog="LTLf functionalities",
    description="Functionnalities for LTLf synthesis and model checking")
args.add_argument("action",
                  choices=["generate", "modelcheck", "internalgen", "internalcheck", "internalgennegspec"],
                  help="Action to perform. generate will create all the "
                  "control (aag-files) and negations of the specifications "
                  "(hoa files prefixed with n_) using a default implementation "
                  "based on spot. The actions prefixed with internal are mainly "
                  "used to run subprocesses.")
args.add_argument("inpath",
                  help="Folder for instances to process. For generate, a folder "
                  "containing the tlsf is expected. For modelcheck, a folder "
                  "containing the controller (instance_name.aag) and the negated "
                  "specification (n_instance_name.hoa) is expected.")
args.add_argument("outpath",
                  help="Folder or file to store results. For generate, a folder "
                  "in which the pairs of controller and negated specification will be stored. "
                  "For modelcheck, the path to a logfile.")
args.add_argument("-t", "--timeout",
                  help="Timeout in seconds. If not specified, timeout is 10sec.",
                  type = int,
                  default = 10)

if __name__ == "__main__":
    myargs = args.parse_args(sys.argv[1:])

    if myargs.action == "generate":
        for x in os.listdir(myargs.inpath):
            print(time.time(), f"Running {x}")
            print(f"timeout {myargs.timeout} {SPOTPY} ltlf_fun.py internalgen {os.path.join(myargs.inpath, x)} {myargs.outpath}")
            subprocess.run(f"timeout {myargs.timeout} {SPOTPY} ltlf_fun.py internalgen {os.path.join(myargs.inpath, x)} {myargs.outpath}",
                           shell=True)
    elif myargs.action == "internalgen":
        gen1(myargs.inpath, myargs.outpath)
    elif myargs.action == "internalgennegspec":
        ins, outs, f = tlsf2f(myargs.inpath)
        save_neg_spec(myargs.outpath, f, outs)
    elif myargs.action == "modelcheck":
        for x in os.listdir(myargs.inpath):
            if x.endswith("hoa"):
                continue
            print(time.time(), f"Model checking {x}")
            subprocess.run(f"timeout {myargs.timeout} {SPOTPY} ltlf_fun.py internalcheck {os.path.join(myargs.inpath, x)} {myargs.outpath}",
                           shell=True)

    elif myargs.action == "internalcheck":
            modelcheck1(myargs.inpath, myargs.outpath, myargs.timeout-1)
    else:
        raise RuntimeError(f"Unrecognized arguments {sys.argv}")





