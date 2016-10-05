#!/bin/bash

DIR=`dirname "${BASH_SOURCE[0]}"`
ABSDIR=`cd "$DIR"; pwd`

cd $DIR/..

VERSION=`cat version`

cd build
cp tg-timer_$VERSION.tar.gz tg-timer_$VERSION.orig.tar.gz
tar xzf tg-timer_$VERSION.orig.tar.gz
cp -r $ABSDIR/debian tg-timer-$VERSION
cd tg-timer-$VERSION
debuild -us -uc
