#!/bin/sh
mkdir -p config
aclocal -I config
autoconf
automake --add-missing
