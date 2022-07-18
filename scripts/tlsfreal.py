#!/usr/bin/env python3
"""
This script generates the cactusplots for the TLSF tracks of
SYNTCOMP, along with rankings and other useful information
NOTE: It focuses on time, not on quality

Guillermo A. Perez @ UAntwerp, 2022
"""

import itertools
import matplotlib.pyplot as plt
import pandas as pd
import sys


def genCactus(filename, parallel=True):
    bound = "wallclock time" if parallel else "cpu time"
    if parallel:
        print("Using time bounds for PARALLEL configurations")
    else:
        print("Using time bounds for SEQUENTIAL configurations")
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
    participants = [
        "ltlsynt",
        "Otus",
        "sdf",
        "SPORE",
        "Strix",
        "_acab_sb"
    ]
    best = {}

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
                if (p not in best or numbenchs > best[p][0]
                    or (numbenchs == best[p][0] and
                        cumsum.iloc[-1] < best[p][1].iloc[-1])):
                    best[p] = (numbenchs, cumsum)
                break
        assert(found)
        print(f"Tool {p}, Configuration {config} solved " +
              f"{cumsum.shape[0]} benchs " +
              f"in {cumsum.iloc[-1]}s")

    # Show best plot per tool
    final = {
        "Strix PGAME": "Strix",
        "ltlsynt": "ltlsynt",
        "Otus": "Otus",
        "sdf": "sdf",
        "SPORE": "SPORE",
        "Strix": "Strix",
        "_acab_sb": "Acacia bonsai"
    }
    markers = itertools.cycle(('h', '+', '.', 'o', '*', 'D', 's'))
    for tool in best:
        (_, cumsum) = best[tool]
        print(f"The best config of {final[tool]} solved {cumsum.shape[0]} " +
              f"in {cumsum.iloc[-1]}s")
        plt.plot(range(1, cumsum.shape[0] + 1), cumsum, label=final[tool],
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


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Expected the full path for the "
              + "input file", file=sys.stderr)
        exit(1)
    else:
        genCactus(sys.argv[1], False)
        genCactus(sys.argv[1], True)
        exit(0)
