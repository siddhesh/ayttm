SUBDIRS = include

noinst_LTLIBRARIES = libjabber.la

AM_CFLAGS = $(GLIB_CFLAGS) -I../libxode/include -Iinclude -Iinclude/jabber

libjabber_la_SOURCES = \
	jid.c \
	log.c \
	pproxy.c \
	jutil.c \
	rate.c \
    jpacket.c \
    jconn.c
