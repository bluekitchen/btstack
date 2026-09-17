@echo off
setlocal
cd /d "%~dp0"

echo ================================================================
echo   Restoring Default Microsoft Windows Bluetooth Driver
echo ================================================================
echo.

net session >nul 2>&1
if %errorLevel% neq 0 (
    echo [INFO] Requesting Administrator privileges to restore driver...
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0RESTORE_WINDOWS_BLUETOOTH.ps1"

echo.
pause
