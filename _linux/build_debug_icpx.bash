#!/bin/bash

clear
[[ -n "${INTEL_TARGET_ARCH}" ]] || . /opt/intel/oneapi/2025.3/oneapi-vars.sh
. ./config_fs.bash
#[ -e ./cmk/debug-icpx ] && rm -f -r ./cmk/debug-icpx
cmake -D CMAKE_CXX_COMPILER=icpx -D CMAKE_BUILD_TYPE=Debug -B ./cmk/debug-icpx -S ..
cmake --build ./cmk/debug-icpx --config Debug --verbose
cmake --install ./cmk/debug-icpx --prefix ./ --config Debug --verbose
