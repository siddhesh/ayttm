SUBDIRS = libfaim

AM_CFLAGS = $(GTK_CFLAGS) -I$(top_srcdir)/src -Ilibfaim/include

pkglib_LTLIBRARIES = aim-oscar.la
aim_oscar_la_SOURCES = aim-oscar.c

aim_oscar_la_LDFLAGS = -module -avoid-version libfaim/src/*.lo

EXTRA_DIST = README
# Gets rid of the datarootdir warning
datarootdir=@datarootdir@
