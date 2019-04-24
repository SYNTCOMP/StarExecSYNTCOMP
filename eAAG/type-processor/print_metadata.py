#!/usr/bin/env python2.7

import argparse
import sys
from argparse import FileType


def main(original_lines):
    in_comment = False
    for l in original_lines:
        if '#!SYNTCOMP' in l:
            in_comment = True
            continue
        elif '#.' in l:
            in_comment = False
            continue
        # handle the comments then
        if in_comment:
            key, val = l.strip().split(':')
            key = key.strip()
            val = val.strip()
            val = (val[:124] + '...') if len(val) > 128 else val
            print(key + '=' + val)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('original', type=FileType())
    args = parser.parse_args(sys.argv[1:])
    main(list(args.original.readlines()))
