#!/bin/bash

SIZES=`cat sizes`

PNGS=`for A in $SIZES; do echo ${A}x${A}/apps/tg-timer.png; done`

icotool -c -o tg-timer.ico $PNGS

PNGS=`for A in $SIZES; do echo ${A}x${A}/mimetypes/application-x-tg-timer-data.png; done`

icotool -c -o tg-document.ico $PNGS
