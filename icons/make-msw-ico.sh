#!/bin/bash

SIZES=`cat sizes`

PNGS=`for A in $SIZES; do echo $[A]x$[A]/tg-timer.png; done`

icotool -c -o tg-timer.ico $PNGS

PNGS=`for A in $SIZES; do echo $[A]x$[A]/tg-document.png; done`

icotool -c -o tg-document.ico $PNGS
