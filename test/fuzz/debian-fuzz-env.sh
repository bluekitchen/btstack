#!/bin/sh
DIR=`dirname $0`
BTSTACK_ROOT=`realpath $DIR/../..`
docker run --rm -ti -v $BTSTACK_ROOT:/btstack  -w /btstack/test/fuzz fuzz

