#!/bin/bash

cd "$(dirname "$0")"

# Explicitly set the compiler to g++-16
/snap/bin/cmake -S . -B build -G Ninja \
    -DCMAKE_CXX_COMPILER=g++-16 \
    -DCMAKE_CXX_SCAN_FOR_MODULES=ON

/snap/bin/cmake --build build

# export SANDBOX_MOUNTS="/usr/bin:/usr/bin:ro,/usr/lib:/usr/lib:ro,/lib:/lib:ro,/lib64:/lib64:ro,/usr/include:/usr/include:ro,/snap:/snap:ro,/usr/libexec/gcc:/usr/libexec/gcc:ro,$(pwd):/app:rw"
