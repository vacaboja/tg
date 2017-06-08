#!/bin/bash

if test ! -d "$1"
then
	echo Usage: $0 resources-dir
	exit 1
fi

DIR=`dirname "${BASH_SOURCE[0]}"`
ABSDIR=`cd "$DIR"; pwd`

RESOURCES=`cd "$1"; pwd`

TARGET="$ABSDIR/../build/msi"
TMP="$ABSDIR/../build/tmp"

cd "$ABSDIR"/..

VERSION=`cat version`

make

rm -rf "$TARGET"
rm -rf "$TMP"
mkdir -p "$TARGET"
cp "$ABSDIR/tg-timer.wxs" "$TARGET"
cp "$ABSDIR/LICENSE.rtf" "$TARGET"
cp "$ABSDIR/../README.md" "$TARGET"
cp "$ABSDIR/../LICENSE" "$TARGET"
cp "$ABSDIR/../build/tg.exe" "$TARGET"
cp -r "$RESOURCES" "$TMP"
cp "$ABSDIR/../icons/tg-document.ico" "$TMP"
heat dir "$TMP" -srd -gg -sreg -dr INSTALLDIR -cg Resources -out "$TARGET/Resources.wxs"
mv "$TMP"/* "$TARGET"

cd "$TARGET"

candle tg-timer.wxs Resources.wxs
light -out tg-timer_${VERSION}.msi -ext WixUIExtension tg-timer.wixobj Resources.wixobj
