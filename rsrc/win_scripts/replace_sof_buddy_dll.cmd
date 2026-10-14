@echo off
setlocal
set "STAGED=%~1"
set "TARGET=%~2"
for /L %%N in (1,1,300) do (
  copy /Y "%STAGED%" "%TARGET%" >nul 2>&1
  if not errorlevel 1 (
    del /Q "%STAGED%" >nul 2>&1
    del /Q "%~f0" >nul 2>&1
    exit /b 0
  )
  ping -n 3 127.0.0.1 >nul
)
exit /b 1
