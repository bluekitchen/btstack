#!/bin/sh

echo Creating SpringBoardAccess package
rm -rf SpringBoardAccess

mkdir -p SpringBoardAccess/DEBIAN
cp control SpringBoardAccess/DEBIAN

mkdir -p SpringBoardAccess/usr/local/bin
cp SpringBoardAccess-test SpringBoardAccess/usr/local/bin

mkdir -p SpringBoardAccess/Library/MobileSubstrate/DynamicLibraries
export CODESIGN_ALLOCATE=/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/codesign_allocate ; ldid -S SpringBoardAccess.dylib
export CODESIGN_ALLOCATE=/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/codesign_allocate ; ldid -S SpringBoardAccess-test
cp SpringBoardAccess.dylib SpringBoardAccess.plist SpringBoardAccess/Library/MobileSubstrate/DynamicLibraries

echo Packaging SpringBoardAccess
export COPYFILE_DISABLE
export COPY_EXTENDED_ATTRIBUTES_DISABLE
dpkg-deb -b SpringBoardAccess
ls -la SpringBoardAccess.deb
