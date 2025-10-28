#!/bin/bash

clear
. ./config_fs.bash
#[ -e ./cmk/tests-gcc ] && rm -f -r ./cmk/tests-gcc
cmake -D CMAKE_C_COMPILER=gcc -D CMAKE_CXX_COMPILER=g++ -D CMAKE_BUILD_TYPE=Debug -D BUILD_TESTS=ON -B ./cmk/tests-gcc -S ..
cmake --build ./cmk/tests-gcc --config Debug
ctest --test-dir ./cmk/tests-gcc --rerun-failed --output-on-failure
