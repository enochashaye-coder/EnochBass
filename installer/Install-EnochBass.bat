@echo off
REM Right-click -> "Run as administrator". Installs Enoch Bass.vst3 next to this file into the system VST3 folder.
setlocal
cd /d "%~dp0"
net session >nul 2>&1 || (echo Please right-click this file and choose "Run as administrator". & pause & exit /b 1)
if not exist "Enoch Bass.vst3" (echo "Enoch Bass.vst3" must be in the same folder as this installer. & pause & exit /b 1)
xcopy /E /I /Y "Enoch Bass.vst3" "%CommonProgramFiles%\VST3\Enoch Bass.vst3" >nul && echo Installed! Rescan plugins in your DAW and look for "Enoch Bass" under Instruments. || echo Install failed.
pause
