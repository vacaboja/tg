#!/bin/bash

DIR=`dirname "${BASH_SOURCE[0]}"`
ABSDIR=`cd "$DIR"; pwd`

cd $DIR/..

VERSION=`cat version`

git archive HEAD --prefix=tg-$VERSION/ | gzip -c > build/tg_$VERSION.orig.tar.gz

cd build

tar xzf tg_$VERSION.orig.tar.gz
cp -r $ABSDIR/debian tg-$VERSION
cd tg-$VERSION
debuild -us -uc
