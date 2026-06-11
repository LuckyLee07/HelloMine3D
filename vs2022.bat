@echo off
set SCRIPT_DIRECTORY=%~dp0
set CURRENT_DIRECTORY=%cd%
set ARGUMENTS=%*

cd /d %SCRIPT_DIRECTORY%

tools\premake\premake5 --os=windows --file=premake/premake.lua %ARGUMENTS% vs2022

cd /d %CURRENT_DIRECTORY%
pause
