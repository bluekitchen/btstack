#!/bin/sh
mkdir -p config
aclocal -I m4
autoconf
automake --add-missing
