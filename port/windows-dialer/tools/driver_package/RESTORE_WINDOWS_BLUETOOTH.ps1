<#
.SYNOPSIS
    Restores default Microsoft Windows Bluetooth driver for TP-Link Adapter.
#>

#Requires -RunAsAdministrator
$ErrorActionPreference = 'Continue'

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  Restoring Default Microsoft Windows Bluetooth Driver" -ForegroundColor White
Write-Host "==========================================================" -ForegroundColor Cyan

Write-Host "[1/2] Removing custom WinUSB driver package..." -ForegroundColor Cyan
$drivers = & pnputil /enum-drivers
$oemInfs = @()
for ($i = 0; $i -lt $drivers.Count; $i++) {
    if ($drivers[$i] -match "tplink_winusb.inf" -or $drivers[$i] -match "Dialer Systems") {
        if ($drivers[$i - 1] -match "(oem\d+\.inf)") {
            $oemInfs += $Matches[1]
        }
    }
}

foreach ($oem in $oemInfs) {
    Write-Host "      Uninstalling $oem..." -ForegroundColor Yellow
    & pnputil /delete-driver $oem /uninstall /force | Out-Null
}

Write-Host "[2/2] Scanning for Hardware and Re-binding Inbox Bluetooth Driver..." -ForegroundColor Cyan
& pnputil /scan-devices | Out-Null
Start-Sleep -Seconds 2

$dev = Get-PnpDevice | Where-Object { $_.InstanceId -like "*VID_2357&PID_0604*" } | Select-Object -First 1
if ($dev) {
    Write-Host "`n==========================================================" -ForegroundColor Green
    Write-Host "  RESTORED TO DEFAULT WINDOWS BLUETOOTH STACK!" -ForegroundColor White
    Write-Host "  Friendly Name: $($dev.FriendlyName)" -ForegroundColor Green
    Write-Host "  Service:       $($dev.Service)" -ForegroundColor Green
    Write-Host "  Status:        $($dev.Status)" -ForegroundColor Green
    Write-Host "==========================================================" -ForegroundColor Green
}
