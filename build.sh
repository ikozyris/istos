#!/bin/sh
CORE_SRC="core/engine-main.cpp core/attacks.cpp core/make-move.cpp core/evaluate.cpp core/search.cpp core/transposition-table.cpp"
#g++-15 test/visualizer.cpp $CORE_SRC -o chess -Wall -O3 -flto -march=native
g++-15 test/perf-test.cpp $CORE_SRC -o chess -Wall -O3 -flto -march=native -g
