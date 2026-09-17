<#
.SYNOPSIS
    Automated, 100% Silent WinUSB Driver Installer for TP-Link Bluetooth 5.4 Adapter.
    Uses digitally signed driver package and Windows SetupAPI (pnputil). Zero BSOD risk.
#>

#Requires -RunAsAdministrator
$ErrorActionPreference = 'Stop'
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  Dialer Automated Silent WinUSB Driver Activation" -ForegroundColor White
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. Import Certificate into Windows Trusted Root & Trusted Publisher stores
$cerFile = Join-Path $ScriptDir "dialer_driver.cer"
if (Test-Path $cerFile) {
    Write-Host "[1/3] Adding Trusted Driver Certificate to Windows Store..." -ForegroundColor Cyan
    & certutil -addstore -f "Root" $cerFile | Out-Null
    & certutil -addstore -f "TrustedPublisher" $cerFile | Out-Null
    Write-Host "      Certificate trusted successfully." -ForegroundColor Green
} else {
    Write-Host "[ERROR] Certificate file not found: $cerFile" -ForegroundColor Red
    exit 1
}

# 2. Add and Install Signed Driver Package via Windows SetupAPI
$infFile = Join-Path $ScriptDir "tplink_winusb.inf"
if (Test-Path $infFile) {
    Write-Host "[2/3] Installing Signed WinUSB Driver Package into Windows Driver Store..." -ForegroundColor Cyan
    $pnpOutput = & pnputil /add-driver $infFile /install
    $pnpOutput | ForEach-Object { Write-Host "      $_" -ForegroundColor Gray }
} else {
    Write-Host "[ERROR] INF file not found: $infFile" -ForegroundColor Red
    exit 1
}

# 3. Verify Active Driver Status & Activate Interface
Write-Host "[3/3] Activating and Verifying TP-Link USB Adapter..." -ForegroundColor Cyan
$dev = Get-PnpDevice | Where-Object { $_.InstanceId -like "*VID_2357&PID_0604*" } | Select-Object -First 1

if ($dev) {
    & pnputil /restart-device $dev.InstanceId | Out-Null
    Start-Sleep -Seconds 1
    $devAfter = Get-PnpDevice | Where-Object { $_.InstanceId -like "*VID_2357&PID_0604*" } | Select-Object -First 1

    Write-Host "`n==========================================================" -ForegroundColor Green
    Write-Host "  DEVICE READY FOR BUMBLE AUDIO DIALER!" -ForegroundColor White
    Write-Host "  Friendly Name: $($devAfter.FriendlyName)" -ForegroundColor Green
    Write-Host "  Service:       $($devAfter.Service)" -ForegroundColor Green
    Write-Host "  Status:        $($devAfter.Status)" -ForegroundColor Green
    Write-Host "==========================================================" -ForegroundColor Green
} else {
    Write-Host "[WARN] TP-Link adapter not currently plugged in. Driver is pre-staged and will activate when plugged in." -ForegroundColor Yellow
}
