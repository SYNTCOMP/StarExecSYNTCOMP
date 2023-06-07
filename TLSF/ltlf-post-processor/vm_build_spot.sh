#!/bin/bash
DIR=$( cd "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )

SPOTV="spot-2.11.5"

cd "${DIR}/source"

tar -zxvf "${SPOTV}.tar.gz"

mv ${SPOTV} spot

cd spot

scl enable devtoolset-8 './configure --disable-devel --disable-shared --enable-static'
scl enable devtoolset-8 'make -j 4'
scl enable devtoolset-8 'sudo make install'
