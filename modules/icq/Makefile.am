SUBDIRS = libicq
libdir = $(pkgdatadir)/modules

AM_CFLAGS = $(GTK_CFLAGS) -I$(top_srcdir)/src

EXTRA_DIST = icq_mod.h

lib_LTLIBRARIES = icq.la
icq_la_SOURCES = icq.c

icq_la_LDFLAGS = -module -avoid-version libicq/*.lo
