#!/usr/bin/env sh

# Collect all examples in a single folder
USAGE="Usage: package.sh <build folder> <output folder>"

# command line checks, bash
if [ $# -ne 2 ]; then
        echo ${USAGE}
        exit 0
fi
build_folder=$1
target_folder=$2

echo "[-] Delete intermediate files"
rm -f ${build_folder}/*.in.html

echo "[-] Create output folder"
rm -rf ${target_folder}
mkdir -p ${target_folder}

echo "[-] Copy .html/.js/.wasm files"
cp ${build_folder}/*.html ${build_folder}/*.js ${build_folder}/*.wasm ${target_folder}

echo "[-] All examples are provided in folder ${build_folder}"
echo "[+] Done"