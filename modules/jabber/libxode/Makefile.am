INCLUDES = -Iinclude

SUBDIRS = include

noinst_LTLIBRARIES = libxode.la

AM_CFLAGS = $(GLIB_CFLAGS)

libxode_la_SOURCES = \
	xstream.c \
	expat.c \
	xhash.c \
	genhash.c \
	hashtable.c \
	pool.c \
	sha.c \
	snprintf.c \
	socket.c \
	str.c \
	xmlnode.c \
	xmlparse.c \
	xmlrole.c \
	xmltok.c


EXTRA_DIST = xmltok_impl.c xmltok_ns.c
# Gets rid of the datarootdir warning
datarootdir=@datarootdir@
