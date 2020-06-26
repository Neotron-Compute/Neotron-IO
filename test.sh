#!/bin/sh
set -e
echo "Compiling..."
g++ -o ./test ./tests/runner.cpp -I. -I./tests
echo "Running..."
./test
