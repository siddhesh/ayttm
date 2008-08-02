SUBDIRS = libirc

pkglib_LTLIBRARIES = irc.la

AM_CFLAGS = $(GTK_CFLAGS) -I$(top_srcdir)/src -Ilibirc

if MINGW32
WIN32_LDFLAGS = -no-undefined
WIN32_LIBADD = -L../../src -layttm -lintl -lglib-2.0 -lws2_32 -lgtk
endif

irc_la_SOURCES = irc.c

irc_la_LDFLAGS = -module -avoid-version $(WIN32_LDFLAGS)
irc_la_LIBADD = libirc/*.lo $(WIN32_LIBADD)
# Gets rid of the datarootdir warning
datarootdir=@datarootdir@
