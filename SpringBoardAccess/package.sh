#!/bin/sh

PACKAGE=SpringBoardAccess

VERSION=0.2
REVISION=`svn info | grep Revision | cut -d " " -f 2`
ARCHIVE=$PACKAGE-$VERSION-$REVISION.deb

echo Creating $PACKAGE package version $VERSION revision $REVISION
sudo rm -rf $PACKAGE

mkdir -p $PACKAGE/DEBIAN
cp control $PACKAGE/DEBIAN
echo "Version: $VERSION-$REVISION" >> $PACKAGE/DEBIAN/control

mkdir -p $PACKAGE/usr/local/bin
cp SpringBoardAccess-test $PACKAGE/usr/local/bin

mkdir -p $PACKAGE/Library/MobileSubstrate/DynamicLibraries
cp SpringBoardAccess.dylib SpringBoardAccess.plist $PACKAGE/Library/MobileSubstrate/DynamicLibraries

# set ownership to root:root
sudo chown -R 0:0 $PACKAGE

echo Packaging $PACKAGE
export COPYFILE_DISABLE
export COPY_EXTENDED_ATTRIBUTES_DISABLE
dpkg-deb -b $PACKAGE $ARCHIVE
dpkg-deb --info $ARCHIVE
