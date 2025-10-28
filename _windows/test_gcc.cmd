@ECHO OFF
CLS
CALL "%~dp0config_env_gcc.cmd"
cmake -G Ninja -D CMAKE_C_COMPILER=gcc -D CMAKE_CXX_COMPILER=g++ -D CMAKE_BUILD_TYPE=Debug -D BUILD_TESTS=ON -B ./cmk/tests-gcc -S ..
cmake --build ./cmk/tests-gcc --config Debug
ctest --test-dir ./cmk/tests-gcc
CD %~dp0
