#!/usr/bin/env python3
"""
This script generates detailed information for each
benchmark family

Philipp Schlehuber-Caissier @ EPITA, 2023
"""

import argparse
import itertools
import math

import matplotlib.pyplot as plt
import pandas as pd
import sys, os
import json

from utils import *

partPerTrack = {
    "PGreal": {
        "strix pgame": "Strix",
        "knor repair": "Knor",
        "ltlsynt": "ltlsynt"
    },
    "PGsynt": {
        "strix pgame": "Strix",
        "knor repair": "Knor",
        "ltlsynt": "ltlsynt"
    },
    "LTLreal": {
        "strix pgame": "Strix",
        "ltlsynt23": "ltlsynt",
        "Otus": "Otus",
        "sdf": "sdf",
        "spore": "SPORE",
        "strix": "Strix",
        "abonsai": "Acacia bonsai"
    },
    "LTLsynt": {
        "abonsai": "Acacia bonsai",
        "ltlsynt23": "ltlsynt",
        "otus": "Otus",
        "sdf": "sdf",
        "strix": "Strix"
    },
    "LTLFreal": {
        "lisa-syntcomp": "lisa",
        "ltlfsynt23prebuild": "ltlfsynt",
        "lydiasyft": "LydiaSyft",
        "nike": "Nike",
        "tople": "tople"
    }
}

bestDF = dict()

def doPlot(df, participants, nInst, synthesis, bound, verbose = False):

    if synthesis:
        ff, aa = plt.subplots(1, 2, figsize=(9,4))
        aa0 = aa[0]
        aa1 = aa[1]
    else:
        ff, aa = plt.subplots(1, 1, figsize=(9,4))
        aa0 = aa

    summary = dict()
    best = dict()

    # Group them by tool configuration for the rest
    for (tool, config), subdf in df.groupby([df.solver, df.configuration]):

        tool_name = participants[tool.lower()]

        if verbose:
            subdf.to_csv(f"{tool_name}.{config}.csv")
        numbenchs = subdf.shape[0]
        totTime = subdf[bound].sum() if numbenchs else math.inf
        # store the summary
        if not synthesis:
            summary[(tool_name, config)] = (numbenchs, totTime)
        else:
            summary[(tool_name, config)] = (numbenchs, totTime, subdf["Synthesis_score"].sum())

        # store the info of the best configs
        if (tool_name not in best or numbenchs > best[tool_name][0]
                or (numbenchs == best[tool_name][0] and
                    totTime < best[tool_name][1])):
            best[tool_name] = (numbenchs, totTime, config, subdf)
        print(f"Tool {tool_name}, Configuration {config} solved " +
              f"{numbenchs} benchs " +
              f"in {totTime}s")

    # Show best plot per tool
    markers = itertools.cycle(('h', '+', '.', 'o', '*', 'D', 's'))
    for tool, (numbenchs, totTime, config, subdf) in best.items():
        print(f"The best config of {tool} solved "
              f"{numbenchs} " +
              f"in {totTime}s")
        thisM = next(markers)
        aa0.plot(subdf.InstIdx, [max(1e-6, v) for v in subdf[bound]],
                 thisM,
                 label=f"{tool} {numbenchs} / {nInst}")
        if synthesis:
            aa1.plot(subdf.InstIdx, subdf.Synthesis_score,
                     thisM,
                     label=f"{tool} {numbenchs} / {nInst}")


    # Show the plot and close everything after
    if not synthesis:
        aa0.legend()
    aa0.set_yscale("log")
    aa0.set_ylabel(f"{bound} (s)")
    aa0.set_xlabel("benchmark nbr")

    if synthesis:
        aa1.legend()
        aa1.set_ylabel("Q scor")
        aa1.set_xlabel("benchmark nbr")

    ff.tight_layout()

    return ff, best

