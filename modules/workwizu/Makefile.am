libdir = $(pkgdatadir)/modules

AM_CFLAGS = $(GTK_CFLAGS) -I$(top_srcdir)/src -I.

EXTRA_DIST = workwizu.h

lib_LTLIBRARIES = workwizu.la
workwizu_la_SOURCES = workwizu.c

workwizu_la_LDFLAGS = -module -avoid-version
