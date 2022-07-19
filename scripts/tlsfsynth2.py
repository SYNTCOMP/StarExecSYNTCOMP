#!/usr/bin/env python3
"""
This script generates the cactus plot for the results of the TLSF synthesis
track

Guillermo A. Perez @ UAntwerp, 2022
"""


import itertools
import matplotlib.pyplot as plt
import pandas as pd
import sys


def genCactus(fname):
    # Loading the data in a pandas data frame
    df = pd.read_csv(fname)

    # Remove all entries without model checking having passed
    df = df[df.result.isin(["REALIZABLE", "NEW-REALIZABLE"])]

    # Prepare to get the best configuration per tool
    participants = [
        "ltlsynt",
        "Strix",
        "sdf",
        "Otus"
    ]

    best = {}

    # Group them by tool configuration for the rest
    for config, subdf in df.groupby(df.configuration):
        found = False
        for p in participants:
            tool = subdf.head(1).solver.iloc[0]
            if tool.lower().startswith(p.lower()):
                found = True
                subdf = subdf.sort_values(by="Score")
                cumsum = subdf.Score.cumsum()
                total = cumsum.iat[-1]
                if p not in best or total > best[p][0]:
                    best[p] = (total, cumsum)
                break
        assert(found)
        print(f"Tool {p}, Configuration {config} got " +
              f"{cumsum.iloc[-1]} points from " +
              f"{cumsum.shape[0]} benchs")

    # Show best plot per tool
    final = {
        "Strix": "Strix",
        "ltlsynt": "ltlsynt",
        "sdf": "sdf",
        "Otus": "Otus"
    }
    markers = itertools.cycle(('h', '+', '.', 'o', '*', 'D', 's'))
    for tool in best:
        (_, cumsum) = best[tool]
        print(f"The best config of {final[tool]} got " +
              f"{cumsum.iloc[-1]} points from " +
              f"{cumsum.shape[0]} benchs")
        plt.plot(range(1, cumsum.shape[0] + 1), cumsum, label=final[tool],
                 marker=next(markers))

    # Show the plot and close everything after
    plt.legend(loc="lower right")
    plt.ylabel("Total score")
    plt.xlabel("No. of solved benchmarks")
    plt.show()
    plt.close()


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Expected the full path for the "
              + "input file", file=sys.stderr)
        exit(1)
    else:
        genCactus(sys.argv[1])
        exit(0)
