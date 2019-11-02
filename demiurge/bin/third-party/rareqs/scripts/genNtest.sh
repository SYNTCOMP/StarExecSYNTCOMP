#!/bin/bash
#
# File:  genNtest.sh
# Author:  mikolas
# Created on:  Fri Apr 5 12:31:58 WEST 2013
# Copyright (C) 2013, Mikolas Janota
#
if [[ $# != 3 ]] ; then
    echo "Usage: $0 <solver1> <solver2> <iters>"
    exit 100;
fi
if  ! ( which `cut <<<$1 -f1 -d\ ` >/dev/null 2>&1 ) ; then
    echo "Looks like $1 can't be run."
    exit 300
fi
if  ! ( which `cut <<<$2 -f1 -d\ ` >/dev/null 2>&1 ) ; then
    echo "Looks like $2 can't be run."
    exit 300
fi
mkdir -p bugs
S1=$1
S2=$2
N=$3
TOTAL_RES=0
while [[ $N > 0 ]]; do
  echo $N
  let N="${N} - 1"
  STAMP=`date +%s`_${RANDOM}
  I=/tmp/gNt_${STAMP}
  ./rqbf_nu.py 0.25 0.25 60 120 >$I  
  chmod -w $I
  echo $STAMP
  $S1 $I
  R1=$?
  $S2 $I
  R2=$?
  chmod u+w $I
  if [ $R1 -eq $R2 ]; then
   rm -f $I
  else
    mv -v $I ./bugs/bug_${STAMP}.qdimacs
    chmod -w ./bugs/bug_${STAMP}.qdimacs
    echo 'FAIL'
    let TOTAL_RES="${TOTAL_RES}+1"
  fi
done
if [ $TOTAL_RES -eq 0 ]; then
  echo 'OK'
else
  echo 'FAIL', $TOTAL_RES
fi
exit $TOTAL_RES
