#!/bin/sh
if [ -z $1 -o -z $2 ]; then
    echo "Usage: $0 INPUT OUTPUT"
    exit 1
fi
dd if=$1 of=$2 bs=1 skip=52
