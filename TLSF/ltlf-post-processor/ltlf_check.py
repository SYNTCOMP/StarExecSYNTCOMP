import sys, subprocess, os, time, argparse, tempfile
from typing import List
try:
    import spot, buddy
except:
    sys.path.append("/usr/local/lib/python3.11/site-packages/")
    import spot, buddy

SPOTPY = "python3"
LISA = "lisa"

args = argparse.ArgumentParser(
    prog="ltlf checker",
    description="Checks a terminating aiger controller versus a LTLf specification.")
args.add_argument("ctrl",
                  type=str,
                  help="Path to the controller given as aag file.")
args.add_argument("tlsf",
                  type=str,
                  help="Path to the specification given as tlsf file.")
args.add_argument("--ltlf2fa",
                  choices=["lisa", "basic"],
                  type=str,
                  default="lisa",
                  help="Defines the ltlf to nfa/dfa translator. Currently defaults to lisa.")
args.add_argument("-t", "--timeout",
                  help="Timeout in seconds for model checking. If not specified, timeout is 10sec.",
                  type=int,
                  default=10)
args.add_argument("--timeout_trans",
                  help="Timeout in seconds for ltlf to fa translation. If not specified, timeout is 20sec.",
                  type=int,
                  default = 20)
args.add_argument("--seqdone",
                  help="Name of the special AP indicating that the sequence is terminated.",
                  type=str,
                  default="__seqdone__"
                  )


if __name__ == "__main__":
    from ltlf_fun import tlsf2f

    starexecresult = ""
    def quit():
        print(starexecresult)
        return 0

    myargs = args.parse_args(sys.argv[1:])

    # Get some infos about the solution obtained
    ctrl_result = None
    with open(myargs.ctrl, "r") as f:
        ctrl_result = f.readline().strip("\n").strip(" ").lower()
        ctrl_header = f.readline().strip("\n").strip(" ").lower()
    if ctrl_result not in ["realizable", "unrealizable"]:
        starexecresult += f"Error=output started with {ctrl_result}.\n"
        sys.exit(quit())

    ctrl_cleaned = tempfile.NamedTemporaryFile("w+", suffix=".aag")
    ctrl_cleaned.flush()
    subprocess.run(f"tail -n +2 {myargs.ctrl} >> {ctrl_cleaned.name}", shell=True)

    # get some informations about the instance
    try:
        ins, outs, f, instinfo = tlsf2f(myargs.tlsf)
    except Exception as e:
        starexecresult += f"Error=instance parse error {e}\n"
        sys.exit(quit())
    expected_res =  instinfo.get("status", "unknown")
    if expected_res not in ["realizable", "unrealizable", "unkown"]:
        starexecresult += f"""Error=expected result must be one of "realizable", "unrealizable", "unkown
 but got {expected_res}.\n"""
        sys.exit(quit())
    starexecresult += f"Expected_result={expected_res}\n"

    # Exclude some cases
    if ctrl_result == "realizable" and expected_res == "unrealizable":
        starexecresult += "Error=false positive\n"
        starexecresult += "Model-check-result=MC-INCORRECT\n"
        sys.exit(quit())
    if ctrl_result == "unrealizable" and expected_res == "realizable":
        starexecresult += "Error=false positive\n"
        starexecresult += "Model-check-result=MC-INCORRECT\n"
        sys.exit(quit())
    if ctrl_result == "unrealizable" and expected_res == "unknown":
        starexecresult += "Starexec-result=NEW-UNREALIZABLE\n"

    # Extract informations about controller size
    ctrl_header = ctrl_header.split(" ")
    if len(ctrl_header) < 6 or ctrl_header[0] != "aag":
        starexecresult += f"Error=Invalid aag header!\n"
        sys.exit(quit())
    ctrl_header = [int(x) for x in ctrl_header[1:]]
    num_latch = ctrl_header[2]
    num_gates = ctrl_header[4]

    # We have a candidate controller and an instance with an either realizable or
    # unknown status. We actually need to run the model checker
    # translate the negation of the specification depending on the translator to be used

    # Translation
    # todo: Precompute the automaton and store it as to allow for model checking
    # of larger instances
    negspecfile = tempfile.NamedTemporaryFile("w+", suffix=".hoa")
    if myargs.ltlf2fa == "lisa":
        # Call lisa
        # The resulting hoa has the alive prop (called "alive") -> call to finite
        with tempfile.NamedTemporaryFile("w+", suffix=".ltlf") as fltlf:
            ins, outs, f, instinfo = tlsf2f(myargs.tlsf)
            # Negate
            f = "!("+f+")"
            fltlf.write(f)
            fltlf.flush()
            transres = subprocess.run(f"timeout {myargs.timeout_trans} lisa -exp -out -ltlf {fltlf.name}", shell=True,
                                      stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            if transres.returncode != 0:
                starexecresult += "Error=lisa translation fail or timeout\n"
                sys.exit(quit())
            # Get the automaton and process it
            try:
                aut = spot.automaton("output.hoa")
                autf = spot.to_finite(aut)
                outbdd = buddy.bddtrue
                for aout in outs:
                    outbdd &= buddy.bdd_ithvar(autf.register_ap(aout))
                spot.set_synthesis_outputs(autf, outbdd)
                negspecfile.write(autf.to_str("hoa"))
            except Exception as e:
                starexecresult += f"Error={e}\n"
                sys.exit(quit())
    elif myargs.ltlf2fa == "basic":
        transres = subprocess.run(f"timeout {SPOTPY} ltlf_fun.py {myargs.tlsf} {negspecfile.name}", shell=True)
        if transres.returncode != 0:
            starexecresult += "Error=basic translation fail or timeout\n"
            sys.exit(quit())
    else:
        starexecresult += "Error=unknown translation type\n"
        sys.exit(quit())

    # Actual model check
    rcheckres = subprocess.run(f"timeout {myargs.timeout} ltlfcheck {negspecfile.name} {ctrl_cleaned.name} {myargs.seqdone}",
                              shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if rcheckres.returncode != 0:
        starexecresult += "Error=modelchecking fail or timeout\n"
        sys.exit(quit())

    checkres = rcheckres.stdout.decode("utf-8").strip("\n").strip("").split("\n")[-1]
    if checkres == "model checking result is OK":
        starexecresult += f"""Starexec-result={"" if instinfo.get("status", None) == "realizable" else "NEW-"}REALIZABLE\n"""
        starexecresult += "Model-check-result=MC-CORRECT\n"
        starexecresult += f"Synthesis_latches={num_latch}\n"
        starexecresult += f"Synthesis_gates={num_gates}\n"
        sys.exit(quit())
    if checkres == "model checking result is NOK":
        starexecresult += "Error=controller does not implement specification\n"
        starexecresult += "Model-check-result=MC-INCORRECT\n"
        sys.exit(quit())
    if checkres == "model checking result is NOTERMINATION":
        starexecresult += "Error=controller is not terminating\n"
        starexecresult += "Model-check-result=MC-INCORRECT\n"
        sys.exit(quit())

    starexecresult += "Error=Unknown result\n"
    sys.exit(quit())

