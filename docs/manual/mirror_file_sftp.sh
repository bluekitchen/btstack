#!/bin/bash
USAGE="Usage: mirror_file_sftp.sh user@host local_path remote_path"

# command line checks, bash
if [ $# -ne 3 ]; then
        echo ${USAGE}
        exit 0
fi
user_host=$1
local_path=$2
remote_path=$3

echo Mirroring file ${local_path} to ${user_host}:${remote_path}

# SFTP is very peculiar: recursive put only works for a single directory
# note: does not work with sftp of OS X 10.11
sftp ${user_host} << EOF
  put ${local_path} ${remote_path}
  quit
EOF
rm -rf tmp/${folder}
