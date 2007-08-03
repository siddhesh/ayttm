noinst_LTLIBRARIES = libyahoo2.la

EXTRA_DIST = yahoo2.h yahoo2_types.h md5.h yahoo2_callbacks.h \
	yahoo_httplib.h yahoo_debug.h yahoo_util.h \
	yahoo_list.h sha.h yahoo_fn.h

AM_CFLAGS = $(GLIB_CFLAGS)
AM_CPPFLAGS = -DUSE_STRUCT_CALLBACKS=1

libyahoo2_la_SOURCES = libyahoo2.c crypt.c md5.c sha.c \
	yahoo_httplib.c yahoo_util.c yahoo_list.c yahoo_fn.c

# Gets rid of the datarootdir warning
datarootdir=@datarootdir@
