@echo off
echo Testing g++ version...
C:\msys64\ucrt64\bin\g++.exe --version > "C:\repos\Daydream\DwarfortSim\pc_sim\gpp_ver.txt" 2>&1
echo Exit: %ERRORLEVEL%
type "C:\repos\Daydream\DwarfortSim\pc_sim\gpp_ver.txt"

echo.
echo Testing simple compile...
echo int main(){return 0;} > "C:\repos\Daydream\DwarfortSim\pc_sim\tiny.cpp"
C:\msys64\ucrt64\bin\g++.exe "C:\repos\Daydream\DwarfortSim\pc_sim\tiny.cpp" -o "C:\repos\Daydream\DwarfortSim\pc_sim\tiny.exe" > "C:\repos\Daydream\DwarfortSim\pc_sim\tiny_out.txt" 2>&1
echo Exit: %ERRORLEVEL%
type "C:\repos\Daydream\DwarfortSim\pc_sim\tiny_out.txt"
