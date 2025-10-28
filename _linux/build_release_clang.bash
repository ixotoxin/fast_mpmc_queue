#!/bin/bash

clear
. ./config_fs.bash
#[ -e ./cmk/release-clang ] && rm -f -r ./cmk/release-clang
cmake -D CMAKE_CXX_COMPILER=clang++-19 -D CMAKE_BUILD_TYPE=Release -B ./cmk/release-clang -S ..
cmake --build ./cmk/release-clang --config Release --verbose
cmake --install ./cmk/release-clang --prefix ./ --config Release --verbose
