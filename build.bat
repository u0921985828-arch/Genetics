@echo off
REM ---------------------------------------------------------------------------
REM  CHRONA - one-command build for Windows.
REM
REM  Requirements (installed once):
REM    * CMake >= 3.22        (https://cmake.org/download)
REM    * Visual Studio 2022 with the "Desktop development with C++" workload
REM    * Internet on first run (JUCE is fetched automatically by CMake)
REM
REM  Usage:  build.bat            (Release)
REM          build.bat Debug
REM ---------------------------------------------------------------------------
setlocal
cd /d "%~dp0"

set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Release

echo ^>^> Configuring (%BUILD_TYPE%)...
cmake -B build -G "Visual Studio 17 2022" -A x64
if errorlevel 1 exit /b 1

echo ^>^> Building...
cmake --build build --config %BUILD_TYPE% -j
if errorlevel 1 exit /b 1

echo.
echo ^>^> Done. Look for CHRONA.vst3 under build\CHRONA_artefacts\%BUILD_TYPE%\VST3
endlocal
