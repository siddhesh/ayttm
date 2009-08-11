noinst_LTLIBRARIES = libmsn2.la

EXTRA_DIST = md5.h msn_bittybits.h msn_core.h msn_interface.h 

libmsn2_la_SOURCES = msn.c msn-connection.c msn-http.c msn-message.c msn-commands.c \
			msn-soap.c msn-contacts.c msn-login.c

noinst_HEADERS = msn.h msn-connection.h msn-message.h msn-soap.h msn-http.h \
		msn-login.h msn-util.h

AM_CFLAGS = $(EB_CFLAGS) $(DEBUG_LIBMSN)
AM_CXXFLAGS = $(EB_CFLAGS)

# Gets rid of the datarootdir warning
datarootdir=@datarootdir@
