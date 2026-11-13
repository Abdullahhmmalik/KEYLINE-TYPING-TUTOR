@echo off
cd /d "%~dp0"
rem Development launcher: always compile the current sources; never fall back to an old EXE.
call build.bat
if errorlevel 1 exit /b 1
start "" "%~dp0build\keyline.exe" %*
