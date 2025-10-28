@ECHO OFF
CLS
CALL "%~dp0config_env_clang.cmd"
cmake -G Ninja -D CMAKE_C_COMPILER=clang -D CMAKE_CXX_COMPILER=clang++ -D CMAKE_BUILD_TYPE=Debug -D BUILD_TESTS=ON -B ./cmk/tests-clang -S ..
cmake --build ./cmk/tests-clang --config Debug
ctest --test-dir ./cmk/tests-clang
CD %~dp0
