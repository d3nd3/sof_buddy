@echo off
setlocal
cd /d "%~dp0\.."
set "EXE=%~1"
if "%EXE%"=="" set "EXE=%CD%\SoF2MP.exe"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0patch_sof2mp.ps1" -ExePath "%EXE%"
if errorlevel 1 exit /b 1
echo sof2bud enabled for: %EXE%
exit /b 0
