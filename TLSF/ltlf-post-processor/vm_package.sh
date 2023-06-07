#!/bin/bash

rm -rf postproc
mkdir postproc

for xx in syfco checkltlf process
do
    cp ${xx} ./postproc/
done

cd postproc
zip ltlfpostproc.zip ./*
