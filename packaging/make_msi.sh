#!/bin/bash

if test ! -d "$1"
then
	echo Usage: $0 dll-dir
	exit 1
fi

DIR=`dirname "${BASH_SOURCE[0]}"`
ABSDIR=`cd "$DIR"; pwd`

DLLS=`cd "$1"; pwd`

TARGET="$ABSDIR/../build/msi"

cd "$ABSDIR"/..

VERSION=`cat version`

make clean
make

mkdir -p "$TARGET"
cp "$ABSDIR/tg-timer.wxs" "$TARGET"
cp "$ABSDIR/LICENSE.rtf" "$TARGET"
cp "$ABSDIR/../README.md" "$TARGET"
cp "$ABSDIR/../LICENSE" "$TARGET"
cp "$ABSDIR/../build/tg.exe" "$TARGET"
cp "$ABSDIR/../build/tg-lt.exe" "$TARGET"
cp "$DLLS"/* "$TARGET"
heat dir "$DLLS" -srd -gg -sreg -dr INSTALLDIR -cg Dlls -out "$TARGET/Dlls.wxs"

cd "$TARGET"

candle tg-timer.wxs Dlls.wxs
light -out tg-timer-${VERSION}.msi -ext WixUIExtension tg-timer.wixobj Dlls.wixobj
