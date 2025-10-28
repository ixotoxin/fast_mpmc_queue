#!/bin/bash

clear
. ./config_fs.bash
#[ -e ./cmk/release-auto ] && rm -f -r ./cmk/release-auto
cmake -D CMAKE_BUILD_TYPE=Release -B ./cmk/release-auto -S ..
cmake --build ./cmk/release-auto --config Release --verbose
cmake --install ./cmk/release-auto --prefix ./ --config Release --verbose
