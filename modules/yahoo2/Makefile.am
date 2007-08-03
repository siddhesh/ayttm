SUBDIRS = libyahoo2

AM_CFLAGS = $(GTK_CFLAGS) -Ilibyahoo2 -I$(top_srcdir)/src
AM_CPPFLAGS = -DUSE_STRUCT_CALLBACKS=1

if MINGW32
WIN32_LDFLAGS = -no-undefined
WIN32_LIBADD = -L../../src -layttm -lintl -lglib-2.0 -lws2_32
endif

pkglib_LTLIBRARIES = yahoo2.la
yahoo2_la_SOURCES = yahoo.c

yahoo2_la_LDFLAGS = -module -avoid-version $(WIN32_LDFLAGS)
yahoo2_la_LIBADD = libyahoo2/*.lo $(WIN32_LIBADD)
# Gets rid of the datarootdir warning
datarootdir=@datarootdir@
