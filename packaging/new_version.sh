#!/bin/bash

DIR=`dirname "${BASH_SOURCE[0]}"`

VERSION=$1

cd "$DIR"
echo "$VERSION" > ../version
./make_wxs.sh
dch -m -v "$VERSION-1"
