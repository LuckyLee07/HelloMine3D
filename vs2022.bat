@echo off
set SCRIPT_DIRECTORY=%~dp0
set CURRENT_DIRECTORY=%cd%
set ARGUMENTS=%*

cd /d %SCRIPT_DIRECTORY%\premake

where premake5 >nul 2>nul
if errorlevel 1 (
    echo premake5 not found. Install Premake 5 and make sure it is on PATH.
    cd /d %CURRENT_DIRECTORY%
    pause
    exit /b 1
)

premake5 --file=premake.lua %ARGUMENTS% vs2022

cd /d %CURRENT_DIRECTORY%
pause
