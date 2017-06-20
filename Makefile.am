SUBDIRS = icons
bin_PROGRAMS = tg-timer
EXTRA_PROGRAMS = tg-timer-dbg \
		 tg-timer-prf \
		 tg-timer-vlg
MOSTLYCLEANFILES := $(EXTRA_PROGRAMS)

tg_timer_SOURCES = src/algo.c \
		   src/audio.c \
		   src/computer.c \
		   src/config.c \
		   src/interface.c \
		   src/output_panel.c \
		   src/serializer.c \
		   src/tg.h
if HAVE_WINDRES
tg_timer_SOURCES += tg-timer.res
endif

tg_timer_dbg_SOURCES = $(tg_timer_SOURCES)
tg_timer_prf_SOURCES = $(tg_timer_SOURCES)
tg_timer_vlg_SOURCES = $(tg_timer_SOURCES)

LIBS = $(GTK_LIBS) \
       $(GTHREAD_LIBS) \
       $(PORTAUDIO_LIBS) \
       $(FFTW_LIBS) \
       -lpthread \
       -lm

AM_CPPFLAGS = -DPROGRAM_NAME=\"tg-timer\" -DVERSION=\"$(PACKAGE_VERSION)\"
tg_timer_dbg_CPPFLAGS = $(AM_CPPFLAGS) -DDEBUG

AM_CFLAGS = $(GTK_CFLAGS) \
	    $(GTHREAD_CFLAGS) \
	    $(PORTAUDIO_CFLAGS) \
	    $(FFTW_CFLAGS)
tg_timer_dbg_CFLAGS = $(AM_CFLAGS) -ggdb
tg_timer_prf_CFLAGS = $(AM_CFLAGS) -pg
tg_timer_vlg_CFLAGS = $(AM_CFLAGS) -g

desktopdir = $(datadir)/applications
desktop_DATA = icons/tg-timer.desktop

dist_man_MANS = docs/tg-timer.1

xdgdatadir = @XDGDATADIR@
mimedir = $(xdgdatadir)/mime
xmldir = $(mimedir)/packages
xml_DATA = icons/tg-timer.xml

EXTRA_DIST := autogen.sh \
	      icons \
	      packaging

test: tg-timer-dbg
	./tg-timer-dbg test
.PHONY: test

tg-timer.res: icons/tg-timer.rc icons/tg-timer.ico
	windres icons/tg-timer.rc -O coff -o $@

valgrind: tg-timer-vlg
	valgrind --leak-check=full -v --num-callers=99 --suppressions=.valgrind.supp ./$^
.PHONY: valgrind

