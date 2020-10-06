#!/usr/bin/env sh

# get commit and tag
tag=`git tag --points-at`
commit=`git rev-parse --short HEAD`

# use tag if available
if [ -z "$tag" ]
then
  version=$commit
else
  version=$tag
fi

# create mkdocs.yml
sed -e "s|VERSION|$version|" mkdocs-template.yml > mkdocs.yml
