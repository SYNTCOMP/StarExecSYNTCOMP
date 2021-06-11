#!/usr/bin/env python3

"""
This script reads a StarExec job-information file given in CSV format and
holding information about a TLSF realizability jobs and collects useful
statistics.

Author: Guillermo A. Perez @ UAntwerp 2021
"""

import csv
import sys


def parse_csv(filename, bench_root):
    with open(filename) as csv_file:
        csv_reader = csv.reader(csv_file, delimiter=',')
        line_count = 0
        solve_count = dict()
        solved_by = dict()
        for row in csv_reader:
            if line_count == 0:
                cols = dict(zip(row, range(len(row))))
            else:
                config_name = row[cols["solver"]] + "-" +\
                    row[cols["configuration"]]
                bench_name = row[cols["benchmark"]]
                if config_name not in solve_count:
                    solve_count[config_name] = 0
                if bench_name not in solved_by:
                    solved_by[bench_name] = []
                solve_count[config_name] += 1
                solved_by[bench_name].append(row[cols["solver"]] + "-" +
                                             row[cols["configuration"]])
            line_count += 1

    print("Printing statistics:")
    unique_solve = [(itm[1][0], itm[0]) for itm in solved_by.items()
                    if len(itm[1]) == 1]
    for config, count in sorted(solve_count.items(),
                                key=lambda itm: itm[1],
                                reverse=True):
        print(f"{config} solved {count} benchmarks")
        unique = [itm[1] for itm in unique_solve
                  if itm[0] == config]
        if len(unique) > 0:
            print(f"{config} uniquely solved {len(unique)} benchs: {unique}")

    return 0


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Expected the full path for the "
              + "input file", file=sys.stderr)
        exit(1)
    else:
        exit(parse_csv(sys.argv[1], None))
