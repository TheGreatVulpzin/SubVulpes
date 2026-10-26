@echo off
rem Build the Windows x86 server and launcher. Set VCVARS for another MSVC installation.
setlocal
set VSLANG=1033
where cl >nul 2>nul && goto have_cl
if not defined VCVARS set VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars32.bat
call "%VCVARS%" >nul 2>nul || (echo ERROR: vcvars32.bat not found & exit /b 1)
:have_cl
cd /d "%~dp0.."
if not exist build\server\.cmake\api\v1\query mkdir build\server\.cmake\api\v1\query
type nul > build\server\.cmake\api\v1\query\codemodel-v2
cmake -S . -B build\server -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release || exit /b 1
cmake --build build\server || exit /b 1
echo OK: build\server\subvulpes_server.exe
echo OK: build\launcher\subvulpes_launcher.exe
