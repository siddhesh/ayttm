AM_CFLAGS = $(GLIB_CFLAGS) $(JASPER_CFLAGS) -I$(top_srcdir)/src

if MINGW32
WIN32_LDFLAGS = -no-undefined
WIN32_LIBADD = -L../../src -layttm -lintl -lglib-2.0
endif

pkglib_LTLIBRARIES = img2jpc.la 

img2jpc_la_SOURCES = img2jpc.c 
img2jpc_la_LDFLAGS = -module -avoid-version $(WIN32_LDFLAGS)
img2jpc_la_LIBADD = $(JASPER_LIBS) $(WIN32_LIBADD)

