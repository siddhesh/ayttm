EXTRA_DIST = select-keys.h
AM_CFLAGS = $(GPGME_CFLAGS) $(GTK_CFLAGS) -I$(top_srcdir)/src 

if MINGW32
WIN32_LDFLAGS = -no-undefined
WIN32_LIBADD = -L../../src -layttm -lintl -lglib-2.0 -lgtk -lgdk
endif

pkglib_LTLIBRARIES = aycryption.la

aycryption_la_SOURCES = aycryption.c select-keys.c
aycryption_la_LDFLAGS = -module -avoid-version $(WIN32_LDFLAGS)
aycryption_la_LIBADD = $(GPGME_LIBS) $(WIN32_LIBADD)
