#!/bin/sh
DIR=`dirname $0`
BTSTACK_ROOT="/Projects/btstack/"
# call to build image
# docker image build -t fuzz .
docker run --rm -ti -v $BTSTACK_ROOT:/btstack  -w /btstack/test/fuzz fuzz

