#!/usr/bin/env python2.7

import argparse
import sys
from argparse import FileType


def parse_header(spec_lines):
    #      M I L O A
    # aag 25 6 0 1 19
    #  0  1  2 3 4 5
    header_tokens = spec_lines[0].strip().split()
    M = int(header_tokens[1])
    nof_inputs = int(header_tokens[2])
    L = int(header_tokens[3])
    nof_outputs = int(header_tokens[4])
    A = int(header_tokens[5])
    assert M == L + nof_inputs + A, \
        'M is not the sum of I, L, A'
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


def get_uncontrol_definitions(control_inputs, spec_lines):
    # TODO: what happens if some signal is short-circuited?
    uncontrol_definitions = set()

    nof_inputs, nof_outputs = parse_header(spec_lines)
    definitions_only = spec_lines[(nof_inputs + nof_outputs):
                                  get_index_of_last_definition(spec_lines)]
    for d in definitions_only:
        int_tokens = set(int(x) for x in d.strip().split())
        if int_tokens.isdisjoint(control_inputs):
            uncontrol_definitions.add(d)
    return uncontrol_definitions


def check_valid_metadata(spec_lines):
    in_comment = False
    for l in spec_lines:
        if '#!SYNTCOMP' in l:
            assert not in_comment, 'Invalid nesting of metadata labels'
            in_comment = True
            continue
        elif '#.' in l:
            assert in_comment, 'Metadata end-label does not have a start'
            in_comment = False
            continue
    assert not in_comment, 'Metadata labels not closed'


def main(original_lines, synthesized_lines):
    check_valid_metadata(original_lines)
    orig_all_inputs = get_inputs(original_lines)
    orig_control_inputs = get_control_inputs(original_lines)
    orig_uncontrol_inputs = orig_all_inputs.difference(orig_control_inputs)
    onof_inputs, onof_outputs = parse_header(original_lines)

    assert onof_outputs == 1, \
        'More than one output defined!'
    assert orig_uncontrol_inputs, \
        'There are no uncontrollable inputs!'
    assert orig_control_inputs, \
        'There are no controllable inputs!'

    # loading information about the synthesis output
    check_valid_metadata(synthesized_lines)
    synthd_all_inputs = get_inputs(synthesized_lines)
    synthd_control_inputs = get_control_inputs(synthesized_lines)
    synthd_uncontrol_inputs = \
        synthd_all_inputs.difference(synthd_control_inputs)
    snof_inputs, snof_outputs = parse_header(synthesized_lines)

    assert snof_outputs == 1, \
        'More than one output defined!'
    assert synthd_uncontrol_inputs, \
        'There are no uncontrollable inputs!'
    assert len(synthd_uncontrol_inputs) == len(orig_uncontrol_inputs), \
        'The no. of uncontrollable inputs does not match after synthesis'
    assert synthd_control_inputs == 0, \
        'There are controllable inputs left!'


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('original', type=FileType())
    parser.add_argument('synthesized', type=FileType())
    args = parser.parse_args(sys.argv[1:])
    main(list(args.original.readlines()), list(args.synthesized.readlines()))
