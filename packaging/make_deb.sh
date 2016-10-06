#!/bin/bash

DIR=`dirname "${BASH_SOURCE[0]}"`
ABSDIR=`cd "$DIR"; pwd`

cd "$DIR"/..

VERSION=`cat version`

cd build
rm -rf deb
mkdir deb
cp tg-timer_"$VERSION".tar.gz deb/tg-timer_"$VERSION".orig.tar.gz

cd deb

tar xzf tg-timer_"$VERSION".orig.tar.gz
cp -r "$ABSDIR"/debian tg-timer-"$VERSION"
cd tg-timer-"$VERSION"
debuild -us -uc
