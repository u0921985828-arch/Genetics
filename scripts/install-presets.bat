@echo off
rem Copy the CHRONA factory preset banks into %APPDATA%\CHRONA\Presets
setlocal
set "SRC=%~dp0..\presets"
set "DEST=%APPDATA%\CHRONA\Presets"
if not exist "%DEST%" mkdir "%DEST%"
xcopy /E /I /Y "%SRC%\*" "%DEST%\" >nul
echo Installed CHRONA preset banks to %DEST%
endlocal
