#!/bin/bash

DIR=`dirname "${BASH_SOURCE[0]}"`
ABSDIR=`cd "$DIR"; pwd`

cd $DIR/..

VERSION=`cat version`

git archive HEAD --prefix=tg-timer-$VERSION/ | gzip -c > build/tg-timer_$VERSION.orig.tar.gz

cd build

tar xzf tg-timer_$VERSION.orig.tar.gz
cp -r $ABSDIR/debian tg-timer-$VERSION
cd tg-timer-$VERSION
debuild -us -uc
