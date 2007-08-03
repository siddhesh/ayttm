noinst_LTLIBRARIES = libicq.la

EXTRA_DIST = libicq.h

AM_CFLAGS = $(GTK_CFLAGS) -I$(top_srcdir)/libproxy -I$(top_srcdir)/src

libicq_la_SOURCES = libicq.c	\
	receive.c		\
	send.c			\
	tcp.c			\
	config.c		\
	util.c			\
	icq.h			\
	receive.h		\
	send.h			\
	tcp.h			\
	icqconfig.h		\
	util.h			\
	icqfile.c

# Gets rid of the datarootdir warning
datarootdir=@datarootdir@
