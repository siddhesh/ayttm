SUBDIRS = libjabber libxode

EXTRA_DIST = libEBjabber.h

AM_CFLAGS = $(GTK_CFLAGS) -I$(top_srcdir) -I$(top_srcdir)/src -Ilibjabber/include -Ilibxode/include $(EB_CFLAGS)

if MINGW32
WIN32_LDFLAGS = -no-undefined
WIN32_LIBADD = -L../../src -layttm -lintl -lglib-2.0 -lws2_32
endif

pkglib_LTLIBRARIES = jabber.la
jabber_la_SOURCES = jabber.c libEBjabber.c 

jabber_la_LDFLAGS = -module -avoid-version $(WIN32_LDFLAGS)
jabber_la_LIBADD = libjabber/*.lo libxode/*.lo $(WIN32_LIBADD)
