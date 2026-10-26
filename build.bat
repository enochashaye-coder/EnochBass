@echo off
REM ============================================================
REM  Enoch Bass - one-click Windows build + install (VST3)
REM  Needs: Visual Studio (Community is free) with "Desktop development with C++", CMake, and Git.
REM  Run this file by RIGHT-CLICK -> "Run as administrator" so it can install the plugin.
REM ============================================================
setlocal
cd /d "%~dp0"

where cmake >nul 2>nul || (echo CMake was not found. Install it from https://cmake.org/download/ and choose "Add CMake to the system PATH". Then run this again. & pause & exit /b 1)
where git >nul 2>nul || (echo Git was not found. Install it from https://git-scm.com/download/win ^(keep the default options^). Then run this again. & pause & exit /b 1)

echo Configuring (the first run downloads JUCE, this can take a few minutes)...
set "GEN="
for %%G in ("Visual Studio 17 2022" "Visual Studio 18 2026") do (
  if not defined GEN (
    if exist build rmdir /s /q build
    cmake -S . -B build -G %%G -A x64 >nul 2>nul && set "GEN=%%~G"
  )
)
if not defined GEN (
  if exist build rmdir /s /q build
  cmake -S . -B build -A x64 && set "GEN=default"
)
if not defined GEN (
  echo.
  echo Configure failed. Make sure Visual Studio is installed with "Desktop development with C++", and that you are online.
  pause & exit /b 1
)
echo Using generator: %GEN%
cmake -S . -B build -A x64 >nul 2>nul

echo Building (about 5-10 minutes the first time)...
cmake --build build --config Release --target EnochBass_VST3 -j
if errorlevel 1 (echo. & echo Build failed. Copy the last lines above and send them to me. & pause & exit /b 1)

set "SRC=build\EnochBass_artefacts\Release\VST3\Enoch Bass.vst3"
set "DST=%CommonProgramFiles%\VST3"
if not exist "%SRC%" (echo Could not find the built plugin at "%SRC%". & pause & exit /b 1)
echo.
echo Installing to "%DST%" ...
xcopy /E /I /Y "%SRC%" "%DST%\Enoch Bass.vst3" >nul
if errorlevel 1 (
  echo Could not copy automatically ^(run this file as Administrator^). Copy this folder yourself:
  echo   %CD%\%SRC%
  echo into:
  echo   %DST%
) else (
  echo.
  echo DONE! Rescan plugins in your DAW and look for "Enoch Bass" under Instruments.
)
pause
