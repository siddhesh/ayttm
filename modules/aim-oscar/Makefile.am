SUBDIRS = libfaim
libdir = $(pkgdatadir)/modules

AM_CFLAGS = $(GTKCFLAGS) -Ilibfaim

lib_LTLIBRARIES = aim-oscar.la
aim_oscar_la_SOURCES = aim-oscar.c

aim_oscar_la_LDFLAGS = -module -avoid-version libfaim/*.lo
