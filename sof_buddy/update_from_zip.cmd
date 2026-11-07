@echo off
setlocal EnableExtensions EnableDelayedExpansion

:parse_args
if /i "%~1"=="--reset-features" (
    set "SOFBUDDY_UPDATER_RESET_FEATURES=1"
    shift
    goto :parse_args
)
if /i "%~1"=="--fresh-settings" (
    set "SOFBUDDY_UPDATER_FRESH=1"
    shift
    goto :parse_args
)

:: Run from a temp copy: extraction replaces update_from_zip.cmd, and cmd.exe
:: reads batch files by byte offset while executing. Overwriting the running
:: script corrupts the rest of the install (garbled labels/prompts).
if not defined SOFBUDDY_UPDATER_REEXEC (
    set "SOFBUDDY_UPDATER_REEXEC=1"
    for %%I in ("%~dp0..") do set "SOFBUDDY_UPDATER_ROOT=%%~fI"
    set "REEXEC_DIR="
    if defined TEMP if exist "%TEMP%" set "REEXEC_DIR=%TEMP%"
    if not defined REEXEC_DIR set "REEXEC_DIR=!SOFBUDDY_UPDATER_ROOT!\sof_buddy\update"
    if not exist "!REEXEC_DIR!" mkdir "!REEXEC_DIR!" >nul 2>&1
    set "REEXEC_SCRIPT=!REEXEC_DIR!\sofbuddy_update_%RANDOM%%RANDOM%.cmd"
    copy /y "%~f0" "!REEXEC_SCRIPT!" >nul 2>&1
    if exist "!REEXEC_SCRIPT!" (
        call "!REEXEC_SCRIPT!" %* & set "REEXEC_RC=!ERRORLEVEL!" & del /f /q "!REEXEC_SCRIPT!" >nul 2>&1 & exit /b !REEXEC_RC!
    )
)

if not defined SOFBUDDY_UPDATER_ROOT for %%I in ("%~dp0..") do set "SOFBUDDY_UPDATER_ROOT=%%~fI"
if "!SOFBUDDY_UPDATER_ROOT:~-1!"=="\" set "SOFBUDDY_UPDATER_ROOT=!SOFBUDDY_UPDATER_ROOT:~0,-1!"
set "SCRIPT_DIR=%SOFBUDDY_UPDATER_ROOT%\sof_buddy\"
set "ROOT_DIR=%SOFBUDDY_UPDATER_ROOT%"
pushd "%ROOT_DIR%" >nul 2>&1
if errorlevel 1 (
    echo Could not locate SoF root folder.
    if not defined NO_PAUSE pause
    exit /b 1
)
set "UPDATE_DIR=%ROOT_DIR%\sof_buddy\update"
set "PROGRESS_FILE=%UPDATE_DIR%\.progress"
if not exist "%UPDATE_DIR%" mkdir "%UPDATE_DIR%" >nul 2>&1
set "ZIP_PATH="
set "EXTRACT_METHOD="
set "IS_WINE="
set "IS_PROTON="
set "IS_UNIVERSAL="
set "NO_PAUSE="
set "FEATURES_BACKUP="
set "KEEP_FEATURES="
set "FEATURES_ABSENT="
if defined SOFBUDDY_UPDATER_NO_PAUSE set "NO_PAUSE=1"
if defined SOFBUDDY_UPDATER_SILENT set "NO_PAUSE=1"
if defined SOFBUDDY_UPDATER_SILENT set "SOFBUDDY_UPDATER_NO_PAUSE=1"
call :load_install_context
if not defined IS_WINE reg query "HKCU\Software\Wine" >nul 2>&1 && set "IS_WINE=1"
call :detect_universal

:: GUI when _sofbuddy_update_use_gui is on (install_context.env sets SOFBUDDY_SKIP_GUI=0).
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

