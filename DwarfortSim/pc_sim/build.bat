@echo off
:: ================================================================
::  DwarfortSim PC Debug Build
::  Uses MSYS2 ucrt64 g++ if present, otherwise falls back to any
::  g++ already on PATH.
:: ================================================================

set SRC=main.cpp ^
        ..\map.cpp ^
        ..\tasks.cpp ^
        ..\dwarves.cpp ^
        ..\pathfind.cpp ^
        ..\fortplan.cpp ^
        ..\renderer.cpp

set FLAGS=-std=c++11 -I. -I.. -Wall -Wno-unused-variable -Wno-unused-function
set OUT=DwarfortSim_sim.exe

:: Prefer MSYS2 ucrt64
set GPP=g++
if exist "C:\msys64\ucrt64\bin\g++.exe" (
    set GPP=C:\msys64\ucrt64\bin\g++.exe
    set TEMP=C:\Users\%USERNAME%\AppData\Local\Temp
    set TMP=%TEMP%
)

echo Building with %GPP%...
%GPP% %FLAGS% %SRC% -o %OUT%

if %ERRORLEVEL% == 0 (
    echo.
    echo Build successful: %OUT%
    echo Controls: +/- speed   d debug   q quit
    echo.
    %OUT%
) else (
    echo.
    echo Build FAILED.
)
pause
