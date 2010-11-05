#!/bin/sh
mkdir -p config
aclocal -Im4
autoconf
automake --add-missing
