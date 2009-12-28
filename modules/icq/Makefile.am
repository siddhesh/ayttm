SUBDIRS = libicq

AM_CFLAGS = $(GTK_CFLAGS) -I$(top_srcdir)/src

EXTRA_DIST = icq_mod.h

pkglib_LTLIBRARIES = icq.la
icq_la_SOURCES = icq.c

icq_la_LDFLAGS = -module -avoid-version libicq/*.lo
# Gets rid of the datarootdir warning
datarootdir=@datarootdir@
