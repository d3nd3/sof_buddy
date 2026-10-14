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
set "PROGRESS_FILE=%UPDATE_DIR%\.progress"
if not exist "%UPDATE_DIR%" mkdir "%UPDATE_DIR%" >nul 2>&1
set "ZIP_PATH="
set "EXTRACT_METHOD="
set "IS_WINE="
set "IS_PROTON="
set "NO_PAUSE="
if defined SOFBUDDY_UPDATER_NO_PAUSE set "NO_PAUSE=1"
call :load_install_context
if not defined IS_WINE reg query "HKCU\Software\Wine" >nul 2>&1 && set "IS_WINE=1"
if not defined SOFBUDDY_SKIP_GUI if defined IS_WINE set "SOFBUDDY_SKIP_GUI=1"
if not defined SOFBUDDY_SKIP_GUI if defined IS_PROTON set "SOFBUDDY_SKIP_GUI=1"

:: GUI updater: native Windows universal only (Wine/Proton use in-DLL extract or zip fallbacks below).
if not defined SOFBUDDY_UPDATER_SILENT if not defined SOFBUDDY_SKIP_GUI if exist "%SCRIPT_DIR%sof_buddy_update.exe" (
    start "" "%SCRIPT_DIR%sof_buddy_update.exe" %*
    popd
    exit /b 0
)

call :report_progress 5 "Preparing..."

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

call :report_progress 12 "Update package ready"
if not defined SOFBUDDY_UPDATER_SILENT (
    echo Applying update from:
    echo   !ZIP_PATH!
    echo.
    echo Make sure Soldier of Fortune is fully closed before continuing.
    echo.
)

call :report_progress 28 "Extracting files..."
call :extract_zip "!ZIP_PATH!"

if not exist "%ROOT_DIR%\sof_buddy.dll" goto :extract_failed
if not exist "%ROOT_DIR%\sof_buddy\update_from_zip.cmd" goto :extract_failed

call :report_progress 72 "Verifying files..."
if not defined SOFBUDDY_UPDATER_SILENT echo.
if not defined SOFBUDDY_UPDATER_SILENT echo Update extraction complete.
if defined EXTRACT_METHOD echo Extraction method: !EXTRACT_METHOD!

if /i "!EXTRACT_METHOD!"=="vbscript" (
    echo Keeping update zip because VBScript extraction is not fully reliable.
    call :print_linux_manual_command "!ZIP_PATH!"
) else (
    echo Removing update zip.
    del /f /q "!ZIP_PATH!" >nul 2>&1
    if exist "!ZIP_PATH!" (
        echo Warning: could not delete "!ZIP_PATH!".
    ) else (
        echo Removed: !ZIP_PATH!
    )
)
call :report_progress 88 "Applying options..."
if /i "%SOFBUDDY_UPDATER_FRESH%"=="1" (
    call :prune_sofbuddy_cfg
) else if not defined SOFBUDDY_UPDATER_SILENT (
    echo.
    set "FRESH=N"
    set /p "FRESH=Start with fresh settings? [y/N]: "
    if /i "!FRESH!"=="y" call :prune_sofbuddy_cfg
)
call :report_progress 100 "Done"
if exist "%UPDATE_DIR%\pending_update.txt" del /f /q "%UPDATE_DIR%\pending_update.txt" >nul 2>&1
if exist "%UPDATE_DIR%\install_context.env" del /f /q "%UPDATE_DIR%\install_context.env" >nul 2>&1
if not defined SOFBUDDY_UPDATER_SILENT (
    echo.
    echo You can now launch SoF normally.
)
popd
if not defined NO_PAUSE pause
exit /b 0

:extract_failed
echo.
echo [ERROR] Failed to extract update zip.
echo.
call :print_linux_manual_command "!ZIP_PATH!"
if not defined IS_WINE (
    echo Try installing 7-Zip and rerun this script:
    echo   https://www.7-zip.org/
)
popd
if not defined NO_PAUSE pause
exit /b 1

:load_install_context
if not exist "%UPDATE_DIR%\install_context.env" exit /b 0
for /f "usebackq tokens=1,* delims==" %%A in ("%UPDATE_DIR%\install_context.env") do (
    set "%%A=%%B"
)
exit /b 0

:extract_zip
set "ZIP=%~1"

