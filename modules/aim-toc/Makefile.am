SUBDIRS = libtoc
libdir = $(pkgdatadir)/modules

AM_CFLAGS = $(GTK_CFLAGS) -I$(top_srcdir)/src

EXTRA_DIST = aim.h

lib_LTLIBRARIES = aim-toc.la

aim_toc_la_SOURCES = aim-toc.c
aim_toc_la_LDFLAGS = -module -avoid-version libtoc/libtoc.lo
