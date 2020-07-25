#!/bin/sh
set -e

LOCAL_DIR=`dirname $0`
BTSTACK_ROOT=`realpath $LOCAL_DIR/../..`

# build image if it doesn't exist
if [[ "$(docker images -q fuzz 2> /dev/null)" == "" ]]; then
  	echo "Image for libfuzz does not exist, creating it now..."
	docker image build --no-cache -t fuzz .
fi

# enter fuzz container and bring repo along
docker run --rm -ti -v $BTSTACK_ROOT:/btstack  -w /btstack/test/fuzz fuzz

