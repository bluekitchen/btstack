#!/bin/sh
for i in `find . -name "*.h"` `find . -name "*.m"` `find . -name "*.c"` 
do
  if ! grep -q Copyright $i
  then
    echo Fixing $i
    cat COPYING $i >$i.new && mv $i.new $i
  fi
done

