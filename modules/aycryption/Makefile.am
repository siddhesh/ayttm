EXTRA_DIST = select-keys.h
AM_CFLAGS = $(GTK_CFLAGS)  $(GLIB_CFLAGS) -I$(top_srcdir)/src 
pkglib_LTLIBRARIES = aycryption.la

aycryption_la_SOURCES = aycryption.c select-keys.c
aycryption_la_LDFLAGS = -module -avoid-version
