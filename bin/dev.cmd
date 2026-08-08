@echo off
setlocal
set "ROOT_DIR=%~dp0.."
call "%~dp0build.cmd"
if errorlevel 1 exit /b %errorlevel%
cd /d "%ROOT_DIR%\build"
ctest
if errorlevel 1 exit /b %errorlevel%
crumb.exe
