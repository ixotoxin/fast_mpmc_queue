@ECHO OFF
CLS
CALL "%~dp0config_env_gcc.cmd"
cmake -G Ninja -D CMAKE_CXX_COMPILER=g++ -D CMAKE_BUILD_TYPE=Release -B ./cmk/release-gcc -S ..
cmake --build ./cmk/release-gcc --config Release --verbose
cmake --install ./cmk/release-gcc --prefix ./ --config Release --verbose
CD %~dp0
