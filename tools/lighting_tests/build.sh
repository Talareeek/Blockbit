#!/bin/bash
set -e
cd "$(dirname "$0")"

# Build the standalone lighting test harness. We compile LightingSystem.cpp
# straight from src/ — the module is deliberately dependency-free so nothing
# else is linked in.
clang++ -O2 -std=c++20 -Wall -Wextra -Wpedantic \
    -I../../include \
    -o lighting_tests \
    tests.cpp \
    ../../src/LightingSystem.cpp

echo "Zbudowano: lighting_tests"
