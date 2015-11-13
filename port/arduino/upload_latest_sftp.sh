#!/bin/bash
USAGE="Usage: upload_latest_sftp.sh host path user"

# command line checks, bash
if [ $# -ne 3 ]; then
        echo ${USAGE}
        exit 0
fi
host=$1
path=$2
user=$3

echo Uploading generated archive to ${host}/${path} with user ${user}

# SFTP is very peculiar: recursive put only works for a single directory
sftp ${user}@${host} << EOF
  put btstack-arduino-latest.zip ${path}
  quit
EOF
