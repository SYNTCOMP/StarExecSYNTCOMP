#!/usr/bin/env python3

"""
This script reads a StarExec job-information file given in CSV format and
holding information about a PGAME synthesis job and adds to it a column with
the minimal implementation (over values in the file! this ignores tags).  The
reference value includes the number of latches + no. of gates. It assumes
that all entries in the file correspond to correctly synthesized outputs (no
unreal entries). Additionally, it directly adds a score per entry based on the
new reference.

Author: Guillermo A. Perez @ UAntwerp 2020-2021
"""

import csv
import math
import sys


def parse_csv(filename, bench_root):
    with open(filename) as csv_file:
        csv_reader = csv.reader(csv_file, delimiter=',')
        line_count = 0
        min_ref = dict()
        for row in csv_reader:
            if line_count == 0:
                cols = dict(zip(row, range(len(row))))
            elif row[cols["result"]] in ["REALIZABLE", "NEW-REALIZABLE"]:
                long_name = row[cols["benchmark"]]
                bench_name = long_name[long_name.rfind('/')+1:]
                new_size = int(row[cols["Synthesis_latches"]]) +\
                    int(row[cols["Synthesis_gates"]])
                if bench_name not in min_ref:
                    min_ref[bench_name] = new_size
                else:
                    min_ref[bench_name] = min(new_size, min_ref[bench_name])
                assert(min_ref[bench_name] >= 0)
            line_count += 1

    # We now re-traverse the thing to print out the new CSV file
    with open(filename) as csv_file:
        csv_reader = csv.reader(csv_file, delimiter=',')
        line_count = 0
        for row in csv_reader:
            if line_count == 0:
                print(f"{','.join(row)},Min_Ref,Score")
            elif row[cols["result"]] in ["REALIZABLE", "NEW-REALIZABLE"]:
                long_name = row[cols["benchmark"]]
                bench_name = long_name[long_name.rfind('/')+1:]
                # below I load the sizes + 1 to avoid division by 0
                # for stupid benchmarks which require no gates
                # and no latches in the solution
                new_size = int(row[cols["Synthesis_latches"]]) +\
                    int(row[cols["Synthesis_gates"]]) + 1
                ref_size = float(min_ref[bench_name] + 1)
                score = 2.0 - math.log(new_size / ref_size, 10)
                capped_score = max(0.0, score)
                print(f"{','.join(row)},{min_ref[bench_name]},{capped_score}")
            line_count += 1
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Expected the full path for the "
              + "input file", file=sys.stderr)
        exit(1)
    else:
        exit(parse_csv(sys.argv[1], None))
