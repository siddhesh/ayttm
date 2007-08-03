SUBDIRS = libtoc

AM_CFLAGS = $(GTK_CFLAGS) -I$(top_srcdir)/src

EXTRA_DIST = icq.h

if MINGW32
WIN32_LDFLAGS = -no-undefined
WIN32_LIBADD = -L../../src -layttm -lintl -lglib-2.0 -lws2_32
endif

pkglib_LTLIBRARIES = icq-toc.la

icq_toc_la_SOURCES = icq-toc.c
icq_toc_la_LDFLAGS = -module -avoid-version $(WIN32_LDFLAGS)
icq_toc_la_LIBADD = libtoc/libtoc.lo $(WIN32_LIBADD)
# Gets rid of the datarootdir warning
datarootdir=@datarootdir@
