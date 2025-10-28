@ECHO OFF
IF NOT "%MSVC_DIR%" == "" GOTO MSVC_DIR_IS_SET
SET MSVC_DIR=C:\Program Files\Microsoft Visual Studio\2022\Community
CALL "%MSVC_DIR%\VC\Auxiliary\Build\vcvars64.bat"
:MSVC_DIR_IS_SET
CALL "%~dp0config_env_cmake.cmd"
CALL "%~dp0config_fs.cmd"
