#!/bin/sh
echo "Configure BTstack for use with iOS using the theos build system"

# get version from svn
cd src; ./get_version.sh; cd ..

# check if $THEOS is set

# remove autoconf created files
rm -f btstack-config.h config.h Makefile src/Makefile example/Makefile

# use theos Makefiles
ln -s btstack-config-iphone.h btstack-config.h
ln -s Makefile.iphone Makefile
ln -s Makefile.iphone src/Makefile
ln -s Makefile.iphone example/Makefile
