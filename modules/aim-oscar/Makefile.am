SUBDIRS = libfaim

AM_CFLAGS = $(GTKCFLAGS) -Ilibfaim

pkglib_LTLIBRARIES = aim-oscar.la
aim_oscar_la_SOURCES = aim-oscar.c

aim_oscar_la_LDFLAGS = -module -avoid-version libfaim/*.lo
