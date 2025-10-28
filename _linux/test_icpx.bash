#!/bin/bash

clear
[[ -n "${INTEL_TARGET_ARCH}" ]] || . /opt/intel/oneapi/2025.3/oneapi-vars.sh
. ./build_gtest_icpx.bash
#[ -e ./cmk/test1-icpx ] && rm -f -r ./cmk/test1-icpx
cmake -D CMAKE_C_COMPILER=icx -D CMAKE_CXX_COMPILER=icpx -D CMAKE_BUILD_TYPE=Debug -D BUILD_TESTS=ON -B ./cmk/tests-icpx -S ..
cmake --build ./cmk/tests-icpx --config Debug
ctest --test-dir ./cmk/tests-icpx --rerun-failed --output-on-failure