where tar.exe >nul 2>&1
if not errorlevel 1 (
    tar.exe -xf "%ZIP%" -C "%ROOT_DIR%" >nul 2>&1
    if not errorlevel 1 (
        set "EXTRACT_METHOD=tar"
        exit /b 0
    )
)

where 7z.exe >nul 2>&1
if not errorlevel 1 (
    7z.exe x -y "%ZIP%" -o"%ROOT_DIR%" >nul 2>&1
    if not errorlevel 1 (
        set "EXTRACT_METHOD=7z"
        exit /b 0
    )
)

if defined HOST_SH if defined HOST_ZIP (
    call :extract_with_host_sh
    if not errorlevel 1 (
        set "EXTRACT_METHOD=host-sh"
        exit /b 0
    )
)

if defined IS_WINE (
    call :extract_with_unix_unzip "%ZIP%"
    set "EXTRACT_METHOD=unix-unzip"
    exit /b 0
)

if not defined IS_WINE (
    call :extract_with_vbscript "%ZIP%"
    if not errorlevel 1 (
        set "EXTRACT_METHOD=vbscript"
        exit /b 0
    )
)

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

:extract_with_host_sh
if not defined HOST_SH exit /b 1
if not defined HOST_ZIP exit /b 1
start /wait /unix "%HOST_SH%" "%HOST_ZIP%"
exit /b %ERRORLEVEL%

:print_linux_manual_command
set "ZIP=%~1"
if not defined HOST_SH for /f "delims=" %%I in ('winepath.exe -u "%ROOT_DIR%\sof_buddy\update_from_zip.sh" 2^>nul') do set "HOST_SH=%%I"
if not defined HOST_ZIP for /f "delims=" %%I in ('winepath.exe -u "%ZIP%" 2^>nul') do set "HOST_ZIP=%%I"
if not defined HOST_SH for /f "delims=" %%I in ('winepath -u "%ROOT_DIR%\sof_buddy\update_from_zip.sh" 2^>nul') do set "HOST_SH=%%I"
if not defined HOST_ZIP for /f "delims=" %%I in ('winepath -u "%ZIP%" 2^>nul') do set "HOST_ZIP=%%I"
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

for /f "delims=" %%I in ('winepath.exe -w /usr/bin/unzip 2^>nul') do set "UNZIP_BIN=%%I"
if not defined UNZIP_BIN for /f "delims=" %%I in ('winepath -w /usr/bin/unzip 2^>nul') do set "UNZIP_BIN=%%I"
if not defined UNZIP_BIN for /f "delims=" %%I in ('winepath -w /bin/unzip 2^>nul') do set "UNZIP_BIN=%%I"

if not defined UNZIP_BIN if exist "Z:\usr\bin\unzip" set "UNZIP_BIN=Z:\usr\bin\unzip"
if not defined UNZIP_BIN if exist "Z:\bin\unzip" set "UNZIP_BIN=Z:\bin\unzip"

if not defined UNZIP_BIN exit /b 1

set "UNIX_ZIP="
set "UNIX_DEST="

for /f "delims=" %%I in ('winepath.exe -u "%ZIP_WIN%" 2^>nul') do set "UNIX_ZIP=%%I"
if not defined UNIX_ZIP for /f "delims=" %%I in ('winepath -u "%ZIP_WIN%"') do set "UNIX_ZIP=%%I"
for /f "delims=" %%I in ('winepath.exe -u "%ROOT_DIR%" 2^>nul') do set "UNIX_DEST=%%I"
if not defined UNIX_DEST for /f "delims=" %%I in ('winepath -u "%ROOT_DIR%"') do set "UNIX_DEST=%%I"

if not defined UNIX_ZIP exit /b 1
if not defined UNIX_DEST exit /b 1

start /b /wait "" "%UNZIP_BIN%" -o "%UNIX_ZIP%" -d "%UNIX_DEST%" >nul 2>&1
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

:report_progress
if not defined SOFBUDDY_UPDATER_SILENT exit /b 0
if not defined PROGRESS_FILE exit /b 0
>"%PROGRESS_FILE%" echo %~1|%~2
exit /b 0

:prune_sofbuddy_cfg
if not defined SOFBUDDY_UPDATER_SILENT echo Searching for old configuration to prune...
for /r "%ROOT_DIR%" %%F in (sofbuddy.cfg) do (
    if exist "%%F" (
        if not defined SOFBUDDY_UPDATER_SILENT echo   Removing: %%F
        del /f /q "%%F" >nul 2>&1
    )
)
exit /b 0
