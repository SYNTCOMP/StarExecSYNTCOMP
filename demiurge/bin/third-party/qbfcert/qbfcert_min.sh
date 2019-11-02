#! /bin/bash

# This is a reduced version of the script qbfcert.sh.
# We just removed all code (sanity checks with the SAT-solver, etc.)
# which we do not need.
# Our similifications are supposed to make the script faster and
# more readable.

pgm=$0
ABS_PATH=`dirname ${pgm}`
TOOLBASEDIR=$ABS_PATH

# tools
QDPLL="$TOOLBASEDIR/depqbf/depqbf"
QRPCHECK="$TOOLBASEDIR/qrpcheck/qrpcheck"
QRPCERT="$TOOLBASEDIR/qrpcert/qrpcert"

SAT=10
UNSAT=20

INPUT=$1

readonly formula=$INPUT
readonly proof=$INPUT.qrp
readonly core_proof=$INPUT.core.qrp
readonly certificate=$INPUT.aiger

function cleanup
{
  rm -f "$proof"
  rm -f "$core_proof"
}

function cleanup_and_exit
{
 cleanup;
 exit 1
}

trap 'cleanup; exit 1' SIGHUP SIGINT SIGTERM

cleanup; 

# 1: running the solver
  echo "Running depqbf ..."
  $QDPLL --dep-man=simple --trace=bqrp < "$formula" > "$proof"
  solver_result=$?
  echo "depqbf result: $solver_result"
  if [[ $solver_result != $SAT && $solver_result != $UNSAT ]]; then
    echo "depqbf error."
    cleanup_and_exit;
  fi

# 2: running qrpcheck
  echo "Running qrpcheck ..."
  if [[ $solver_result == $UNSAT ]]; then
    ${QRPCHECK} -p "$proof" > "$core_proof"
  elif [[ $solver_result == $SAT ]]; then
    ${QRPCHECK} -p -f "$formula" "$proof" > "$core_proof"
  fi
  qrpcheck_result=$?
  if [[ $qrpcheck_result != 0 ]]; then
    echo "qrpcheck error."
    cleanup_and_exit;
  fi

# 3: test qrpcert
    echo "Running qrpcert ..."
    ${QRPCERT} --aiger-ascii "$core_proof" > "$certificate"
    qrpcert_result=$?
    if [[ $qrpcert_result != 0 ]]; then
    echo "qrpcert error."
      cleanup_and_exit;
    fi

cleanup;

exit 0
