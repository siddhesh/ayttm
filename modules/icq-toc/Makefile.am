SUBDIRS = libtoc
libdir = $(pkgdatadir)/modules

AM_CFLAGS = $(GTK_CFLAGS) -I$(top_srcdir)/src

EXTRA_DIST = icq.h

lib_LTLIBRARIES = icq-toc.la

icq_toc_la_SOURCES = icq-toc.c
icq_toc_la_LDFLAGS = -module -avoid-version libtoc/libtoc.lo
