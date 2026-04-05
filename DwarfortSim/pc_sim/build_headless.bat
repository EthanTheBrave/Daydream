@echo off
cd /d C:\repos\Daydream\DwarfortSim\pc_sim
C:\msys64\ucrt64\bin\g++.exe -std=c++11 -DHEADLESS=10000 -I. -I.. main.cpp ..\map.cpp ..\tasks.cpp ..\dwarves.cpp ..\pathfind.cpp ..\fortplan.cpp ..\renderer.cpp -o DwarfortSim_sim.exe 2>build_err.txt
if %ERRORLEVEL% == 0 (
    echo BUILD_OK
    DwarfortSim_sim.exe > run_out.txt 2>&1
    echo RUN_DONE
) else (
    echo BUILD_FAIL
    type build_err.txt
)
