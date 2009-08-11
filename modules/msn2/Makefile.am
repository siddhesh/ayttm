SUBDIRS = libmsn2

EXTRA_DIST = 

AM_CFLAGS = $(GTK_CFLAGS) -I$(top_srcdir)/src -Ilibmsn2

if MINGW32
WIN32_LDFLAGS = -no-undefined
WIN32_LIBADD = -L../../src -layttm -lintl -lglib-2.0 -lws2_32
endif

pkglib_LTLIBRARIES = msn2.la
msn2_la_SOURCES = msn.c

msn2_la_LDFLAGS = -module -avoid-version $(WIN32_LDFLAGS)
msn2_la_LIBADD = libmsn2/*.lo $(WIN32_LIBADD)
# Gets rid of the datarootdir warning
datarootdir=@datarootdir@
