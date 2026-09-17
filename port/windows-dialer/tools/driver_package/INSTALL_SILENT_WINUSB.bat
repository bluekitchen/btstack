@echo off
setlocal
cd /d "%~dp0"

echo ================================================================
echo   Silent Automated WinUSB Driver Setup for Dialer
echo ================================================================
echo.

net session >nul 2>&1
if %errorLevel% neq 0 (
    echo [INFO] Requesting Administrator privileges to register driver...
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0INSTALL_SILENT_WINUSB.ps1"

echo.
pause
