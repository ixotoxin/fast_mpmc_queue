@ECHO OFF
CLS
CALL "%~dp0config_env_gcc.cmd"
cmake -G Ninja -D CMAKE_CXX_COMPILER=g++ -D CMAKE_BUILD_TYPE=Debug -B ./cmk/debug-gcc -S ..
cmake --build ./cmk/debug-gcc --config Debug --verbose
cmake --install ./cmk/debug-gcc --prefix ./ --config Debug --verbose
CD %~dp0
