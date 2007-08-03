AM_CFLAGS = $(GTK_CFLAGS) -I$(top_srcdir)/src

EXTRA_DIST = 

if MINGW32
WIN32_LDFLAGS = -no-undefined
WIN32_LIBADD = -L../../src -layttm -lintl -lglib-2.0 -lws2_32 -lgtk
endif

pkglib_LTLIBRARIES = import_gaim.la import_gnomeicu.la importlicq.la import_everybuddy.la importicq.la 

import_gaim_la_SOURCES = import_gaim.c
import_gaim_la_LDFLAGS = -module -avoid-version $(WIN32_LDFLAGS)
import_gaim_la_LIBADD = $(WIN32_LIBADD)

import_gnomeicu_la_SOURCES = import_gnomeicu.c
import_gnomeicu_la_LDFLAGS = -module -avoid-version $(WIN32_LDFLAGS)
import_gnomeicu_la_LIBADD = $(WIN32_LIBADD)

importicq_la_SOURCES = importicq.c
importicq_la_LDFLAGS = -module -avoid-version $(WIN32_LDFLAGS)
importicq_la_LIBADD = $(WIN32_LIBADD)

importlicq_la_SOURCES = importlicq.c
importlicq_la_LDFLAGS = -module -avoid-version $(WIN32_LDFLAGS)
importlicq_la_LIBADD = $(WIN32_LIBADD)

import_everybuddy_la_SOURCES = import_everybuddy.c
import_everybuddy_la_LDFLAGS = -module -avoid-version $(WIN32_LDFLAGS)
import_everybuddy_la_LIBADD = $(WIN32_LIBADD)
# Gets rid of the datarootdir warning
datarootdir=@datarootdir@
