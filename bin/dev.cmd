@echo off
setlocal
set "ROOT_DIR=%~dp0.."
call "%~dp0build.cmd"
if errorlevel 1 exit /b %errorlevel%
ctest --preset default
if errorlevel 1 exit /b %errorlevel%
"%ROOT_DIR%\build\crumb.exe"
