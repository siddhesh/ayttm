SUBDIRS = libmsn2

EXTRA_DIST = 

AM_CXXFLAGS = $(GTK_CFLAGS) -I$(top_srcdir)/src -Ilibmsn2

pkglib_LTLIBRARIES = msn2.la
msn2_la_SOURCES = msn.C

msn2_la_LDFLAGS = -module libmsn2/*.lo -lstdc++ -avoid-version
