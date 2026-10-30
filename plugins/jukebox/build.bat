@echo off
rem Build the SDK template target with sv_add_plugin. Optional arguments: build folder, SDK root.
setlocal
set VSLANG=1033
if defined VCVARS goto setup_msvc
where cl >nul 2>nul && goto have_cl
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (echo ERROR: Install MSVC x86 Build Tools. & exit /b 1)
for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VCVARS=%%I\VC\Auxiliary\Build\vcvars32.bat"
if not defined VCVARS (echo ERROR: No MSVC x86 tools found. & exit /b 1)
:setup_msvc
call "%VCVARS%" >nul 2>nul || (echo ERROR: MSVC setup failed. & exit /b 1)
:have_cl
if /i "%VSCMD_ARG_TGT_ARCH%"=="x64" (echo ERROR: Use an x86 MSVC prompt. & exit /b 1)
set "JUKEBOX_BUILD=%~dp0out"
if not "%~1"=="" set "JUKEBOX_BUILD=%~1"
if not "%~2"=="" goto external_sdk
cmake -S "%~dp0." -B "%JUKEBOX_BUILD%" -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release || exit /b 1
goto build_plugin
:external_sdk
cmake -S "%~dp0." -B "%JUKEBOX_BUILD%" -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DSV_SDK_ROOT="%~2" || exit /b 1
:build_plugin
cmake --build "%JUKEBOX_BUILD%" --target sv-jukebox || exit /b 1
echo OK: %JUKEBOX_BUILD%\plugins\sv.jukebox\jukebox.dll
