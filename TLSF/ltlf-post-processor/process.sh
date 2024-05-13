#!/bin/bash
# AUTHORS: Guillermo A. Perez + Philipp Meyer + Philipp Schlehuber-Caissier
# DESCRIPTION: The post-processor for synthesis tools on .tlsf benchmarks
#              (Based on Jens Kreber's script) in the LTLf track.
#              This version of the post-processor uses Spot
#              to model check synthesized controllers.
# arg1 = the solver's output (which StarExec saves to a file!, so this is a
#        a file path too)
# arg2 = the absolute path to the benchmark file
# arg3 = the path to the "permanent directory" associated to the pair

syntffull="$1"
origf="$2"
permdir="$3"

syntbasef="$(basename -- $syntffull)"

syntf="nocomments_$syntbasef" # We do not need to keep this
syntcommentf="$permdir/comments_$syntbasef" # We want to keep this

if [ ! -f "$origf" ]; then
    echo "Error=No input file found!"
    exit
fi

if [ ! -f "$syntffull" ]; then
    echo "Error=No output file found!"
    exit
fi

if [ ! -d "$permdir" ]; then
    echo "Error=Syntcomp outputfolder does not exist"
    exit
fi

# Extension to output format:
# A comment always starts with "c " (like in dimacs)
# and can be either a starexec-value like
# c Algo1_subtime=11.1
# So a key=value pair. key must start with a capital letter (I guess?)
# keys and values are restricted to 128 characters
# Or simple strings
# c YourCommentAsString
# without a character limitation but which must NOT contain a "="
# Starexec-values will be shown in the job-output
# Basic comments are (without the " c"-prefix) copied
# into the job-output

# File without comment but no stamp
grep -v $'[^\t]*\tc .*' "$syntffull" | sed 's/^[^\t]*\t//' > "$syntf"

# Bare comments without stamp and comment marker
grep $'[^\t]*\tc .*' "$syntffull" | sed 's/^[^\t]*\tc //' | grep -v "=" > "$syntcommentf"

#Keyword comments -> echoed to console
grep $'[^\t]*\tc .*' "$syntffull" | sed 's/^[^\t]*\tc //' | grep "="

#Launch the actual checking done in python
python3 ltlf_check.py "$syntf" "$origf"

exit $?