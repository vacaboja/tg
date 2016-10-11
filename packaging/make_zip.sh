#!/bin/bash

if test ! -d "$1"
then
	echo Usage: $0 dll-dir
	exit 1
fi

DIR=`dirname "${BASH_SOURCE[0]}"`
ABSDIR=`cd "$DIR"; pwd`

DLLS=`cd "$1"; pwd`

cd "$ABSDIR"/..

VERSION=`cat version`
NAME="tg-timer_$VERSION"
TARGET="$ABSDIR/../build/$NAME"

make

rm -rf "$TARGET"
mkdir -p "$TARGET"
cp "$ABSDIR/../README.md" "$TARGET"
cp "$ABSDIR/../LICENSE" "$TARGET"
cp "$ABSDIR/../build/tg.exe" "$TARGET"
cp "$ABSDIR/../build/tg-lt.exe" "$TARGET"
cp "$DLLS"/* "$TARGET"

cd build
rm -f "${NAME}.zip"
7z a -tzip "${NAME}.zip" "${NAME}"/*
