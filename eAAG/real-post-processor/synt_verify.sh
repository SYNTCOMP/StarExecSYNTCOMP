#!/bin/bash

# Written by Jens Kreber
# Version 1.1.1

#export dir
dir=$(dirname $0)

function verify () {


# tuning
ALLOW_NO_TAG=y   # will not exit if no syntcomp tag is found
#


NO_OUTPUT=-401
SYNTAX_ERROR=-403
NOT_AIGER=-402
UNREALIZABLE=15
UNREALIZABLE_NOT_RECOGNIZED=-500
REALIZABLE_NOT_RECOGNIZED=-501
MODEL_CHECKING_PASSED=1
MODEL_CHECKING_FAILED=-1
MODEL_CHECK_TIMEOUT=-24
REALIZABLE=16
MAYBE_REAL=18
MAYBE_UNREAL=17
SEEMS_LIKE_PASSED=19
VERIFIER_ERROR=-25

modelchecking_time=3600
origf="$1"
syntf="$2"
do_synt="$3"

if [ ! "$#" -gt 0 ]; then
    echo "Usage: $0 <original aiger file> <output of solver> [<arbitrary string to enable synthesis mode>]"
    exit $VERIFIER_ERROR
fi

#echo "Call dir: ${dir}"
#echo "Current dir: $(pwd)"
echo -n "SyntVerify - Running in "
if [ -n "$do_synt" ]; then
    echo "synthesis mode."
else
    echo "realizability mode."
fi

echo "Instance file: $origf"
echo -e "Solver output file: ${syntf}\n"


if [ ! -f "$syntf" ]; then
    echo -e -n "No output file found!\n\n$NO_OUTPUT"
    exit $NO_OUTPUT
fi

if [ -n "$VERIFIER_EXECUTABLES" ]; then
    modelchecker="$VERIFIER_EXECUTABLES/iimc"
    syntchecker="$VERIFIER_EXECUTABLES/syntactic_checker.py"
else
    modelchecker="$dir/iimc"
    syntchecker="$dir/syntactic_checker.py"
fi

syntline=$(grep -n "^#!SYNTCOMP" "$origf" | head -n 1 | cut -d ':' -f 1)
if [ -z "$syntline" ]; then
    if [ -z $ALLOW_NO_TAG ]; then
	echo -e -n "Error: The given input file has no SYNTCOMP tag!\n\n$VERIFIER_ERROR"
	exit $VERIFIER_ERROR
    else
        echo "Warning: No SYNTCOMP tag found in the file! Will assume status \"unknown\" for further processing."
	status="unknown"
    fi
else
    synttag=$(tail -n +$syntline "$origf")
    status=$(grep "STATUS[[:space:]]*:" <<< "$synttag" | cut -d ':' -f 2 | sed "s/ //g")
fi

# Check realizability
if (grep -i -q "^UNREALIZABLE" "$syntf"); then
    case "$status" in
	"realizable")
	    echo -e -n "Realizable not recognized\n\n$REALIZABLE_NOT_RECOGNIZED"
	    exit $REALIZABLE_NOT_RECOGNIZED
	    ;;
	"unrealizable")
	    echo -e -n "Unrealizable correct\n\n$UNREALIZABLE"
	    exit $UNREALIZABLE
	    ;;
	"unknown")
	    echo -e -n "Status is unknown, the output was UNREALIZABLE\n\n$MAYBE_UNREAL"
	    exit $MAYBE_UNREAL
	    ;;
	*)
	    echo -e -n "Error: Incorrect status value.\n\n$VERIFIER_ERROR"
	    exit $VERIFIER_ERROR
    esac
elif (grep -i -q "^REALIZABLE" "$syntf"); then
    case "$status" in
	"realizable")
	    if [ -z "$do_synt" ]; then
		echo -e -n "Realizable correct\n\n$REALIZABLE"
		exit $REALIZABLE
	    fi
	    ;;
	"unrealizable")
	    echo -e -n "Unrealizable not recognized\n\n$UNREALIZABLE_NOT_RECOGNIZED"
	    exit $UNREALIZABLE_NOT_RECOGNIZED
	    ;;
	"unknown")
	    if [ -z "$do_synt" ]; then
		echo -e -n "Status is unknown, the output was REALIZABLE\n\n$MAYBE_REAL"
		exit $MAYBE_REAL
	    fi
	    ;;
	*)
	    echo -e -n "Error: Incorrect status value.\n\n$VERIFIER_ERROR"
	    exit $VERIFIER_ERROR
    esac
fi

if [ -z "$do_synt" ]; then
    echo -e -n "Found nor REALIZABLE nor UNREALIZABLE\n\n$NO_OUTPUT"
    exit $NO_OUTPUT
fi

# Check synt
aagline=$(grep -n "^aag " "$syntf" | head -n 1 | cut -d ':' -f 1)
if [ -z "$aagline" ]; then
    echo -e -n "Could not find aag header.\n\n$NOT_AIGER"
    exit $NOT_AIGER
fi
tail -n +"$aagline" "$syntf" > "${syntf}.aag"
echo -n "Syntactic check.. "
python "$syntchecker" "$origf" "${syntf}.aag"
res=$?
if [ "$res" -eq 0 ]; then
    echo "passed!"
else
    echo -e -n "failed! Output file is NOT OK syntactically.\n\n$SYNTAX_ERROR"
    exit $SYNTAX_ERROR
fi

# Model checking
ulimit -t "$modelchecking_time"
check_res=$($modelchecker "${syntf}.aag")
res_val=$?
check_res_last=$(tail -n 1 <<< "$check_res")
if [[ "$check_res_last" =~ ^0$ ]];  then
    :
elif [[ $res_val == 137 || $res_val == 152 || $res_val == 143 ]]; then  # Killed or stopped
    echo -e -n "Model checking timed out or process was killed somehow!\n\n$MODEL_CHECK_TIMEOUT"
    exit $MODEL_CHECK_TIMEOUT
else
    echo -e -n "Model-checking the resulting circuit failed! iimc output: \n$check_res\n\n$MODEL_CHECKING_FAILED"
    exit $MODEL_CHECKING_FAILED
fi

# Determining circuit size
size_orig=$(head -n 1 "$origf" | cut -d ' ' -f 6)
size_synt=$(head -n 1 "${syntf}.aag" | cut -d ' ' -f 6)

echo -e -n "Input circuit size: $size_orig\nOutput circuit size: $size_synt\n"

size_ref=$(grep "REF_SIZE[[:space:]]*:" <<< "$synttag" | cut -d ':' -f 2 | sed "s/ //g")


if [ -n "$size_ref" ]; then
    diff_ref=$((size_synt - size_ref))
    if [ ! "$size_ref" -eq 0 ]; then
	outbyref=$(echo -e "scale=5\n$size_synt / $size_ref\n" | bc -l | sed 's/^\./0./')
    fi
    echo -e -n "Reference circuit size: $size_ref\nDifference to reference: $diff_ref\nOutput by reference: $outbyref\n"
fi

echo -e -n "\n"

if [ "$status" == "unknown" ]; then
    echo -e -n "This is nice. Status was unknown before but this tool passed the model checking with a valid result. Seems like it is realizable.\n\n$SEEMS_LIKE_PASSED"
    exit $SEEMS_LIKE_PASSED
else
    echo -e -n "Model-checking passed!\n\n$MODEL_CHECKING_PASSED"
    exit $MODEL_CHECKING_PASSED
fi

}

verify "$@" 2>&1
