#!/bin/bash

clear
. ./config_fs.bash
#[ -e ./cmk/tests-auto ] && rm -f -r ./cmk/tests-auto
cmake -D CMAKE_BUILD_TYPE=Debug -D BUILD_TESTS=ON -B ./cmk/tests-auto -S ..
cmake --build ./cmk/tests-auto --config Debug
ctest --test-dir ./cmk/tests-auto
