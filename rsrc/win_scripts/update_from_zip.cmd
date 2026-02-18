@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
pushd "%SCRIPT_DIR%.." >nul 2>&1
if errorlevel 1 (
    echo Could not locate SoF root folder.
    pause
    exit /b 1
)
set "ROOT_DIR=%CD%"
set "UPDATE_DIR=%ROOT_DIR%\sof_buddy\update"
set "ZIP_PATH="

if not "%~1"=="" (
    set "ZIP_PATH=%~1"
    if not exist "!ZIP_PATH!" (
        if exist "%ROOT_DIR%\%~1" set "ZIP_PATH=%ROOT_DIR%\%~1"
    )
) else (
    for /f "delims=" %%F in ('dir /b /a:-d /o-d "%UPDATE_DIR%\*.zip" 2^>nul') do (
        set "ZIP_PATH=%UPDATE_DIR%\%%F"
        goto :zip_selected
    )
)

:zip_selected
if not defined ZIP_PATH (
    echo No update zip found in "%UPDATE_DIR%".
    echo First run: sofbuddy_update download
    popd
    pause
    exit /b 1
)
if not exist "!ZIP_PATH!" (
    echo Update zip not found: "!ZIP_PATH!"
    popd
    pause
    exit /b 1
)

echo Applying update from:
echo   !ZIP_PATH!
echo.
echo Make sure Soldier of Fortune is fully closed before continuing.
echo.

call :extract_zip "!ZIP_PATH!"
if errorlevel 1 (
    echo.
    echo Failed to extract update zip.
    echo Try installing 7-Zip and rerun this script:
    echo   https://www.7-zip.org/
    popd
    pause
    exit /b 1
)

echo.
echo Update extraction complete.
echo.
echo Searching for old configuration to prune...
for /r "%ROOT_DIR%" %%F in (sofbuddy.cfg) do (
    if exist "%%F" (
        echo   Deleting: %%F
        del /f /q "%%F" >nul 2>&1
    )
)
echo.
echo You can now launch SoF normally.
popd
pause
exit /b 0

:extract_zip
set "ZIP=%~1"

where powershell.exe >nul 2>&1
if not errorlevel 1 (
    powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "try { Expand-Archive -LiteralPath '%ZIP%' -DestinationPath '%ROOT_DIR%' -Force; exit 0 } catch { exit 1 }"
    if not errorlevel 1 exit /b 0
)

where tar.exe >nul 2>&1
if not errorlevel 1 (
    tar -xf "%ZIP%" -C "%ROOT_DIR%"
    if not errorlevel 1 exit /b 0
)

where 7z.exe >nul 2>&1
if not errorlevel 1 (
    7z x -y "%ZIP%" -o"%ROOT_DIR%"
    if not errorlevel 1 exit /b 0
)

call :extract_with_vbscript "%ZIP%"
if not errorlevel 1 exit /b 0

exit /b 1

:extract_with_vbscript
set "ZIP=%~1"
set "VBS=%TEMP%\sofbuddy_unzip_%RANDOM%%RANDOM%.vbs"
(
    echo zipPath = WScript.Arguments(0^)
    echo dstPath = WScript.Arguments(1^)
    echo Set sh = CreateObject("Shell.Application"^)
    echo Set src = sh.NameSpace(zipPath^)
    echo If src Is Nothing Then WScript.Quit 2
    echo Set dst = sh.NameSpace(dstPath^)
    echo If dst Is Nothing Then WScript.Quit 3
    echo dst.CopyHere src.Items, 16
    echo WScript.Sleep 3000
    echo WScript.Quit 0
) > "%VBS%"

cscript //nologo "%VBS%" "%ZIP%" "%ROOT_DIR%" >nul
set "RC=%ERRORLEVEL%"
del /f /q "%VBS%" >nul 2>&1

if "%RC%"=="0" exit /b 0
exit /b 1
