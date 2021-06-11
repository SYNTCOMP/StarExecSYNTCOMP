#!/usr/bin/env python3

"""
This script reads a StarExec job-information file given in CSV format and
holding information about a TLSF synthesis job and adds to it a column with
the minimal implementation (over values in the file! this ignores tags).  The
reference value includes the number of latches + no. of gates. It assumes
that all entries in the file correspond to correctly synthesized outputs (no
unreal entries). Additionally, it directly adds a score per entry based on the
new reference.

Author: Guillermo A. Perez @ UAntwerp 2020-
"""

import csv
import fileinput
import math
import os
import re
import sys


status_re = re.compile(r"^//STATUS\s*:\s*(.*)$")
refsize_re = re.compile(r"^//REF_SIZE\s*:\s*(.*)$")


def get_tags(filename):
    refsize = None
    f = open(filename, 'r')
    for line in f:
        res = status_re.match(line)
        if res is not None:
            assert(res.group(1).strip() == "realizable")
        res = refsize_re.match(line)
        if res is not None:
            refsize = int(res.group(1).strip())
            break
    f.close()
    return refsize


def add_tags(filename, new_min):
    f = open(filename, 'a')
    f.write("\n//#!SYNTCOMP")
    f.write("\n//STATUS : realizable")
    f.write(f"\n//REF_SIZE : {new_min}")
    f.write("\n//#.")
    f.close()


def upd_tags(filename, new_min):
    with fileinput.FileInput(filename, inplace=True, backup='.bak') as f:
        for line in f:
            res = refsize_re.match(line)
            if res is not None:
                print(f"//REF_SIZE : {new_min}")
            else:
                print(line, end='')


def parse_csv(filename, bench_root):
    with open(filename) as csv_file:
        csv_reader = csv.reader(csv_file, delimiter=',')
        line_count = 0
        min_ref = dict()
        for row in csv_reader:
            if line_count == 0:
                cols = dict(zip(row, range(len(row))))
            else:
                bench_name = row[cols["benchmark"]]
                bench_name = bench_name[bench_name.rfind('/')+1:]
                out_by_ref = row[cols["Output_by_reference"]]
                diff_ref = 0 if out_by_ref == "-" else int(out_by_ref)
                new_size = int(row[14]) + int(row[18])
                init_ref = -1 * (diff_ref - new_size)
                if bench_name not in min_ref:
                    if init_ref in [-1, 0]:
                        min_ref[bench_name] = new_size
                    else:
                        min_ref[bench_name] = min(new_size, init_ref)
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
            else:
                bench_name = row[1][row[1].rfind('/')+1:]
                # below I load the sizes + 1 to avoid division by 0
                # for stupid benchmarks which require no gates
                # and no latches in the solution
                new_size = float(int(row[14]) + int(row[18]) + 1)
                ref_size = float(min_ref[bench_name] + 1)
                score = 2.0 - math.log(new_size / ref_size, 10)
                capped_score = max(0.0, score)
                print(f"{','.join(row)},{min_ref[bench_name]},{capped_score}")
            line_count += 1

    # We now explore the subdirectories to find benchmark files and update
    # their tags
    if bench_root is not None:
        print(f"Starting benchmark-tag update from {bench_root}",
              file=sys.stderr)
        for root, dirs, files in os.walk(bench_root):
            for name in files:
                if name in min_ref:
                    full_name = os.path.join(root, name)
                    print(f"Found benchmark {full_name}", file=sys.stderr)
                    ref_size = get_tags(full_name)
                    if ref_size is None:
                        print("Adding tags for the first time",
                              file=sys.stderr)
                        add_tags(full_name, min_ref[name])
                    elif ref_size > min_ref[name]:
                        print("Updating tag",
                              file=sys.stderr)
                        upd_tags(full_name, min_ref[name])

    return 0


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Too few arguments, expected the full path for the "
              + "input file at least", file=sys.stderr)
        exit(1)
    if len(sys.argv) > 3:
        print("Too many arguments, expected the input-file path and "
              + "(optionally) a folder path to update tags", file=sys.stderr)
        exit(1)
    if len(sys.argv) == 3:
        exit(parse_csv(sys.argv[1], sys.argv[2]))
    else:
        exit(parse_csv(sys.argv[1], None))
