@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
pushd "%SCRIPT_DIR%.." >nul 2>&1
if errorlevel 1 (
    echo Could not locate SoF root folder.
    if not defined NO_PAUSE pause
    exit /b 1
)
set "ROOT_DIR=%CD%"
set "UPDATE_DIR=%ROOT_DIR%\sof_buddy\update"
set "ZIP_PATH="
set "EXTRACT_METHOD="
set "IS_WINE="
set "NO_PAUSE="
if defined SOFBUDDY_UPDATER_NO_PAUSE set "NO_PAUSE=1"

:: Check for Wine environment
reg query "HKCU\Software\Wine" >nul 2>&1 && set "IS_WINE=1"

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
    if not defined NO_PAUSE pause
    exit /b 1
)
if /i not "%ZIP_PATH:~1,1%"==":" set "ZIP_PATH=%ROOT_DIR%\%ZIP_PATH%"
for %%I in ("%ZIP_PATH%") do set "ZIP_PATH=%%~fI"
if not exist "!ZIP_PATH!" (
    echo Update zip not found: "!ZIP_PATH!"
    popd
    if not defined NO_PAUSE pause
    exit /b 1
)

echo Applying update from:
echo   !ZIP_PATH!
echo.
echo Make sure Soldier of Fortune is fully closed before continuing.
echo.

call :extract_zip "!ZIP_PATH!"

:: Verify extraction success by checking if key files exist
if not exist "%ROOT_DIR%\sof_buddy.dll" goto :extract_failed
if not exist "%ROOT_DIR%\sof_buddy\update_from_zip.cmd" goto :extract_failed

echo.
echo Update extraction complete.
if defined EXTRACT_METHOD echo Extraction method: !EXTRACT_METHOD!

if /i "!EXTRACT_METHOD!"=="vbscript" (
    echo Keeping update zip because VBScript extraction is not fully reliable.
    echo If files did not update, run sof_buddy/update_from_zip.sh from Linux host.
) else (
    echo Removing update zip.
    del /f /q "!ZIP_PATH!" >nul 2>&1
    if exist "!ZIP_PATH!" (
        echo Warning: could not delete "!ZIP_PATH!".
    ) else (
        echo Removed: !ZIP_PATH!
    )
)
echo.
echo Searching for old configuration to prune...
for /r "%ROOT_DIR%" %%F in (sofbuddy.cfg) do (
    if exist "%%F" (
        echo   Removing: %%F
        del /f /q "%%F" >nul 2>&1
    )
)
echo.
echo You can now launch SoF normally.
popd
if not defined NO_PAUSE pause
exit /b 0

:extract_failed
echo.
echo [ERROR] Failed to extract update zip.
echo.
if defined IS_WINE (
    echo Wine detected. Since automated unzipping failed:
    call :print_linux_manual_command "!ZIP_PATH!"
) else (
    echo Try installing 7-Zip and rerun this script:
    echo   https://www.7-zip.org/
)
popd
if not defined NO_PAUSE pause
exit /b 1

:extract_zip
set "ZIP=%~1"

:: 1. Try Windows 10/Wine built-in tar.exe
:: Skipped on Wine because it often fails silently
if not defined IS_WINE (
    where tar.exe >nul 2>&1
    if not errorlevel 1 (
        tar.exe -xf "%ZIP%" -C "%ROOT_DIR%" >nul 2>&1
        if not errorlevel 1 (
            set "EXTRACT_METHOD=tar"
            exit /b 0
        )
    )
)

:: 2. Try 7-Zip if installed
where 7z.exe >nul 2>&1
if not errorlevel 1 (
    7z.exe x -y "%ZIP%" -o"%ROOT_DIR%" >nul 2>&1
    if not errorlevel 1 (
        set "EXTRACT_METHOD=7z"
        exit /b 0
    )
)

:: 3. Try Linux Host Unzip (Wine Only)
if defined IS_WINE (
    call :extract_with_unix_unzip "%ZIP%"
    :: We assume success here because the function ignores error codes
    :: and falls through to file validation in the main loop.
    set "EXTRACT_METHOD=unix-unzip"
    exit /b 0
)

