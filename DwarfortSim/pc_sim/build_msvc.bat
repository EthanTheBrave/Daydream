@echo off
:: ================================================================
::  DwarfortSim PC Debug Build — MSVC (Visual Studio)
::  Run this from a normal command prompt; it finds VS automatically.
:: ================================================================

set SRC=main.cpp ^
        ..\map.cpp ^
        ..\tasks.cpp ^
        ..\dwarves.cpp ^
        ..\pathfind.cpp ^
        ..\fortplan.cpp ^
        ..\renderer.cpp

set FLAGS=/std:c++14 /I. /I.. /EHsc /W3 /wd4244 /wd4101 /wd4305

set OUT=DwarfortSim_sim.exe

:: Find vswhere to locate the compiler
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist %VSWHERE% (
    echo ERROR: vswhere not found. Is Visual Studio installed?
    pause & exit /b 1
)

:: Get the VS install path
for /f "usebackq delims=" %%i in (`%VSWHERE% -latest -property installationPath`) do set VSDIR=%%i

:: Activate the MSVC environment (x64)
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" > nul 2>&1

echo Building %OUT%...
cl %FLAGS% %SRC% /Fe:%OUT% /link

if %ERRORLEVEL% == 0 (
    echo.
    echo Build successful: %OUT%
    echo Controls:  +/- speed    d = debug overlay    q = quit
    echo.
    %OUT%
) else (
    echo.
    echo Build FAILED — see errors above.
)
pause
