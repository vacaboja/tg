#!/bin/bash

DIR=`dirname "${BASH_SOURCE[0]}"`
ABSDIR=`cd "$DIR"; pwd`

cd $DIR/..

VERSION=`cat version`

tar czf build/tg-timer_$VERSION.tar.gz * --exclude=".*" --exclude="build/*" --exclude="packaging" --xform="s|\\(.*\\)|tg-timer-$VERSION/\1|"
