AC_INIT([tg], [0.4.0], [mamino@lix.polytechnique.fr], [tg], [https://github.com/vacaboja/tg])
AM_INIT_AUTOMAKE([foreign subdir-objects])

AC_PROG_CC
AC_CHECK_LIB([pthread], [pthread_mutex_init], [], [AC_MSG_ERROR([pthread not found])])
AC_CHECK_LIB([m], [sqrt], [], [AC_MSG_ERROR([libm not found])])
PKG_CHECK_MODULES([GTK], [gtk+-2.0 glib-2.0])
PKG_CHECK_MODULES([GTHREAD], [gthread-2.0])
PKG_CHECK_MODULES([PORTAUDIO], [portaudio-2.0])
PKG_CHECK_MODULES([FFTW], [fftw3f])

AC_OUTPUT([Makefile])

