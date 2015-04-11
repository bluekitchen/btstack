#!/bin/sh
DIR=`dirname $0`

echo "Update version info"
$DIR/../../tools/get_version.sh

echo "Rebuild clean"
pushd .
cd $DIR
make clean
make
popd

echo "Build Java Classes"
pushd .
cd $DIR/../../java
ant jar
popd

pushd .
cd $DIR/../..
VERSION=`sed -n -e 's/^.*BTSTACK_VERSION \"\(.*\)\"/\1/p' include/btstack/version.h`
ARCHIVE=btstack-android-mtk-$VERSION.tar.gz
echo "Create Archive $ARCHIVE"
rm -f $ARCHIVE
tar cfz $ARCHIVE platforms/mtk java
popd


