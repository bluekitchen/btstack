#!/bin/sh
echo "Configure BTstack for use with iOS using the theos build system"

# get version from svn
cd src; ./get_version.sh; cd ..

# check if $THEOS is set

# remove autoconf created files
rm -f btstack-config.h config.h layout Makefile src/Makefile example/Makefile ch.ringwald.btstack_*.deb

# use theos Makefiles
cp platforms/ios/btstack-config-iphone.h btstack-config.h
cp platforms/ios/Makefile.iphone Makefile
cp platforms/ios/src/Makefile.iphone src/Makefile
cp platforms/ios/example/Makefile.iphone example/Makefile
ln -s platforms/ios/layout .