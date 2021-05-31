#!/usr/bin/env sh

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

echo "${version}"
