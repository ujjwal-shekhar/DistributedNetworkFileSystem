#!/bin/bash

cd "$(dirname "$0")"

# Explicitly set the compiler to g++-16
/snap/bin/cmake -S . -B build -G Ninja \
    -DCMAKE_CXX_COMPILER=g++-16 \
    -DCMAKE_CXX_SCAN_FOR_MODULES=ON

/snap/bin/cmake --build build