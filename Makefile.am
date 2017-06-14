bin_PROGRAMS = tg-timer
tg_timer_SOURCES = src/algo.c \
		   src/audio.c \
		   src/config.c \
		   src/interface.c \
		   src/tg.h

LIBS = $(GTK_LIBS) \
       $(GTHREAD_LIBS) \
       $(PORTAUDIO_LIBS) \
       $(FFTW_LIBS) \
       -lpthread \
       -lm
AM_CPPFLAGS = -DPROGRAM_NAME=\"$(bin_PROGRAMS)\" -DVERSION=\"$(PACKAGE_VERSION)\"
AM_CFLAGS = $(GTK_CFLAGS) \
	    $(GTHREAD_CFLAGS) \
	    $(PORTAUDIO_CFLAGS) \
	    $(FFTW_CFLAGS)

iconthemedir = $(datadir)/icons/hicolor
appicon16dir = $(iconthemedir)/16x16/apps
appicon22dir = $(iconthemedir)/22x22/apps
appicon32dir = $(iconthemedir)/32x32/apps
appicon48dir = $(iconthemedir)/48x48/apps
appicon64dir = $(iconthemedir)/64x64/apps
appicon128dir = $(iconthemedir)/128x128/apps
appicon256dir = $(iconthemedir)/256x256/apps
appiconscalabledir = $(iconthemedir)/scalable/apps
dist_appicon16_DATA = icons/16x16/tg-timer.png
dist_appicon22_DATA = icons/22x22/tg-timer.png
dist_appicon32_DATA = icons/32x32/tg-timer.png
dist_appicon48_DATA = icons/48x48/tg-timer.png
dist_appicon64_DATA = icons/64x64/tg-timer.png
dist_appicon128_DATA = icons/128x128/tg-timer.png
dist_appicon256_DATA = icons/256x256/tg-timer.png
dist_appiconscalable_DATA = icons/scalable/tg-timer.svg

desktopdir = $(datadir)/applications
desktop_DATA = icons/tg-timer.desktop

dist_man_MANS = docs/tg-timer.1

EXTRA_DIST := autogen.sh \
    icons \
    packaging

