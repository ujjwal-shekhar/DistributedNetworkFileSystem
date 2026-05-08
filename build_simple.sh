#!/bin/bash

cd "$(dirname "$0")"
/snap/bin/cmake -S . -B build -G Ninja && /snap/bin/cmake --build build
