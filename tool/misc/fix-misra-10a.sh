#!/bin/sh

BTSTACK_REL=`dirname $0`'/../..'
BTSTACK_ROOT=`realpath $BTSTACK_REL`

# check coccinelle
command -v spatch >/dev/null 2>&1 || { echo >&2 "spatch from cocinelle required but not installed. Aborting."; exit 1; }

# append u to all literals
spatch --sp-file $BTSTACK_ROOT/tool/misc/append_u_to_constants.cocci --out-place --max-width 200 --dir $BTSTACK_ROOT/src/
spatch --sp-file $BTSTACK_ROOT/tool/misc/append_u_to_constants.cocci --out-place --max-width 200 --dir $BTSTACK_ROOT/3rd-party/

# update only lines that are listed in cstat report
$BTSTACK_ROOT/tool/misc/fix-misra-10a.py

# delete cocci output files
find $BTSTACK_ROOT/ -name "*.cocci_res" -delete
