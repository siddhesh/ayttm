SUBDIRS = libjabber libxode
libdir = $(pkgdatadir)/modules

EXTRA_DIST = libEBjabber.h

AM_CFLAGS = $(GTK_CFLAGS) -I$(top_srcdir)/src -Ilibjabber/include -Ilibxode/include

lib_LTLIBRARIES = jabber.la
jabber_la_SOURCES = jabber.c libEBjabber.c 

jabber_la_LDFLAGS = -module -avoid-version libjabber/*.lo libxode/*.lo
