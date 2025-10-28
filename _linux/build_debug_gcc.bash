#!/bin/bash

clear
. ./config_fs.bash
#[ -e ./cmk/debug-gcc ] && rm -f -r ./cmk/debug-gcc
cmake -D CMAKE_CXX_COMPILER=g++ -D CMAKE_BUILD_TYPE=Debug -B ./cmk/debug-gcc -S ..
cmake --build ./cmk/debug-gcc --config Debug --verbose
cmake --install ./cmk/debug-gcc --prefix ./ --config Debug --verbose
