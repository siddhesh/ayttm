noinst_LTLIBRARIES = libmsn2.la

libmsn2_la_SOURCES = msn.c msn-connection.c msn-http.c msn-message.c msn-commands.c \
			msn-soap.c msn-contacts.c msn-login.c msn-util.c msn-sb.c msn-errors.c

noinst_HEADERS = msn.h msn-connection.h msn-message.h msn-soap.h msn-http.h \
		msn-login.h msn-util.h msn-sb.h msn-account.h msn-ext.h msn-contacts.h \
		msn-errors.h

AM_CFLAGS = $(EB_CFLAGS) $(DEBUG_LIBMSN)
AM_CXXFLAGS = $(EB_CFLAGS)

# Gets rid of the datarootdir warning
datarootdir=@datarootdir@
