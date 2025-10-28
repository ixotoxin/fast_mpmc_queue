@ECHO OFF
CLS
CALL "%~dp0config_env_msvc.cmd"
cmake -G "Visual Studio 17 2022" -D CMAKE_BUILD_TYPE=Debug -D BUILD_TESTS=ON -B ./cmk/tests-msvc -S ..
cmake --build ./cmk/tests-msvc --config Debug
ctest -C Debug --test-dir ./cmk/tests-msvc
CD %~dp0
