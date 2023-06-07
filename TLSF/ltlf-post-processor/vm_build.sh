#!/bin/bash

scl enable devtoolset-8 'g++ -O3 -std=c++17 -o ./checkltlf ./source/checkltlf.cpp /usr/local/lib/libspot.a /usr/local/lib/libbddx.a'
