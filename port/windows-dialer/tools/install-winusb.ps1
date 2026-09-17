# Automated WinUSB Driver Installer for TP-Link UB500 (VID 0x2357 / PID 0x0604)
$DriverDir = Join-Path $PSScriptRoot "driver_package"
& (Join-Path $DriverDir "INSTALL_SILENT_WINUSB.ps1")
