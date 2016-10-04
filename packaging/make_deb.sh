#!/bin/bash

DIR=`dirname "${BASH_SOURCE[0]}"`
ABSDIR=`cd "$DIR"; pwd`

cd $DIR/..

VERSION=`cat version`

tar czf build/tg-timer_$VERSION.orig.tar.gz * --exclude=".*" --exclude="build/*" --xform="s|\\(.*\\)|tg-timer-$VERSION/\1|"
cd build
tar xzf tg-timer_$VERSION.orig.tar.gz
cp -r $ABSDIR/debian tg-timer-$VERSION
cd tg-timer-$VERSION
debuild -us -uc
