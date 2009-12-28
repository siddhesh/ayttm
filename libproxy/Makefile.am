noinst_LIBRARIES = libproxy.a

EXTRA_DIST =   networking.h proxy.h net_constants.h
AM_CFLAGS = $(GLIB_CFLAGS)


libproxy_a_SOURCES = proxy.c common.c networking.c ssl.c ssl_certificate.c

noinst_HEADERS = proxy_private.h common.h ssl.h ssl_certificate.h

# Gets rid of the datarootdir warning
datarootdir=@datarootdir@
