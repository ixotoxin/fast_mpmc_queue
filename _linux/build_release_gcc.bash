#!/bin/bash

clear
. ./config_fs.bash
#[ -e ./cmk/release-gcc ] && rm -f -r ./cmk/release-gcc
cmake -D CMAKE_CXX_COMPILER=g++ -D CMAKE_BUILD_TYPE=Release -B ./cmk/release-gcc -S ..
cmake --build ./cmk/release-gcc --config Release --verbose
cmake --install ./cmk/release-gcc --prefix ./ --config Release --verbose
