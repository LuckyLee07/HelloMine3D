@echo off
set SCRIPT_DIRECTORY=%~dp0
set CURRENT_DIRECTORY=%cd%
set ARGUMENTS=%*

cd /d %SCRIPT_DIRECTORY%
rmdir /S/Q build

cd bin
del /S/Q *.log *.pdb *.exe *.idb
cd ..

echo on
tools\premake\premake5 --os=windows --file=premake/premake.lua %ARGUMENTS% vs2017

cd /d %CURRENT_DIRECTORY%

pause