:: 4. Try VBScript (Windows Only)
if not defined IS_WINE (
    call :extract_with_vbscript "%ZIP%"
    if not errorlevel 1 (
        set "EXTRACT_METHOD=vbscript"
        exit /b 0
    )
)

:: 5. Try PowerShell (Windows Only)
if not defined IS_WINE (
    where powershell.exe >nul 2>&1
    if not errorlevel 1 (
        powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "try { Expand-Archive -LiteralPath '%ZIP%' -DestinationPath '%ROOT_DIR%' -Force; exit 0 } catch { exit 1 }" >nul 2>&1
        if not errorlevel 1 (
            set "EXTRACT_METHOD=powershell"
            exit /b 0
        )
    )
)

exit /b 1

:print_linux_manual_command
set "ZIP=%~1"
set "HOST_SH="
set "HOST_ZIP="
for /f "delims=" %%I in ('winepath.exe -u "%ROOT_DIR%\sof_buddy\update_from_zip.sh" 2^>nul') do set "HOST_SH=%%I"
for /f "delims=" %%I in ('winepath.exe -u "%ZIP%" 2^>nul') do set "HOST_ZIP=%%I"
if defined HOST_SH if defined HOST_ZIP (
    echo.
    echo Run this command in your Linux terminal:
    echo   /bin/bash "%HOST_SH%" "%HOST_ZIP%"
    echo.
)
exit /b 0

:extract_with_unix_unzip
set "ZIP_WIN=%~1"
set "UNZIP_BIN="

:: Locate unzip via winepath
for /f "delims=" %%I in ('winepath -w /usr/bin/unzip 2^>nul') do set "UNZIP_BIN=%%I"
if not defined UNZIP_BIN for /f "delims=" %%I in ('winepath -w /bin/unzip 2^>nul') do set "UNZIP_BIN=%%I"

:: Fallback
if not defined UNZIP_BIN if exist "Z:\usr\bin\unzip" set "UNZIP_BIN=Z:\usr\bin\unzip"
if not defined UNZIP_BIN if exist "Z:\bin\unzip" set "UNZIP_BIN=Z:\bin\unzip"

if not defined UNZIP_BIN exit /b 1

set "UNIX_ZIP="
set "UNIX_DEST="

for /f "delims=" %%I in ('winepath -u "%ZIP_WIN%"') do set "UNIX_ZIP=%%I"
for /f "delims=" %%I in ('winepath -u "%ROOT_DIR%"') do set "UNIX_DEST=%%I"

if not defined UNIX_ZIP exit /b 1
if not defined UNIX_DEST exit /b 1

:: Execute using start /b /wait.
:: This runs successfully but might return ErrorLevel 1 due to permission warnings.
start /b /wait "" "%UNZIP_BIN%" -o "%UNIX_ZIP%" -d "%UNIX_DEST%" >nul 2>&1

:: We intentionally return success (0) here. 
:: The main script will verify if files actually exist.
exit /b 0

:extract_with_vbscript
set "ZIP=%~1"
if defined IS_WINE exit /b 1
set "VBS_DIR=%TEMP%"
if not defined VBS_DIR set "VBS_DIR=%UPDATE_DIR%"
if not exist "%VBS_DIR%" set "VBS_DIR=%UPDATE_DIR%"
if not exist "%VBS_DIR%" set "VBS_DIR=%ROOT_DIR%"
set "VBS=%VBS_DIR%\sofbuddy_unzip_%RANDOM%%RANDOM%.vbs"
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
if not exist "%VBS%" exit /b 1

where cscript.exe >nul 2>&1
if errorlevel 1 (
    del /f /q "%VBS%" >nul 2>&1
    exit /b 1
)
cscript.exe //nologo "%VBS%" "%ZIP%" "%ROOT_DIR%" >nul 2>&1
set "RC=%ERRORLEVEL%"
del /f /q "%VBS%" >nul 2>&1

if "%RC%"=="0" exit /b 0
exit /b 1