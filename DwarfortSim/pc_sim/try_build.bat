@echo off
REM Use this to build and capture all errors to try_out.txt
REM Run with: cmd.exe /c "C:\repos\Daydream\DwarfortSim\pc_sim\try_build.bat"
set "PATH=C:\msys64\ucrt64\bin;C:\msys64\usr\bin;%PATH%"
cd /d C:\repos\Daydream\DwarfortSim\pc_sim
g++.exe -std=c++11 -DHEADLESS=10000 ^
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
    C:\repos\Daydream\DwarfortSim\save.cpp ^
    -o C:\repos\Daydream\DwarfortSim\pc_sim\DwarfortSim_hl.exe ^
    > C:\repos\Daydream\DwarfortSim\pc_sim\try_out.txt 2>&1
echo EXIT_CODE=%ERRORLEVEL% >> C:\repos\Daydream\DwarfortSim\pc_sim\try_out.txt
if %ERRORLEVEL% == 0 (
    C:\repos\Daydream\DwarfortSim\pc_sim\DwarfortSim_hl.exe >> C:\repos\Daydream\DwarfortSim\pc_sim\try_out.txt 2>&1
)
