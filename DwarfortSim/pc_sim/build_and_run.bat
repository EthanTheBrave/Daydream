@echo off
set "PATH=C:\msys64\ucrt64\bin;C:\msys64\usr\bin;%PATH%"
cd /d C:\repos\Daydream\DwarfortSim\pc_sim
g++.exe -std=c++11 -DHEADLESS=10000 ^
    -I"C:\repos\Daydream\DwarfortSim\pc_sim" ^
    -I"C:\repos\Daydream\DwarfortSim" ^
    "C:\repos\Daydream\DwarfortSim\pc_sim\main.cpp" ^
    "C:\repos\Daydream\DwarfortSim\map.cpp" ^
    "C:\repos\Daydream\DwarfortSim\tasks.cpp" ^
    "C:\repos\Daydream\DwarfortSim\dwarves.cpp" ^
    "C:\repos\Daydream\DwarfortSim\pathfind.cpp" ^
    "C:\repos\Daydream\DwarfortSim\animals.cpp" ^
    "C:\repos\Daydream\DwarfortSim\goblins.cpp" ^
    "C:\repos\Daydream\DwarfortSim\fortplan.cpp" ^
    "C:\repos\Daydream\DwarfortSim\renderer.cpp" ^
    -o "C:\repos\Daydream\DwarfortSim\pc_sim\DwarfortSim_hl.exe" ^
    2> "C:\repos\Daydream\DwarfortSim\pc_sim\build_out.txt"
if %ERRORLEVEL% == 0 (
    echo BUILD_OK
    "C:\repos\Daydream\DwarfortSim\pc_sim\DwarfortSim_hl.exe" > "C:\repos\Daydream\DwarfortSim\pc_sim\run_out.txt" 2>&1
    echo RUN_DONE
) else (
    echo BUILD_FAIL
    type "C:\repos\Daydream\DwarfortSim\pc_sim\build_out.txt"
)