def genPlots(filename, families, track, exclude, synthesis, verbose,
             parallel, timeout, plotType):
    if synthesis:
        print("Checking quality ranking")
    else:
        print("Computing time-based statistics")
        if parallel:
            print("Using time bounds for PARALLEL tracks")
        else:
            print("Using time bounds for SEQUENTIAL tracks")

    # Loading the data in a pandas data frame
    df = pd.read_csv(filename)

    # Remove all entries without a valid output
    # Manual exclusion comes first, always
    print(f"Shape of raw input (not counting disqualified):\n{df.shape[0]}")
    print(f"Excluding {len(exclude)} pair ids")
    df = df[~df["pair id"].isin(exclude)]

    # If synthesis -> Check the MC result before doing anything else
    # We need to know if model check errors (not timeouts, errors as in it violates the spec) happened
    disqualified_solvers = set() # Disqualified solver configurations
    if synthesis:
        mcerrors = ["MC-ERROR"] # Unknown problem with model checking
        errors = df.loc[df.Model_check_result.isin(mcerrors) | df.result.isin(mcerrors), :]
        if errors.size:
            print("Unknown model checking error occurred -> Check and rerun or exclude the pairs")
            print(errors.to_csv(), file=sys.stderr)
            raise RuntimeError("Unknown errors detected: Abort!")

        mcincorrect = ["MC-INCORRECT"] # Provided controller is incorrect
        errors = df.loc[df.Model_check_result.isin(mcincorrect) | df.result.isin(mcincorrect), :]
        if errors.size:
            initial_jobs = df.shape[0]
            for (tool, config), subdf in errors.groupby([df.solver, df.configuration]):
                disqualified_solvers.add((tool, config))
                print(f"The tool {tool} in configuration {config} failed on instances\n {'; '.join(list(subdf.benchmark))}", file=sys.stderr)
            # Exclude the corresponding data
            for (tool, config) in disqualified_solvers:
                df = df[~((df.solver == tool) & (df.configuration == config))]
            print(f"A total of {initial_jobs - df.shape[0]} instances have been removed.", file = sys.stderr)
        else:
            print("No model checking problem detected")

    bound = "wallclock time" if parallel else "cpu time"
    possibleRes = ["REALIZABLE", "NEW-REALIZABLE"]
    if not synthesis:
        possibleRes.extend(["UNREALIZABLE", "NEW-UNREALIZABLE"])
    df = df[df.result.isin(possibleRes)]
    df = df[~df.status.isin(["timeout (cpu)", "timeout (wallclock)"])]
    if timeout is not None:
        df = df[df[bound] <= timeout]
    print(f"Shape after removing invalid output status and timeouts:\n{df.shape[0]}")

    # Checking for misclassifications
    # this only makes sense if there can be mismatching
    # classes (so not for synthesis)
    if not synthesis:
        for bench, subdf in df.groupby(df.benchmark):
            real = subdf[subdf.result.isin(["REALIZABLE",
                                            "NEW-REALIZABLE"])]
            unrl = subdf[subdf.result.isin(["UNREALIZABLE",
                                            "NEW-UNREALIZABLE"])]
            assert real.shape[0] == 0 or unrl.shape[0] == 0,\
                f"{bench} is misclassified by some tool!\n{real}\n{unrl}"
    else:  # for synthesis, check the model-checking result
        fail = df[df["Model_check_result"] != "SUCCESS"]
        if fail.size:
            raise RuntimeError(f"Model checking failed but tool/config was not excluded"\
                               f"or other untreated error in  {fail}")

    # Compute the scores
    # For synthesis, we compute the minimal size in the file per benchmark
    # as well as the total size (for convenience); then we also compute
    # the scores
    if synthesis:
        df = df.astype({"Synthesis_gates": "float64",
                        "Synthesis_latches": "float64"})
        df["Synthesis_total"] = (df["Synthesis_gates"] +
                                 df["Synthesis_latches"])
        df["Synthesis_ref"] =\
            df.groupby(df.benchmark)["Synthesis_total"].transform("min")
        df["Synthesis_score"] = df.apply(lambda row:
                                         getScore(row["Synthesis_total"],
                                                  row["Synthesis_ref"]),
                                         axis=1)
        if verbose:
            print(df.head())

    # Prepare to get information per tool
    participants = partPerTrack[track]

    # Extract and sort for each family, then plot
    allColumns = ["family", "nInst", "tool", "config", "nSolved", "time"]
    bestTable = pd.DataFrame(dict(zip(allColumns, [[] for _ in range(len(allColumns))] )))
    if synthesis:
        allColumns.append("Qscore")
        bestTable.insert(bestTable.shape[1], "Qscore", [])


    for name, inst_list in families.items():
        def find(val):
            try:
                return inst_list.index(val)
            except:
                return -1
        df["InstIdx"] = df.apply(lambda row: find(os.path.basename(row.benchmark)), axis=1)

        subdf = df.loc[df.InstIdx >= 0,:]
        subdf = subdf.sort_values("InstIdx")

        fig, best = doPlot(subdf, participants, len(inst_list), synthesis, bound)

        fig.savefig(os.path.join("images",
                                 f"{name.replace('/', '_')}_{parallel}_{synthesis}.{plotType}"))

        for tool, (numbenchs, totTime, config, subdf) in best.items():
            base = dict(zip(allColumns, len(allColumns)*[0]))
            base["family"] = name
            base["nInst"] = len(inst_list)
            base["tool"] = tool
            base["config"] = config
            base["nSolved"] = numbenchs
            base["time"] = totTime
            if synthesis:
                base["Qscore"] = subdf.Synthesis_score.sum()
            bestTable = bestTable.append(base, ignore_index=True)

    plt.show()
    return bestTable


