# Reproducing the rankings and graphs
Most of the work is done using the `inforeal.py` Python script.

## LTL Realizability
The following can be executed to obtain the graphs from the StarExec
information and output files for the corresponding job. 

`python3 inforeal.py LTLreal file.csv --timeout 1800 --force`

Note the `--force` options which helps to ignore tools that panic and
(spuriously) declare a benchmark to be (UN)REALIZABLE after the tool faces an
internal error.

## Parity game Realizability
`python3 inforeal.py PGreal file.csv --timeout 1800`

## LTLf Realizability

`python3 inforeal.py LTLFreal file.csv --timeout 1800 --force`
