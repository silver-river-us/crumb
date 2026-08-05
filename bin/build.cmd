@echo off
setlocal
set "ROOT_DIR=%~dp0.."
cmake --preset default -S "%ROOT_DIR%"
if errorlevel 1 exit /b %errorlevel%
cmake --build --preset default
