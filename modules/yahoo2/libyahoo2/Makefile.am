noinst_LTLIBRARIES = libyahoo2.la

noinst_HEADERS = md5.h yahoo_debug.h yahoo_util.h sha1.h yahoo_fn.h

AM_CFLAGS = $(GLIB_CFLAGS)
AM_CPPFLAGS = -DUSE_STRUCT_CALLBACKS=1
LIBS = @LIBS@ $(GLIB_LIBS)

libyahoo2_la_SOURCES = libyahoo2.c crypt.c md5.c sha1.c \
	yahoo_httplib.c yahoo_util.c yahoo_list.c yahoo_fn.c

pkginclude_HEADERS = yahoo2.h yahoo2_callbacks.h yahoo2_types.h \
	yahoo_httplib.h yahoo_list.h yahoo_fn.h