def printTable(sumSeq, sumPar, synthesis):
    print("<table><tbody><tr>")

    if not synthesis:
        print("<th>Solver</th>"
              "<th>Configuration</th>"
              "<th>Solved (seq/par)</th>"
              "<th>CPU time (s)</th>"
              "<th>Wallclock time (s)</th></tr>")
        for (p, config) in sumSeq:
            (numseq, totSeq) = sumSeq[(p, config)]
            (numpar, totPar) = sumPar[(p, config)]
            print("<tr>")
            print(f"<td>{p}</td>"
                  f"<td>{config}</td>"
                  f"<td>{numseq}/{numpar}</td>"
                  f"<td>{totSeq:.2f}</td>"
                  f"<td>{totPar:.2f}</td>")
            print("</tr>")
    else:
        print("<th>Solver</th>"
              "<th>Config</th>"
              "<th>Solved (seq/par)</th>"
              "<th>CPU time (s)</th>"
              "<th>Wallclock (s)</th>"
              "<th>Score (seq/par)</th></tr>")
        for (p, config) in sumSeq:
            (numseq, totSeq, scoreSeq) = sumSeq[(p, config)]
            (numpar, totPar, scorePar) = sumPar[(p, config)]
            print("<tr>")
            print(f"<td>{p}</td>"
                  f"<td>{config}</td>"
                  f"<td>{numseq}/{numpar}</td>"
                  f"<td>{totSeq:.2f}</td>"
                  f"<td>{totPar:.2f}</td>"
                  f"<td>{scoreSeq:.2f}/{scorePar:.2f}</td>")
            print("</tr>")

    print("</tbody></table>")


parser = argparse.ArgumentParser(description="Summarize the job "
                                             "information from "
                                             "StarExec")
parser.add_argument("track", type=str,
                    choices=partPerTrack.keys())
parser.add_argument("sxdata",
                    help="Full path to the StarExec "
                         "job info file")
parser.add_argument("families", help="Full path to the "\
                    "json file describing the families.")
parser.add_argument("--timeout", type=int,
                    help="Timeout, in seconds, for objective "
                         "sequential track comparison")
parser.add_argument("--verbose", action="store_true",
                    help="Print more information messages "
                         "and dump intermediate csv files")
parser.add_argument("--expairs", type=int, nargs='*',
                    metavar="PID", default=[],
                    help="pair ids you wish to exclude")
parser.add_argument("--synthesis", action="store_true",
                    help="Compute synthesis quality ranking")
parser.add_argument("-s", "--save", type=str, default="png",
                    help="File format of generated plots, defaults to png")

if __name__ == "__main__":
    args = parser.parse_args()

    with open(args.families, "r") as fp:
        families = json.load(fp)

    summarySeq = genPlots(args.sxdata, families, args.track, args.expairs,
                           args.synthesis, args.verbose, False,
                           args.timeout, args.save)
    summarySeq.to_html("images/seq.html")
    #summaryPar = genPlots(args.sxdata, families, args.track, args.expairs,
    #                       args.synthesis, args.verbose, True,
    #                       args.timeout, args.save)
    #printTable(summarySeq, summaryPar, args.synthesis)
    exit(0)
