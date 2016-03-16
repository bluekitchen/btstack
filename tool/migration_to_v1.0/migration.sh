#!/bin/bash
set -e
set -u

USAGE="Usage: migration.sh src-path dest-path"

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
DIR=`dirname $0`

echo "Creating copy of $SRC at $DEST"
cp -r $SRC/ $DEST

echo "Updating function calls"

# simple function rename
find $DEST -type f -print0 | xargs -0 sed -i -f $DIR/migration.sed

# complext function rename using coccinelle
command -v spatch >/dev/null 2>&1 || { echo >&2 "spatch from cocinelle required but not installed. Aborting."; exit 1; }
spatch --sp-file $DIR/migration.cocci --in-place  --dir $DEST > /dev/null # 2>&1
echo "Done. Good luck!"


