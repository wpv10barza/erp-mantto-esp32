@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0PCU-ERP-MANTTO-ESP32.ps1" %*
set EXITCODE=%ERRORLEVEL%
echo.
if not "%EXITCODE%"=="0" echo PCU finalizo con error %EXITCODE%.
pause
exit /b %EXITCODE%
