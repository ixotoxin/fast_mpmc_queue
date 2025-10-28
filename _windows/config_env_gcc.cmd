@ECHO OFF
IF NOT "%GCC_DIR%" == "" GOTO GCC_DIR_IS_SET
SET GCC_DIR=C:\Devel\Platform\GCC\15.2.0-x86_64-win32-seh-ucrt-rt_v13-rev0
SET PATH=%GCC_DIR%\bin;%PATH%
:GCC_DIR_IS_SET
CALL "%~dp0config_env_ninja.cmd"
CALL "%~dp0config_env_cmake.cmd"
CALL "%~dp0config_fs.cmd"
