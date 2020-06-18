#!/bin/sh
set -e
g++ -o ./test ./tests/runner.cpp -I.
./test

