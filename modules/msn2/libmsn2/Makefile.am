noinst_LTLIBRARIES = libmsn2.la

EXTRA_DIST = md5.h msn_bittybits.h msn_core.h msn_interface.h 

libmsn2_la_SOURCES = msn_bittybits.C msn_core.C md5.c 

libmsn2_la_LDFLAGS = -lstdc++ 

AM_CFLAGS = $(EB_CFLAGS) $(DEBUG_LIBMSN)
AM_CXXFLAGS = $(EB_CFLAGS)

msntest:
	@$(CC) -lssl -lcrypto -o msntest ssl.c msn_interface.C msn_core.C msn_bittybits.C md5.c


# Gets rid of the datarootdir warning
datarootdir=@datarootdir@
