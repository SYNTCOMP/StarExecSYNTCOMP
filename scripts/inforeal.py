#!/usr/bin/env python3
"""
This script generates the cactusplots for some tracks of
SYNTCOMP, along with rankings and other useful information

Guillermo A. Perez @ UAntwerp, 2023
"""

import argparse
import itertools
import math
import matplotlib.pyplot as plt
import pandas as pd


partPerTrack = {
    "PGreal": {
        "Strix PGAME": "Strix",
        "Knor repair": "Knor",
        "ltlsynt": "ltlsynt"
    },
    "PGsynt": {
        "Strix PGAME": "Strix",
        "Knor repair": "Knor",
        "ltlsynt": "ltlsynt"
    },
    "LTLreal": {
        "Strix PGAME": "Strix",
        "ltlsynt23": "ltlsynt",
        "Otus": "Otus",
        "sdf": "sdf",
        "SPORE": "SPORE",
        "Strix": "Strix",
        "abonsai": "Acacia bonsai"
    },
    "LTLsynt": {
        "abonsai": "Acacia bonsai",
        "ltlsynt23": "ltlsynt",
        "Otus": "Otus",
        "sdf": "sdf",
        "Strix": "Strix"
    },
    "LTLFreal": {
        "lisa-syntcomp": "lisa",
        "ltlfsynt23prebuild": "ltlfsynt",
        "LydiaSyft": "LydiaSyft",
        "Nike": "Nike",
        "tople": "tople"
    }
}


def getScore(syntTotal, syntRef):
    assert syntTotal >= syntRef
    return max(0.0, 2.0 - math.log((syntTotal + 1.0) / (syntRef + 1.0), 10))


def genCactus(filename, track, exclude, synthesis, verbose,
              parallel, timeout):
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
    bound = "wallclock time" if parallel else "cpu time"
    print("Shape of raw input:")
    print(df.shape[0])
    print(f"Excluding {len(exclude)} pair ids")
    df = df[~df["pair id"].isin(exclude)]
    possibleRes = ["REALIZABLE", "NEW-REALIZABLE"]
    if not synthesis:
        possibleRes.extend(["UNREALIZABLE", "NEW-UNREALIZABLE"])
    df = df[df.result.isin(possibleRes)]
    df = df[~df.status.isin(["timeout (cpu)", "timeout (wallclock)"])]
    if timeout is not None:
        df = df[df[bound] <= timeout]
    print("Shape after removing invalid output status and timeouts")
    print(df.shape[0])

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
        assert fail.shape[0] == 0, f"Model checking failed {fail}"

    # Prepare to get information per tool
    participants = partPerTrack[track]
    best = {}
    summary = {}

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
        print(df.head())

    # Group them by tool configuration for the rest
    for (tool, config), subdf in df.groupby([df.solver, df.configuration]):
        found = False
        for p in participants:
            if tool.lower().startswith(p.lower()):
                found = True
                subdf = subdf.sort_values(by=bound)
                if verbose:
                    subdf.to_csv(f"{p}.{config}.csv")
                numbenchs = subdf.shape[0]
                cumsum = subdf[bound].cumsum()
                # store the summary
                if not synthesis:
                    summary[(p, config)] = (numbenchs, cumsum.iloc[-1])
                else:
                    scoresum = subdf["Synthesis_score"].sum()
                    summary[(p, config)] = (numbenchs, cumsum.iloc[-1],
                                            scoresum)

                # store the info of the best configs
                if (p not in best or numbenchs > best[p][0]
                    or (numbenchs == best[p][0] and
                        cumsum.iloc[-1] < best[p][1].iloc[-1])):
                    best[p] = (numbenchs, cumsum)
                print(f"Tool {p}, Configuration {config} solved " +
                      f"{numbenchs} benchs " +
                      f"in {cumsum.iloc[-1]}s")
                break
        assert found, f"Did not find tool {tool}"

    # Show best plot per tool
    markers = itertools.cycle(('h', '+', '.', 'o', '*', 'D', 's'))
    for tool in best:
        (_, cumsum) = best[tool]
        print(f"The best config of {participants[tool]} solved "
              f"{cumsum.shape[0]} " +
              f"in {cumsum.iloc[-1]}s")
        plt.plot(range(1, cumsum.shape[0] + 1), cumsum,
                 label=participants[tool],
                 marker=next(markers))

    # Show the plot and close everything after
    plt.legend(loc="lower right")
    plt.yscale("log")
    if parallel:
        plt.ylabel("Total wall-clock time (s)")
    else:
        plt.ylabel("Total cpu time (s)")
    plt.xlabel("No. of solved benchmarks")
    plt.show()
    plt.close()

    return summary


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
              "<th>Score</th></tr>")
        for (p, config) in sumSeq:
            (numseq, totSeq, score) = sumSeq[(p, config)]
            (numpar, totPar) = sumPar[(p, config)]
            print("<tr>")
            print(f"<td>{p}</td>"
                  f"<td>{config}</td>"
                  f"<td>{numseq}/{numpar}</td>"
                  f"<td>{totSeq:.2f}</td>"
                  f"<td>{totPar:.2f}</td>"
                  f"<td>{score:.2f}</td>")
            print("</tr>")

    print("</tbody></table>")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Summarize the job "
                                                 "information from "
                                                 "StarExec")
    parser.add_argument("track", type=str,
                        choices=partPerTrack.keys())
    parser.add_argument("sxdata",
                        help="Full path to the StarExec "
                             "job info file")
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
    args = parser.parse_args()
    summarySeq = genCactus(args.sxdata, args.track, args.expairs,
                           args.synthesis, args.verbose, False,
                           args.timeout)
    # Note that we only need the score once, so we can hardcode False
    # in the synthesis parameter next
    summaryPar = genCactus(args.sxdata, args.track, args.expairs,
                           False, args.verbose, True, args.timeout)
    printTable(summarySeq, summaryPar, args.synthesis)
    exit(0)
