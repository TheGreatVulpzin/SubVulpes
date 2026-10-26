@echo off
rem Builds build\launcher\subvulpes_launcher.exe with the C# compiler of .NET Framework 4.
setlocal
cd /d "%~dp0..\.."
set NET=%WINDIR%\Microsoft.NET\Framework\v4.0.30319
if not exist build\launcher mkdir build\launcher
set LAUNCHER_TARGET=/target:winexe /out:build\launcher\subvulpes_launcher.exe
set LAUNCHER_TEST_SOURCE=
if "%~1"=="test" (
    set LAUNCHER_TARGET=/target:exe /main:SubVulpes.LauncherUpdateTests /out:build\launcher\launcher-update-tests.exe
    set LAUNCHER_TEST_SOURCE=tests\launcher_update_test.cs
)
"%NET%\csc.exe" /nologo %LAUNCHER_TARGET% /optimize+ ^
    /platform:x86 /r:System.Windows.Forms.dll /r:System.Drawing.dll /r:System.Core.dll /r:System.Web.Extensions.dll ^
    /r:"%NET%\System.IO.Compression.dll" /r:"%NET%\System.IO.Compression.FileSystem.dll" ^
    /resource:tools\release\default-config.ini,SubVulpes.DefaultConfig tools\launcher\Launcher.cs %LAUNCHER_TEST_SOURCE%
