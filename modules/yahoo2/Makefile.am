SUBDIRS = libyahoo2
libdir = $(pkgdatadir)/modules

AM_CFLAGS = $(GTK_CFLAGS) -Ilibyahoo2 -I$(top_srcdir)/src
AM_CPPFLAGS = -DUSE_STRUCT_CALLBACKS=1

lib_LTLIBRARIES = yahoo2.la
yahoo2_la_SOURCES = yahoo.c

yahoo2_la_LDFLAGS = -module -avoid-version libyahoo2/*.lo
