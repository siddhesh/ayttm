SUBDIRS = libjabber libxode

EXTRA_DIST = libEBjabber.h

AM_CFLAGS = $(GTK_CFLAGS) -I$(top_srcdir)/src -Ilibjabber/include -Ilibxode/include

pkglib_LTLIBRARIES = jabber.la
jabber_la_SOURCES = jabber.c libEBjabber.c 

jabber_la_LDFLAGS = -module -avoid-version libjabber/*.lo libxode/*.lo
