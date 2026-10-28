@echo off
setlocal
cd /d "%~dp0..\..\.."
if not exist build\runtimes\tmp mkdir build\runtimes\tmp
set TEMP=%CD%\build\runtimes\tmp
set TMP=%TEMP%
python -I tools\sdk\fetch_runtimes.py || exit /b 1
python -I tools\sdk\spikes\make_module.py || exit /b 1
where cl >nul 2>nul && goto have_cl
if not defined VCVARS set VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars32.bat
call "%VCVARS%" >nul 2>nul || exit /b 1
:have_cl
powershell -NoProfile -Command "Add-Content -LiteralPath build/state.txt -Value ('BUILD START ' + [DateTime]::UtcNow.ToString('o')) -Encoding ascii" || exit /b 1
cmake -S tools\sdk\spikes -B build\runtimes\spikes -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
set SV_SPIKE_EXIT=%ERRORLEVEL%
if not "%SV_SPIKE_EXIT%"=="0" goto build_end
cmake --build build\runtimes\spikes --target lua-spike wasm3-spike wamr-spike
set SV_SPIKE_EXIT=%ERRORLEVEL%
if not "%SV_SPIKE_EXIT%"=="0" goto build_end
python -I tools\sdk\spikes\run.py
set SV_SPIKE_EXIT=%ERRORLEVEL%
:build_end
powershell -NoProfile -Command "Add-Content -LiteralPath build/state.txt -Value ('BUILD END ' + [DateTime]::UtcNow.ToString('o')) -Encoding ascii" || exit /b 1
exit /b %SV_SPIKE_EXIT%