call :prepare_features
call :report_progress 28 "Extracting files..."
call :extract_zip "!ZIP_PATH!"
set "EXTRACT_RC=!ERRORLEVEL!"
call :finish_features "!EXTRACT_RC!"
if not "!EXTRACT_RC!"=="0" goto :extract_failed

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
    set /p "FRESH=Start with fresh settings (y/N): "
    if /i "!FRESH!"=="y" call :prune_sofbuddy_cfg
)
if /i "%SOFBUDDY_UPDATER_RESET_FEATURES%"=="1" (
    if defined IS_UNIVERSAL call :reset_feature_toggles
) else if not defined SOFBUDDY_UPDATER_SILENT (
    if defined IS_UNIVERSAL (
        echo.
        set "RESET_FEATURES=N"
        set /p "RESET_FEATURES=Expecting a new feature? Reset feature toggles (y/N): "
        if /i "!RESET_FEATURES!"=="y" call :reset_feature_toggles
    )
)
call :report_progress 100 "Done"
call :delete_if_exists "%UPDATE_DIR%\pending_update.txt"
call :delete_if_exists "%UPDATE_DIR%\install_context.env"
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
echo Close Soldier of Fortune completely, then rerun this script.
echo.
call :print_linux_manual_command "!ZIP_PATH!"
if not defined IS_WINE (
    echo If extraction still fails, install 7-Zip and rerun this script:
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

:detect_universal
:: Only universal builds expose feature toggles in-game; never prompt for
:: features.cfg on compile-time channels (Windows/Linux/XP/debug).
set "IS_UNIVERSAL="
if defined SOFBUDDY_IS_UNIVERSAL (
    if "!SOFBUDDY_IS_UNIVERSAL!"=="1" set "IS_UNIVERSAL=1"
    exit /b 0
)
if exist "%ROOT_DIR%\sof_buddy\flavor.txt" (
    set "FLAVOR="
    set /p FLAVOR=<"%ROOT_DIR%\sof_buddy\flavor.txt"
    if /i "!FLAVOR!"=="universal" set "IS_UNIVERSAL=1"
    exit /b 0
)
:: Universal packages have always shipped the graphical updater; older
:: installs predate flavor.txt, so use it as a fallback marker.
if exist "%ROOT_DIR%\sof_buddy\sof_buddy_update.exe" set "IS_UNIVERSAL=1"
exit /b 0

:prepare_features
if not defined IS_UNIVERSAL (
    set "KEEP_FEATURES=1"
    if exist "%ROOT_DIR%\sof_buddy\features.cfg" (
        set "FEATURES_BACKUP=%UPDATE_DIR%\features.cfg.keep"
        copy /y "%ROOT_DIR%\sof_buddy\features.cfg" "!FEATURES_BACKUP!" >nul 2>&1
    ) else (
        set "FEATURES_ABSENT=1"
    )
    exit /b 0
)
set "REPLACE_FEATURES=%SOFBUDDY_UPDATER_REPLACE_FEATURES%"
if not defined REPLACE_FEATURES if defined SOFBUDDY_UPDATER_SILENT set "REPLACE_FEATURES=1"
if defined REPLACE_FEATURES goto :prepare_features_decide
set "REPLACE_FEATURES=Y"
set /p "REPLACE_FEATURES=Update feature settings to this release's defaults? (Y/n): "
if not defined REPLACE_FEATURES set "REPLACE_FEATURES=Y"
:prepare_features_decide
if /i "!REPLACE_FEATURES!"=="0" goto :features_keep
if /i "!REPLACE_FEATURES!"=="n" goto :features_keep
if /i "!REPLACE_FEATURES!"=="no" goto :features_keep
if /i "!REPLACE_FEATURES!"=="false" goto :features_keep
if exist "%ROOT_DIR%\sof_buddy\features.cfg" (
    set "FEATURES_BACKUP=%UPDATE_DIR%\features.cfg.keep"
    copy /y "%ROOT_DIR%\sof_buddy\features.cfg" "!FEATURES_BACKUP!" >nul 2>&1
)
echo Updating feature settings to this release's defaults.
del /f /q "%ROOT_DIR%\sof_buddy\features.cfg" >nul 2>&1
exit /b 0
:features_keep
set "KEEP_FEATURES=1"
if exist "%ROOT_DIR%\sof_buddy\features.cfg" (
    set "FEATURES_BACKUP=%UPDATE_DIR%\features.cfg.keep"
    copy /y "%ROOT_DIR%\sof_buddy\features.cfg" "!FEATURES_BACKUP!" >nul 2>&1
    echo Keeping your current feature settings.
)
exit /b 0

:finish_features
if not defined FEATURES_BACKUP goto :finish_features_absent
if not "%~1"=="0" goto :restore_features
if defined KEEP_FEATURES goto :restore_features
del /f /q "!FEATURES_BACKUP!" >nul 2>&1
set "FEATURES_BACKUP="
exit /b 0
:restore_features
if exist "!FEATURES_BACKUP!" (
    copy /y "!FEATURES_BACKUP!" "%ROOT_DIR%\sof_buddy\features.cfg" >nul 2>&1
    del /f /q "!FEATURES_BACKUP!" >nul 2>&1
)
set "FEATURES_BACKUP="
set "FEATURES_ABSENT="
exit /b 0
:finish_features_absent
if defined FEATURES_ABSENT if "%~1"=="0" del /f /q "%ROOT_DIR%\sof_buddy\features.cfg" >nul 2>&1
set "FEATURES_ABSENT="
exit /b 0

:extract_zip
set "ZIP=%~1"
:: Packages ship updater scripts as .new. Remove stale copies first so their
:: presence after extraction proves the archive was applied.
del /f /q "%ROOT_DIR%\sof_buddy\update_from_zip.cmd.new" >nul 2>&1
del /f /q "%ROOT_DIR%\sof_buddy\update_from_zip.sh.new" >nul 2>&1
if defined IS_WINE call :fill_host_extract_vars "%ZIP%"

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

if defined HOST_SH if defined HOST_ZIP if exist "%ROOT_DIR%\sof_buddy\update_from_zip.sh" (
    call :extract_with_host_sh
    if not errorlevel 1 (
        set "EXTRACT_METHOD=host-sh"
        exit /b 0
    )
)

if defined IS_WINE (
    call :extract_with_unix_unzip "%ZIP%"
    if errorlevel 1 exit /b 1
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
if exist "%UPDATE_DIR%\.extract_ok" del /f /q "%UPDATE_DIR%\.extract_ok" >nul 2>&1
:: Wine's start /unix returns 3 even when the host process succeeds, so use
:: the marker written by update_from_zip.sh instead of %ERRORLEVEL%.
:: Older .sh scripts do not understand --delegated; only pass it when supported.
set "DELEGATE_ARG="
findstr /c:"--delegated" "%ROOT_DIR%\sof_buddy\update_from_zip.sh" >nul 2>&1
if not errorlevel 1 set "DELEGATE_ARG=--delegated"
:: Run through bash -c: Wine's start /unix mishandles a quoted script path
:: containing spaces, while "$@" keeps every later argument intact.
start /wait /unix /bin/bash -c "exec /bin/bash \"$@\"" _ "%HOST_SH%" !DELEGATE_ARG! "%HOST_ZIP%"
:: Wine's start /wait does not reliably wait for /unix children, so poll the
:: marker that update_from_zip.sh writes after a successful extraction.
set "WAIT_COUNT=0"
:wait_host_extract
if exist "%UPDATE_DIR%\.extract_ok" goto :host_extract_ok
if not exist "%ZIP_PATH%" goto :host_extract_ok
set /a WAIT_COUNT+=1
if !WAIT_COUNT! GEQ 60 goto :host_extract_timeout
ping -n 2 127.0.0.1 >nul 2>&1
goto :wait_host_extract
:host_extract_timeout
:: Older .sh scripts do not write the marker; an extracted .new sidecar is
:: still proof that the archive was applied.
if exist "%ROOT_DIR%\sof_buddy\update_from_zip.cmd.new" goto :host_extract_ok
if exist "%ROOT_DIR%\sof_buddy\update_from_zip.sh.new" goto :host_extract_ok
exit /b 1
:host_extract_ok
del /f /q "%UPDATE_DIR%\.extract_ok" >nul 2>&1
exit /b 0

:print_linux_manual_command
set "ZIP=%~1"
if not exist "%ROOT_DIR%\sof_buddy\update_from_zip.sh" exit /b 0
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

:fill_host_extract_vars
set "ZIP_WIN=%~1"
if not defined HOST_ZIP for /f "delims=" %%I in ('winepath.exe -u "%ZIP_WIN%" 2^>nul') do set "HOST_ZIP=%%I"
if not defined HOST_ZIP for /f "delims=" %%I in ('winepath -u "%ZIP_WIN%" 2^>nul') do set "HOST_ZIP=%%I"
if not defined HOST_SH for /f "delims=" %%I in ('winepath.exe -u "%ROOT_DIR%\sof_buddy\update_from_zip.sh" 2^>nul') do set "HOST_SH=%%I"
if not defined HOST_SH for /f "delims=" %%I in ('winepath -u "%ROOT_DIR%\sof_buddy\update_from_zip.sh" 2^>nul') do set "HOST_SH=%%I"
exit /b 0

:extract_with_unix_unzip
set "ZIP_WIN=%~1"
set "UNIX_ZIP="
set "UNIX_DEST="
for /f "delims=" %%I in ('winepath.exe -u "%ZIP_WIN%" 2^>nul') do set "UNIX_ZIP=%%I"
if not defined UNIX_ZIP for /f "delims=" %%I in ('winepath -u "%ZIP_WIN%" 2^>nul') do set "UNIX_ZIP=%%I"
for /f "delims=" %%I in ('winepath.exe -u "%ROOT_DIR%" 2^>nul') do set "UNIX_DEST=%%I"
if not defined UNIX_DEST for /f "delims=" %%I in ('winepath -u "%ROOT_DIR%"') do set "UNIX_DEST=%%I"
if not defined UNIX_ZIP exit /b 1
if not defined UNIX_DEST exit /b 1
:: Wine's start /unix reports 3 even on success, so verify the archive landed.
start /wait /unix /usr/bin/unzip -o "%UNIX_ZIP%" -d "%UNIX_DEST%" >nul 2>&1
start /wait /unix /bin/unzip -o "%UNIX_ZIP%" -d "%UNIX_DEST%" >nul 2>&1
set "WAIT_COUNT=0"
:wait_unix_extract
if exist "%ROOT_DIR%\sof_buddy\update_from_zip.cmd.new" exit /b 0
if exist "%ROOT_DIR%\sof_buddy\update_from_zip.sh.new" exit /b 0
set /a WAIT_COUNT+=1
if !WAIT_COUNT! GEQ 30 exit /b 1
ping -n 2 127.0.0.1 >nul 2>&1
goto :wait_unix_extract

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
(echo %~1^|%~2)>"%PROGRESS_FILE%"
exit /b 0

:delete_if_exists
if not exist "%~1" exit /b 0
del /f /q "%~1" >nul 2>&1
exit /b 0

:reset_feature_toggles
if not defined SOFBUDDY_UPDATER_SILENT echo Searching for saved feature toggles to reset...
if exist "%ROOT_DIR%\sof_buddy\features.cfg" del /f /q "%ROOT_DIR%\sof_buddy\features.cfg" >nul 2>&1
for /r "%ROOT_DIR%" %%F in (sofbuddy_features.cfg) do (
    if exist "%%F" (
        if not defined SOFBUDDY_UPDATER_SILENT echo   Removing: %%F
        del /f /q "%%F" >nul 2>&1
    )
)
for /r "%ROOT_DIR%" %%F in (sofbuddy.cfg) do (
    if exist "%%F" (
        findstr /v /i /c:"_sofbuddy_feature_" "%%F" > "%%F.tmp" 2>nul
        if errorlevel 2 (
            del /f /q "%%F.tmp" >nul 2>&1
        ) else (
            if exist "%%F.tmp" move /y "%%F.tmp" "%%F" >nul 2>&1
        )
    )
)
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
