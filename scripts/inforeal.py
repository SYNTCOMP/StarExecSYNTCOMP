#!/usr/bin/env python3
"""
This script generates the cactusplots for some tracks of
SYNTCOMP, along with rankings and other useful information
NOTE: It focuses on time, not on quality

Guillermo A. Perez @ UAntwerp, 2022
"""

import argparse
import itertools
import matplotlib.pyplot as plt
import pandas as pd


participantsPGreal = {
    "Strix PGAME": "Strix",
    "Knor repair": "Knor",
    "ltlsynt": "ltlsynt",
}


participantsLTLreal = {
    "Strix PGAME": "Strix",
    "ltlsynt23": "ltlsynt",
    "Otus": "Otus",
    "sdf": "sdf",
    "SPORE": "SPORE",
    "Strix": "Strix",
    "abonsai": "Acacia bonsai"
}


def genCactus(filename, track, parallel=True):
    bound = "wallclock time" if parallel else "cpu time"
    if parallel:
        print("Using time bounds for PARALLEL tracks")
    else:
        print("Using time bounds for SEQUENTIAL tracks")
    # Loading the data in a pandas data frame
    df = pd.read_csv(filename)

    # Remove all entries without a valid output
    print("Shape of raw input:")
    print(df.shape[0])
    df = df[df.result.isin(["REALIZABLE", "NEW-REALIZABLE",
                            "UNREALIZABLE", "NEW-UNREALIZABLE"])]
    df = df[~df.status.isin(["timeout (cpu)", "timeout (wallclock)"])]
    print("Shape after removing invalid output status and timeouts")
    print(df.shape[0])

    # Prepare to get the best configuration per tool
    if track == "PGreal":
        participants = participantsPGreal
    elif track == "LTLreal":
        participants = participantsLTLreal
    else:
        assert False
    best = {}
    summary = {}

    # Group them by tool configuration for the rest
    for config, subdf in df.groupby(df.configuration):
        found = False
        for p in participants:
            tool = subdf.head(1).solver.iloc[0]
            if tool.lower().startswith(p.lower()):
                found = True
                subdf = subdf.sort_values(by=bound)
                cumsum = subdf[bound].cumsum()
                numbenchs = cumsum.shape[0]
                # store the summary
                summary[(p, config)] = (numbenchs, cumsum.iloc[-1])
                # store the info of the best configs
                if (p not in best or numbenchs > best[p][0]
                    or (numbenchs == best[p][0] and
                        cumsum.iloc[-1] < best[p][1].iloc[-1])):
                    best[p] = (numbenchs, cumsum)
                print(f"Tool {p}, Configuration {config} solved " +
                      f"{numbenchs} benchs " +
                      f"in {cumsum.iloc[-1]}s")
                break
        assert found

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


def printTable(sumSeq, sumPar):
    print("<table><tbody><tr>"
          "<th>Solver</th>"
          "<th>Configuration</th>"
          "<th>Solved benchmarksr</th>"
          "<th>Total CPU time (s)</th>"
          "<th>Total Wallclock time (s)</th></tr>")
    for (p, config) in sumSeq:
        (numbenchs, totSeq) = sumSeq[(p, config)]
        (_, totPar) = sumPar[(p, config)]
        print("<tr>")
        print(f"<td>{p}</td>"
              f"<td>{config}</td>"
              f"<td>{numbenchs}</td>"
              f"<td>{totSeq:.2f}</td>"
              f"<td>{totPar:.2f}</td>")
        print("</tr>")
        pass
    print("</tbody></table>")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Summarize the job "
                                                 "information from "
                                                 "StarExec")
    parser.add_argument("track",
                        help="Track type, e.g. PGreal, "
                             "LTLreal, ...")
    parser.add_argument("sxdata",
                        help="Full path to the StarExec "
                             "job info file")
    args = parser.parse_args()
    summarySeq = genCactus(args.sxdata, args.track, False)
    summaryPar = genCactus(args.sxdata, args.track, True)
    printTable(summarySeq, summaryPar)
    exit(0)
