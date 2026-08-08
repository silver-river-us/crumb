@echo off
setlocal
set "ROOT_DIR=%~dp0.."
cd /d "%ROOT_DIR%"
if not exist build mkdir build
cd build
cmake ..
if errorlevel 1 exit /b %errorlevel%
msbuild crumb.sln
