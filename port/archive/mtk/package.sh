#!/bin/sh
DIR=`dirname $0`

echo "Update version info"
$DIR/../../tool/get_version.sh

echo "Rebuild clean"
pushd .
cd $DIR
make clean
make
popd

echo "Build Java Classes"
pushd .
cd $DIR/../../binding/java
ant jar
popd

pushd .
cd $DIR/../..
VERSION=`sed -n -e 's/^.*BTSTACK_VERSION \"\(.*\)\"/\1/p' platform/daemon/src/btstack_version.h`
ARCHIVE=btstack-android-mtk-$VERSION.tar.gz
echo "Create Archive $ARCHIVE"
rm -f $ARCHIVE
tar cfz $ARCHIVE port/mtk binding/java
popd


