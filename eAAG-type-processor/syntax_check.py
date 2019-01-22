#!/usr/bin/env python2.7

import argparse
import sys
from argparse import FileType


def parse_header(spec_lines):
    #      M I L O A
    # aag 25 6 0 1 19
    #  0  1  2 3 4 5
    header_tokens = spec_lines[0].strip().split()
    nof_inputs = int(header_tokens[2])
    nof_outputs = int(header_tokens[4])
    return nof_inputs, nof_outputs


def get_inputs(spec_lines):
    nof_inputs, _ = parse_header(spec_lines)
    input_lines = spec_lines[1:nof_inputs+1]
    return set(int(l.strip()) for l in input_lines)


def is_input_symbol_table(l):
    # i0 i_1
    if l.strip().startswith('i'):
        tokens = l.split()
        wo_i = tokens[0][1:]
        try:
            int(wo_i)
            return True
        except ValueError:
            return False
    return False


def get_input_symbols(spec_lines):
    start = None
    end = None
    for i, l in enumerate(spec_lines):
        if l.strip()[0] == 'i' and not start:
            start = i
        if l.strip() == 'c':
            end = i
            break

    symbol_table_lines = spec_lines[start:end]
    symbol_table = [l for l in symbol_table_lines if is_input_symbol_table(l)]
    return symbol_table


def get_control_inputs(orig_spec_lines):
    control_inputs = set()
    input_symbols = get_input_symbols(orig_spec_lines)
    for s in input_symbols:
        # i0 i_1
        # i1 controllable_1
        tokens = s.strip().split()
        if tokens[1].startswith('controllable'):
            input_index = int(tokens[0][1:])
            input_literal = int(orig_spec_lines[1+input_index])
            control_inputs.add(input_literal)
    return control_inputs


def get_index_of_last_definition(spec_lines):
    for i, l in enumerate(spec_lines):
        if l.strip().startswith('i'):
            return i
    assert 0


def get_non_control_definitions(control_inputs, spec_lines):
    # TODO: what happens if some signal is short-circuited?
    non_control_definitions = set()

    nof_inputs, nof_outputs = parse_header(spec_lines)
    definitions_only = spec_lines[(nof_inputs + nof_outputs):
                                  get_index_of_last_definition(spec_lines)]
    for d in definitions_only:
        int_tokens = set(int(x) for x in d.strip().split())
        if int_tokens.isdisjoint(control_inputs):
            non_control_definitions.add(d)
    return non_control_definitions


def main(original_lines):
    orig_all_inputs = get_inputs(original_lines)
    orig_control_inputs = get_control_inputs(original_lines)
    orig_non_control_inputs = orig_all_inputs.difference(orig_control_inputs)

    assert orig_non_control_inputs, \
        'There are no uncontrollable inputs!'
    assert orig_control_inputs, \
        'There are no controllable inputs!'


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('original', type=FileType())
    args = parser.parse_args(sys.argv[1:])
    main(list(args.original.readlines()))
