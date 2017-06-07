#!/bin/bash

for A in `cat sizes`;
do
	if test ! -d $[A]x$[A]
	then
		mkdir $[A]x$[A]
	fi
	inkscape -z -e $[A]x$[A]/tg-timer.png -w $A -h $A scalable/tg-timer.svg
	inkscape -z -e $[A]x$[A]/tg-document.png -w $A -h $A scalable/tg-document.svg
done
