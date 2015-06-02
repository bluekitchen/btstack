#!/bin/bash
USAGE="Usage: upload_site_sftp.sh host path user"

# command line checks, bash
if [ $# -ne 3 ]; then
        echo ${USAGE}
        exit 0
fi
host=$1
path=$2
user=$3

echo Uploading generated docs to ${host}/${path}/btstack-arduino with user ${user}

# SFTP is very peculiar: recursive put only works for a single directory
sftp ${user}@${host} << EOF
  mkdir ${path}/btstack-arduino
  put -r btstack-arduino ${path}
  quit
EOF
