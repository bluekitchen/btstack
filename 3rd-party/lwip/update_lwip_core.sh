#/bin/sh
DIR=`dirname $0`
LWIP_SOURCE=/Projects/lwip
LWIP_DEST=$DIR/core
rsync -a $LWIP_SOURCE/ $LWIP_DEST --exclude=contrib --exclude=test --exclude=.vscode --exclude=.git* --exclude=doc
