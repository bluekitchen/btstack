#!/bin/sh

echo "Update version info"
pushd .
cd ../..
tools/get_version.sh
popd

echo "Rebuild clean"
make clean
make

echo "Build Java Classes"
pushd .
cd ../../java
ant jar
popd

echo "Create Archive"
pushd .
cd ..
rm -f btstack-android-mtk.tar.gz
tar cfz btstack-android-mtk.tar.gz mtk ../java ../LEScan ../SPPClient
popd


