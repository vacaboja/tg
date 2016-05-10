# A program for timing mechanical watches [![Build Status](https://travis-ci.org/vacaboja/tg.svg?branch=master)](https://travis-ci.org/vacaboja/tg)

The program tg is copyright (C) 2015 by Marcello Mamino, and it is
distributed under the GNU GPL license version 2. The full source code of
tg is available at https://github.com/vacaboja/tg

## Install instructions

The source code can probably be built by any C99 compiler, but only gcc
and clang have been tested. You need the following libraries: gtk2,
portaudio2, fftw3. There are Windows binaries, an install script for OS X,
and more detailed instructions for Debian-based Linux distributions (e.g.
Ubuntu).

### Windows

Binaries can be found at http://ciovil.li/tg.zip

### Macintosh

A formula for the Homebrew package manager has been prepared by GitHub
user [dmnc](https://github.com/dmnc). To use it, you need to install
Homebrew first (instructions on http://brew.sh), then run the following
command

	brew doctor

Thew brew doctor might ask you to install some additional software, and
this must be done. Then, finally, run

	brew install dmnc/horology/tg --HEAD

### Debian or Debian-based (e.g. Ubuntu)

	sudo apt-get install libgtk2.0-dev libjack-jackd2-dev portaudio19-dev libfftw3-dev git
	git clone https://github.com/vacaboja/tg.git
	cd tg
	make

The package libjack-jackd2-dev is not necessary, it only works around a
known bug (https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=718221).
