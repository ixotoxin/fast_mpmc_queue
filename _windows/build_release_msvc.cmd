@ECHO OFF
CLS
CALL "%~dp0config_env_msvc.cmd"
cmake -G "Visual Studio 17 2022" -D CMAKE_BUILD_TYPE=Release -B ./cmk/release-msvc -S ..
cmake --build ./cmk/release-msvc --config Release --verbose
cmake --install ./cmk/release-msvc --prefix ./ --config Release --verbose
CD %~dp0
