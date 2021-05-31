#!/usr/bin/env sh

# get commit and tag
tag=`git tag --points-at HEAD`
commit=`git rev-parse --short HEAD`
branch=`git branch | sed -n -e 's/^\* \(.*\)/\1/p'`

# use tag if available
if [ -z "$tag" ]
then
  version="$branch-$commit"
else
  version="Version $tag ($commit)"
fi

# create mkdocs.yml
sed -e "s|VERSION|$version|" btstack_gettingstarted.tex > $1/btstack_gettingstarted.tex
