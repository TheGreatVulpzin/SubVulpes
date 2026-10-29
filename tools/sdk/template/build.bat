@echo off
rem Build a 32-bit DLL with MSVC. Set VCVARS to a vcvars32.bat path to select an installation.
setlocal
set VSLANG=1033
if defined VCVARS goto setup_msvc
where cl >nul 2>nul && goto have_cl
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (echo ERROR: Install MSVC Build Tools with the C++ x86 tools. & exit /b 1)
for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VCVARS=%%I\VC\Auxiliary\Build\vcvars32.bat"
if not defined VCVARS (echo ERROR: vswhere found no MSVC installation with the C++ x86 tools. & exit /b 1)
:setup_msvc
if not exist "%VCVARS%" (echo ERROR: VCVARS must name an existing vcvars32.bat file. & exit /b 1)
call "%VCVARS%" >nul 2>nul || (echo ERROR: MSVC setup failed. & exit /b 1)
:have_cl
if /i "%VSCMD_ARG_TGT_ARCH%"=="x64" (echo ERROR: Use an x86 MSVC prompt or set VCVARS to vcvars32.bat. & exit /b 1)
cd /d "%~dp0"
if not exist obj mkdir obj
if not exist out mkdir out
cl /nologo /LD /W4 /WX /utf-8 /D_CRT_SECURE_NO_WARNINGS /I ..\include hello.c /Fo:obj\ /Fe:out\hello.dll /link /MACHINE:X86 /IMPLIB:obj\hello.lib /EXPORT:sv_plugin_entry || exit /b 1
copy /y plugin.ini out\plugin.ini >nul || exit /b 1
echo OK: out\hello.dll
echo Next: powershell -NoProfile -ExecutionPolicy Bypass -File ..\tools\pack.ps1 -Folder out -OutputDir ..\dist -ServerExe PATH\subvulpes_server.exe
