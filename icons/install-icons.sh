#!/bin/bash

for A in `cat sizes`;
do
	xdg-icon-resource install --size $A $[A]x$[A]/tg-timer.png
done
