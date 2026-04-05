@echo off
echo Checking for cc1plus...
if exist "C:\msys64\ucrt64\lib\gcc\x86_64-w64-mingw32\15.2.0\cc1plus.exe" (
    echo cc1plus found
) else (
    echo cc1plus MISSING
)

echo.
echo Checking PATH for mingw dlls...
where gcc 2>&1
where g++ 2>&1

echo.
echo Trying compile with full PATH set...
set "PATH=C:\msys64\ucrt64\bin;C:\msys64\usr\bin;%PATH%"
echo int main(){return 0;} > tiny2.cpp
g++.exe tiny2.cpp -o tiny2.exe > diag_out.txt 2>&1
echo Exit: %ERRORLEVEL%
type diag_out.txt
