#!/bin/bash

for A in `cat sizes`;
do
	mkdir -p ${A}x${A}/apps
	mkdir -p ${A}x${A}/mimetypes
	inkscape -z -e ${A}x${A}/apps/tg-timer.png -w $A -h $A scalable/apps/tg-timer.svg &>/dev/null
	optipng -quiet ${A}x${A}/apps/tg-timer.png
	inkscape -z -e ${A}x${A}/mimetypes/application-x-tg-timer-data.png -w $A -h $A scalable/application-x-tg-timer-data.svg &>/dev/null
	optipng -quiet ${A}x${A}/mimetypes/application-x-tg-timer-data.png
done
