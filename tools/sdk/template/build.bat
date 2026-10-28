@echo off
rem Builds the plugin as a 32-bit DLL with MSVC. Set VCVARS to the path of vcvars32.bat for another MSVC installation.
setlocal
set VSLANG=1033
where cl >nul 2>nul && goto have_cl
if not defined VCVARS set VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars32.bat
call "%VCVARS%" >nul 2>nul || (echo ERROR: vcvars32.bat not found & exit /b 1)
:have_cl
cd /d "%~dp0"
if not exist obj mkdir obj
if not exist out mkdir out
cl /nologo /LD /W4 /WX /utf-8 /D_CRT_SECURE_NO_WARNINGS /I ..\include hello.c /Fo:obj\ /Fe:out\hello.dll /link /IMPLIB:obj\hello.lib /EXPORT:sv_plugin_entry || exit /b 1
copy /y plugin.ini out\plugin.ini >nul || exit /b 1
echo OK: out\hello.dll
echo Next: powershell -File ..\tools\pack.ps1 -Folder out -OutputDir ..\dist -ServerExe PATH\subvulpes_server.exe
