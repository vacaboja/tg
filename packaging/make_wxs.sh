#!/bin/bash

DIR=`dirname "${BASH_SOURCE[0]}"`
ABSDIR=`cd "$DIR"; pwd`

cd $DIR

VERSION=`cat ../version`
VERSIONX=`cat ../version | sed "s/\\([0-9]*\\.[0-9]*\\.[0-9]*\\).*/\\1/"`

cat tg-timer.wxs.template | sed \
's/#UUID#/<?php system("echo -n `uuidgen`");?>/g;'\
"s/#VERSION#/$VERSION/g;"\
"s/#VERSIONX#/$VERSIONX/g;"\
| php > tg-timer.wxs
