#!/bin/bash

clear
. ./config_fs.bash
#[ -e ./cmk/tests-clang ] && rm -f -r ./cmk/tests-clang
cmake -D CMAKE_C_COMPILER=clang-19 -D CMAKE_CXX_COMPILER=clang++-19 -D CMAKE_BUILD_TYPE=Debug -D BUILD_TESTS=ON -B ./cmk/tests-clang -S ..
cmake --build ./cmk/tests-clang --config Debug
ctest --test-dir ./cmk/tests-clang --rerun-failed --output-on-failure
