@ECHO OFF
CLS
CALL "%~dp0config_env_clang.cmd"
cmake -G Ninja -D CMAKE_CXX_COMPILER=clang++ -D CMAKE_BUILD_TYPE=Release -B ./cmk/release-clang -S ..
cmake --build ./cmk/release-clang --config Release --verbose
cmake --install ./cmk/release-clang --prefix ./ --config Release --verbose
CD %~dp0
