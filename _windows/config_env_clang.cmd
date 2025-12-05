@ECHO OFF
IF NOT "%CLANG_DIR%" == "" GOTO CLANG_DIR_IS_SET
SET CLANG_DIR=C:\Devel\Platform\Clang\21.1.7-x86_64
SET PATH=%CLANG_DIR%\bin;%PATH%
:CLANG_DIR_IS_SET
CALL "%~dp0config_env_ninja.cmd"
CALL "%~dp0config_env_cmake.cmd"
CALL "%~dp0config_fs.cmd"
