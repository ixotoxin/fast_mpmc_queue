#!/bin/bash

clear
. ./config_fs.bash
#[ -e ./cmk/debug-clang ] && rm -f -r ./cmk/debug-clang
cmake -D CMAKE_CXX_COMPILER=clang++-19 -D CMAKE_BUILD_TYPE=Debug -B ./cmk/debug-clang -S ..
cmake --build ./cmk/debug-clang --config Debug --verbose
cmake --install ./cmk/debug-clang --prefix ./ --config Debug --verbose
