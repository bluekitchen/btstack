#!/bin/bash
set -e
set -u

USAGE="Usage: conversion_to_1.0.sh src-path dest-path"

echo "BTstack conversion to v1.0 helper"
echo "BlueKitchen GmbH, 2016"
echo

# command line checks, bash
if [ $# -ne 2 ]; then
        echo ${USAGE}
        exit 0
fi
SRC=$1
DEST=$2

echo "Creating copy of $SRC at $DEST"
cp -r $SRC/ $DEST

echo "Updating function calls"

# simple function rename
find $DEST -type f -print0 | xargs -0 sed -i -f convert.sed

# complext function rename using coccinelle
command -v spatch >/dev/null 2>&1 || { echo >&2 "spatch from cocinelle required but not installed. Aborting."; exit 1; }
spatch --sp-file convert.cocci --in-place  --dir $DEST > /dev/null # 2>&1
echo "Done. Good luck!"


