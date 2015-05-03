#!/bin/sh

# get version from repository
tools/get_version.sh

# check if $THEOS is set

# use theos Makefiles ??
cp platforms/ios/example/Makefile.iphone example/daemon/Makefile
