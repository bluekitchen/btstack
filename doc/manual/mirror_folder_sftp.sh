#!/bin/bash
USAGE="Usage: mirror_folder_sftp.sh user@host local_path remote_path"

# command line checks, bash
if [ $# -ne 3 ]; then
        echo ${USAGE}
        exit 0
fi
user_host=$1
local_path=$2
remote_path=$3

echo Mirroring folder ${local_path} to ${user_host}:${remote_path}

# SFTP is very peculiar: recursive put only works for a single directory
# note: does not work with sftp of OS X 10.11
folder=`basename ${remote_path}`
path=`dirname ${remote_path}`
rm -rf tmp
mkdir tmp
cp -r ${local_path} tmp/${folder}
sftp ${user_host} << EOF
  mkdir ${remote_path}
  put -r tmp/${folder} ${path}
  quit
EOF
rm -rf tmp/${folder}
