EXTRA_DIST = select-keys.h
AM_CFLAGS = $(GPGME_CFLAGS) $(GTK_CFLAGS) -I$(top_srcdir)/src 
pkglib_LTLIBRARIES = aycryption.la

aycryption_la_SOURCES = aycryption.c select-keys.c
aycryption_la_LDFLAGS = -module -avoid-version
aycryption_la_LIBADD = $(GPGME_LIBS)
