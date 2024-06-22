#!/bin/sh

if [ $# -ne 2 ]; then
    echo "Not enough arguments"
    exit 1
fi
# Finder script for assignment 1
filesdir=$1
searchstr=$2

if ! [ -d "$filesdir" ]; then
    echo "$filesdir is not a directory"
    exit 1
fi

filecount="$(find $filesdir -type f -exec grep -l $searchstr {} \; | wc -l)"
linecount="$(find $filesdir -type f -exec grep $searchstr {} \; | wc -l)"

echo "The number of files are $filecount and the number of matching lines are $linecount"
