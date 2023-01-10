#!/usr/bin/env sh

# get absolute path
TOOL_DIR=`dirname "$0"`
BTSTACK_ROOT=`realpath ${TOOL_DIR}/..`

# get tag from git
tag=`git tag --points-at HEAD`

# get git version
commit=`git rev-parse --short HEAD`

# use tag if available
if [ -z "$tag" ]
then
    version=${commit}
else
    version=${tag}-${commit}
fi

# zip repository
archive_zip="btstack-${version}.zip"
echo Create ${archive_zip}
cd ${BTSTACK_ROOT} && git archive --format=zip -o ${archive_zip} HEAD .

# build HTML documentation
echo Build HTML documentation
cd ${BTSTACK_ROOT}/doc/manual && make update_content html 2&> /dev/null

# add HTML documentation to zip archive
echo Add HTML documentation to zip archive as doc/manual/btstack
cd ${BTSTACK_ROOT} && zip ${archive_zip} doc/manual/btstack > /dev/null

echo Done
