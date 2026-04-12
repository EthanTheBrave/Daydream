@echo off
REM Build and launch the interactive terminal viewer.
REM Run this from a normal Windows Terminal or cmd window — NOT from inside VS Code's
REM embedded terminal, which doesn't render ANSI colours properly.
REM
REM Controls while running:
REM   +   speed up (halve tick interval)
REM   -   slow down (double tick interval)
REM   d   toggle dwarf debug overlay
REM   q   quit

set "PATH=C:\msys64\ucrt64\bin;C:\msys64\usr\bin;%PATH%"
cd /d C:\repos\Daydream\DwarfortSim\pc_sim

echo Building...
g++.exe -std=c++11 ^
    -IC:\repos\Daydream\DwarfortSim\pc_sim ^
    -IC:\repos\Daydream\DwarfortSim ^
    C:\repos\Daydream\DwarfortSim\pc_sim\main.cpp ^
    C:\repos\Daydream\DwarfortSim\map.cpp ^
    C:\repos\Daydream\DwarfortSim\tasks.cpp ^
    C:\repos\Daydream\DwarfortSim\dwarves.cpp ^
    C:\repos\Daydream\DwarfortSim\pathfind.cpp ^
    C:\repos\Daydream\DwarfortSim\animals.cpp ^
    C:\repos\Daydream\DwarfortSim\goblins.cpp ^
    C:\repos\Daydream\DwarfortSim\fortplan.cpp ^
    C:\repos\Daydream\DwarfortSim\renderer.cpp ^
    -o DwarfortSim_interactive.exe 2>&1

if %ERRORLEVEL% neq 0 (
    echo Build failed.
    pause
    exit /b 1
)

echo Build OK. Launching...
DwarfortSim_interactive.exe
