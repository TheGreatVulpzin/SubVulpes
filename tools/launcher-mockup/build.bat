@echo off
rem Builds build\mockup\SubVulpesMockup.exe with the C# compiler of .NET Framework 4.
setlocal
cd /d "%~dp0..\.."
set NET=%WINDIR%\Microsoft.NET\Framework\v4.0.30319
if not exist build\mockup mkdir build\mockup
"%NET%\csc.exe" /nologo /target:winexe /out:build\mockup\SubVulpesMockup.exe /optimize+ /platform:x86 ^
    /r:System.Windows.Forms.dll /r:System.Drawing.dll tools\launcher-mockup\Mockup.cs
