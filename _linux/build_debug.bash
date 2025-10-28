#!/bin/bash

clear
. ./config_fs.bash
#[ -e ./cmk/debug-auto ] && rm -f -r ./cmk/debug-auto
cmake -D CMAKE_BUILD_TYPE=Debug -B ./cmk/debug-auto -S ..
cmake --build ./cmk/debug-auto --config Debug --verbose
cmake --install ./cmk/debug-auto --prefix ./ --config Debug --verbose
