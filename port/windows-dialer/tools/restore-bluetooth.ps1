# Restores Microsoft Inbox Bluetooth Driver for TP-Link UB500
$DriverDir = Join-Path $PSScriptRoot "driver_package"
& (Join-Path $DriverDir "RESTORE_WINDOWS_BLUETOOTH.ps1")
