@ECHO OFF
CLS
CALL "%~dp0config_env_clang.cmd"
cmake -G Ninja -D CMAKE_CXX_COMPILER=clang++ -D CMAKE_BUILD_TYPE=Debug -B ./cmk/debug-clang -S ..
cmake --build ./cmk/debug-clang --config Debug --verbose
cmake --install ./cmk/debug-clang --prefix ./ --config Debug --verbose
CD %~dp0
