#!/usr/bin/env python3

"""
This script reads a StarExec job-information file given in CSV format and
holding information about a TLSF synthesis job and updates the tags in the
benchmark files regarding the status of the benchmark and the size of the
minimal implementation. The reference value includes the number of latches +
no. of gates. (The CSV is assumed to contain only information about completed
jobs, so it is realizable or unrealizable.)

Author: Guillermo A. Perez @ UAntwerp 2020-2021
"""

import csv
import fileinput
import os
import re
import shutil
import sys
from tempfile import NamedTemporaryFile


status_re = re.compile(r"^//STATUS\s*:\s*(.*)$")
refsize_re = re.compile(r"^//REF_SIZE\s*:\s*(.*)$")


def get_tags(filename):
    refsize = None
    f = open(filename, 'r')
    for line in f:
        res = refsize_re.match(line)
        if res is not None:
            refsize = int(res.group(1).strip())
    f.close()
    return refsize


def add_tags(filename, new_min):
    status = "realizable" if new_min >= 0 else "unrealizable"
    f = open(filename, 'a')
    f.write("\n//#!SYNTCOMP")
    f.write(f"\n//STATUS : {status}")
    f.write(f"\n//REF_SIZE : {new_min}")
    f.write("\n//#.")
    f.close()


def upd_tags(filename, new_min):
    status = "realizable" if new_min >= 0 else "unrealizable"
    with fileinput.FileInput(filename, inplace=True, backup='.bak') as f:
        for line in f:
            res = refsize_re.match(line)
            if res is not None:
                print(f"//REF_SIZE : {new_min}")
                continue
            res = status_re.match(line)
            if res is not None:
                print(f"//STATUS : {status}")
                continue
            print(line, end='')


def upd_tags_csv(filename, base, min_ref):
    tempfile = NamedTemporaryFile("w+t", newline="", delete=False)

    with open(filename, "r", newline="") as csvfile, tempfile:
        reader = csv.reader(csvfile, delimiter=",", quotechar='"')
        writer = csv.writer(tempfile, delimiter=",", quotechar='"')
        rowno = 0
        for row in reader:
            if rowno == 0:
                header = row.copy()
                headerIdx = dict(zip(header, range(len(header))))
            else:
                vals = dict(zip(header, row))
                valkeys = [k for k in header
                           if k not in ["status", "refsize"]]
                bench = base +\
                    "_".join([vals[k] for k in sorted(valkeys)]) +\
                    ".tlsf"
                if bench in min_ref:
                    new_min = min_ref[bench]
                    print(f"Found benchmark {bench} in job log")
                    status = "realizable" if new_min >= 0 else "unrealizable"
                    row[headerIdx["status"]] = status
                    row[headerIdx["refsize"]] = str(new_min)
                else:
                    print(f"No data for {bench}", file=sys.stderr)
            writer.writerow(row)
            rowno += 1
    shutil.move(tempfile.name, filename)


def parse_csv(filename, bench_root):
    with open(filename) as csv_file:
        csv_reader = csv.reader(csv_file, delimiter=',')
        line_count = 0
        min_ref = dict()
        for row in csv_reader:
            if line_count == 0:
                pass
            elif "UNREAL" in row[11]:
                bench_name = row[1][row[1].rfind('/')+1:]
                min_ref[bench_name] = -1
            else:
                bench_name = row[1][row[1].rfind('/')+1:]
                diff_ref = 0 if row[17] == "-" else int(row[17])
                new_size = int(row[14]) + int(row[18])
                init_ref = -1 * (diff_ref - new_size)
                if bench_name not in min_ref:
                    min_ref[bench_name] = new_size
                    if init_ref not in [-1, 0]:
                        min_ref[bench_name] = min(new_size, init_ref)
                else:
                    min_ref[bench_name] = min(new_size, min_ref[bench_name])
                assert(min_ref[bench_name] >= 0)
            line_count += 1

    # We now explore the subdirectories to find benchmark files and update
    # their tags
    if bench_root is not None:
        print(f"Starting benchmark-tag update from {bench_root}")
        for root, _, files in os.walk(bench_root):
            for name in files:
                if name in min_ref:
                    full_name = os.path.join(root, name)
                    print(f"Found benchmark {name} in job log")
                    if get_tags(full_name) is None:
                        add_tags(full_name, min_ref[name])
                    else:
                        upd_tags(full_name, min_ref[name])
                elif name.endswith(".tlsf"):
                    base = name[:-5]
                    csvfname = os.path.join(root, base + ".csv")
                    if os.path.isfile(csvfname):
                        upd_tags_csv(csvfname, base, min_ref)
                    else:
                        print(f"No data and no csv file for {name}!",
                              file=sys.stderr)
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
