#!/bin/bash

clear
[[ -n "${INTEL_TARGET_ARCH}" ]] || . /opt/intel/oneapi/2025.3/oneapi-vars.sh
. ./config_fs.bash
#[ -e ./cmk/release-icpx ] && rm -f -r ./cmk/release-icpx
cmake -D CMAKE_CXX_COMPILER=icpx -D CMAKE_BUILD_TYPE=Release -B ./cmk/release-icpx -S ..
cmake --build ./cmk/release-icpx --config Release --verbose
cmake --install ./cmk/release-icpx --prefix ./ --config Release --verbose
