@echo off
rem Setting windows build environment for max32630-fthr using Maxim SDK tools
rem [ARMCortexToolchain.exe](https://download.analog.com/sds/exclusive/SFW0001500A/ARMCortexToolchain.exe).
rem Note!
rem Initial(pro) version absolutely no sanity checks for the paths.

set MAXIM_PATH=C:\Maxim

echo.
echo Input MAXIM_PATH=(%MAXIM_PATH%)
set "maxim="
set /P maxim=": "
if defined maxim set "MAXIM_PATH=%maxim%"
echo Selected MAXIM_PATH=(%MAXIM_PATH%)
echo.

set TOOLCHAIN_PATH=%MAXIM_PATH%\Toolchain
echo Input TOOLCHAIN_PATH=(%TOOLCHAIN_PATH%)
set "toolchain="
set /P toolchain=": "
if defined toolchain set "TOOLCHAIN_PATH=%toolchain%"
echo Selected TOOLCHAIN_PATH=(%TOOLCHAIN_PATH%)
echo.

set MSYS_PATH=%TOOLCHAIN_PATH%\msys\1.0
echo Input MSYS_PATH=(%MSYS_PATH%)
set "msys="
set /P msys=": "
if defined msys set "MSYS_PATH=%msys%"
echo Selected MSYS_PATH=(%MSYS_PATH%)
echo.

echo Current PATH=%PATH%
echo.

echo Prepend path with %TOOLCHAIN_PATH%\bin;%MSYS_PATH%\bin;"
set /p var="Continue?[Y/(N)]: "
if "%var%"== "" goto :EOF
if %var%== Y goto prepend
if %var%== y goto prepend
else goto :EOF

:prepend
set ORIG_PATH=%PATH%
set PATH=%TOOLCHAIN_PATH%\bin;%MSYS_PATH%\bin;%PATH%
echo %PATH%
echo.
