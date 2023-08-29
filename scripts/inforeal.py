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
import sys


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
        "abonsai": "Acacia bonsai",
        "ab23": "A. bonsai 2023",
        "ab-kdt": "AB with KDTrees",
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


def getScore(syntTotal, syntRef):
    assert syntTotal >= syntRef
    return max(0.0, 2.0 - math.log((syntTotal + 1.0) / (syntRef + 1.0), 10))


def genPlots(filename, track, exclude, synthesis, verbose,
             parallel, timeout, force, scatter):
    if synthesis:
        print("Checking quality ranking")
    else:
        print("Computing time-based statistics")
        if parallel:
            print("Using time bounds for PARALLEL tracks")
        else:
            print("Using time bounds for SEQUENTIAL tracks")

    markers = itertools.cycle(('h', '+', '.', 'o', '*',
                               'D', 's', '^', '2', '|'))

    # Loading the data in a pandas data frame
    df = pd.read_csv(filename)

    # If synthesis -> Check the MC result before doing anything else We need
    # to know if model check errors (not timeouts, errors as in it violates
    # the spec) happened
    disqualified_solvers = set()  # Disqualified solver configurations
    if synthesis:
        mcerrors = ["MC-INCORRECT", "MC-ERROR"]
        errors = df.loc[df.Model_check_result.isin(mcerrors) |
                        df.result.isin(mcerrors), :]
        if errors.size:
            initial_jobs = df.shape[0]
            for (tool, config), subdf in errors.groupby([df.solver,
                                                         df.configuration]):
                disqualified_solvers.add((tool, config))
                print(f"The tool {tool} in configuration {config} failed "
                      f"on instances\n {'; '.join(list(subdf.benchmark))}",
                      file=sys.stderr)
            # Exclude the corresponding data
            for (tool, config) in disqualified_solvers:
                df = df[~((df.solver == tool) & (df.configuration == config))]
            print(f"A total of {initial_jobs - df.shape[0]} "
                  "instances have been removed.", file=sys.stderr)
        else:
            print("No model checking error detected")

    # Remove all entries without a valid output
    bound = "wallclock time" if parallel else "cpu time"
    print(f"Shape of raw input (not counting disqualified):\n{df.shape[0]}")
    print(f"Excluding {len(exclude)} pair ids")
    df = df[~df["pair id"].isin(exclude)]
    possibleRes = ["REALIZABLE", "NEW-REALIZABLE"]
    if not synthesis:
        possibleRes.extend(["UNREALIZABLE", "NEW-UNREALIZABLE"])
    df = df[df.result.isin(possibleRes)]
    df = df[~df.status.isin(["timeout (cpu)", "timeout (wallclock)"])]
    if timeout is not None:
        df = df[df[bound] <= timeout]
    print("Shape after removing invalid output "
          f"status and timeouts:\n{df.shape[0]}")

    # Checking for misclassifications
    # this only makes sense if there can be mismatching
    # classes (so not for synthesis)
    if not synthesis:
        misclassed = []
        for bench, subdf in df.groupby(df.benchmark):
            real = subdf[subdf.result.isin(["REALIZABLE",
                                            "NEW-REALIZABLE"])]
            unrl = subdf[subdf.result.isin(["UNREALIZABLE",
                                            "NEW-UNREALIZABLE"])]
            if not (real.shape[0] == 0 or unrl.shape[0] == 0):
                print(f"{bench} is misclassified by some "
                      f"tool!\n{real}\n{unrl}")
                misclassed.append(bench)
                if not force:
                    exit(1)
        # If force is True, we could have misclassifications, let's remove
        # them to be fair
        df = df[~df.benchmark.isin(misclassed)]

    else:  # for synthesis, check the model-checking result
        fail = df[df["Model_check_result"] != "SUCCESS"]
        assert fail.shape[0] == 0, "Model checking failed but tool/config "\
                                   "was not excluded "\
                                   f"or other untreated error in {fail}"

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
    best = {}
    summary = {}

    # Group them by tool configuration now to find a best config per tool
    for (tool, config), subdf in df.groupby([df.solver, df.configuration]):
        assert (tool, config) not in disqualified_solvers,\
                "Table was not properly cleaned"

        tool_name = participants[tool.lower()]

        subdf = subdf.sort_values(by=bound)
        if verbose:
            subdf.to_csv(f"{tool_name}.{config}.csv")
        numbenchs = subdf.shape[0]
        cumsum = subdf[bound].cumsum()
        # store the summary
        if not synthesis:
            summary[(tool_name, config)] = (numbenchs, cumsum.iloc[-1])
        else:
            scoresum = subdf["Synthesis_score"].sum()
            summary[(tool_name, config)] = (numbenchs,
                                            cumsum.iloc[-1],
                                            scoresum)

        # store the info of the best configs
        if (tool_name not in best or numbenchs > best[tool_name][0]
            or (numbenchs == best[tool_name][0] and
                cumsum.iloc[-1] < best[tool_name][1].iloc[-1])):
            best[tool_name] = (numbenchs, cumsum)
        print(f"Tool {tool_name}, Configuration {config} solved " +
              f"{numbenchs} benchs " +
              f"in {cumsum.iloc[-1]}s")

    # Show best cactus plot per tool
    for tool, (_, cumsum) in best.items():
        print(f"The best config of {tool} solved "
              f"{cumsum.shape[0]} " +
              f"in {cumsum.iloc[-1]}s")
        plt.plot(range(1, cumsum.shape[0] + 1), cumsum,
                 label=tool,
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

    # If needed, create a scatter plot (this does not discriminate tools)
    if len(scatter) > 0:
        allconfs = list(map(lambda x: tuple(x.split(':')), scatter))
        (basetool, baseconfig) = allconfs[0]
        baseline = df[(df.solver == basetool) &
                      (df.configuration == baseconfig)]
        print("Preparing scatter plot")
        for (tool, config), subdf in df.groupby([df.solver, df.configuration]):
            if (tool, config) not in allconfs:
                continue

            values = subdf.merge(baseline, left_on="benchmark",
                                 right_on="benchmark")
            if verbose:
                values["diff_time"] = values.apply(lambda row:
                                                   row[bound + "_x"] -
                                                   row[bound + "_y"],
                                                   axis=1)
                values = values.sort_values(by="diff_time")
                values.to_csv(f"{tool}:{config}-{parallel}.csv")
            base = values[bound + "_y"]
            values = values[bound + "_x"]
            plt.scatter(base, values,
                        label=f"{tool}:{config}",
                        marker=next(markers))
        # Show the plot and close everything after
        plt.legend(loc="lower right")
        if parallel:
            plt.ylabel("Total wall-clock time (s)")
        else:
            plt.ylabel("Total cpu time (s)")
        plt.xlabel(f"Baseline {scatter[0]}")
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


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Summarize the job "
                                                 "information from "
                                                 "StarExec")
    parser.add_argument("track", type=str,
                        choices=partPerTrack.keys())
    parser.add_argument("sxdata",
                        help="Full path to the StarExec "
                             "job info file")
    parser.add_argument("--force", action="store_true",
                        help="Ignore misclassifications and "
                             "force output")
    parser.add_argument("--timeout", type=int,
                        help="Timeout, in seconds, for objective "
                             "sequential track comparison")
    parser.add_argument("--verbose", action="store_true",
                        help="Print more information messages "
                             "and dump intermediate csv files")
    parser.add_argument("--expairs", type=int, nargs='*',
                        metavar="PID", default=[],
                        help="pair ids you wish to exclude")
    parser.add_argument("--scatter", type=str, nargs='*',
                        metavar="CONF", default=[],
                        help="Generate a scatter plot for "
                             "the given tool:config comma "
                             "separated pairs (first one "
                             "is taken as baseline)")
    parser.add_argument("--synthesis", action="store_true",
                        help="Compute synthesis quality ranking")
    args = parser.parse_args()
    summarySeq = genPlots(args.sxdata, args.track, args.expairs,
                          args.synthesis, args.verbose, False,
                          args.timeout, args.force, args.scatter)
    summaryPar = genPlots(args.sxdata, args.track, args.expairs,
                          args.synthesis, args.verbose, True,
                          args.timeout, args.force, args.scatter)
    printTable(summarySeq, summaryPar, args.synthesis)
    exit(0)
