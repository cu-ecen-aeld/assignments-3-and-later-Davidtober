#!/bin/sh

if [ $# -ne 2 ]; then
    echo "Not enough arguments"
    exit 1
fi
# Finder script for assignment 1
writefile=$1
writestr=$2

mkdir -p "$(dirname $writefile)"
echo $writestr > $writefile
