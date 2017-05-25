# A program for timing mechanical watches [![Build Status](https://travis-ci.org/vacaboja/tg.svg?branch=master)](https://travis-ci.org/vacaboja/tg)

The program tg is copyright (C) 2015 by Marcello Mamino, and it is
distributed under the GNU GPL license version 2. The full source code of
tg is available at https://github.com/vacaboja/tg

Tg is in development, and there is still no manual. Some info can be found
in this
[thread at WUS](http://forums.watchuseek.com/f6/open-source-timing-software-2542874.html),
in particular the calibration procedure is described at
[this post](http://forums.watchuseek.com/f6/open-source-timing-software-2542874-post29970370.html).

## Install instructions

Tg is known to work under Microsoft Windows, OS X, and Linux. Moreover it
should be possible to compile the source code under most modern UNIX-like
systems. See the sub-sections below for the details.

### Windows

Binaries can be found at https://tg.ciovil.li

### Macintosh

A formula for the Homebrew package manager has been prepared by GitHub
user [dmnc](https://github.com/dmnc). To use it, you need to install
Homebrew first (instructions on http://brew.sh).

Then run the following command to check everything is set up correctly
and follow any instructions it gives you.

	brew doctor

To install tg, run

	brew install dmnc/horology/tg
	
You can now launch tg by typing

	tg-timer &

### Debian or Debian-based (e.g. Mint, Ubuntu)

Binary .deb packages can be downloaded from https://tg.ciovil.li

### Compiling from sources

The source code of tg can probably be built by any C99 compiler, however
only gcc and clang have been tested. You need the following libraries:
gtk+3, portaudio2, fftw3 (all available as open-source).

### Compiling on Debian

To compile tg on Debian

	sudo apt-get install libgtk-3-dev libjack-jackd2-dev portaudio19-dev libfftw3-dev git
	git clone https://github.com/vacaboja/tg.git
	cd tg
	make

The package libjack-jackd2-dev is not necessary, it only works around a
known bug (https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=718221).
