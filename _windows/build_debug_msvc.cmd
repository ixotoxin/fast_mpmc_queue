@ECHO OFF
CLS
CALL "%~dp0config_env_msvc.cmd"
cmake -G "Visual Studio 17 2022" -D CMAKE_BUILD_TYPE=Debug -B ./cmk/debug-msvc -S ..
cmake --build ./cmk/debug-msvc --config Debug --verbose
cmake --install ./cmk/debug-msvc --prefix ./ --config Debug --verbose
CD %~dp0
