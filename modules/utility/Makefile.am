AM_CFLAGS = $(GLIB_CFLAGS) -I$(top_srcdir)/src

EXTRA_DIST = 

if MINGW32
WIN32_LDFLAGS = -no-undefined
WIN32_LIBADD = -L../../src -layttm -lintl -lglib-2.0 -lws2_32
endif

pkglib_LTLIBRARIES = notes.la middle.la rainbow.la autotrans.la custmsg.la

notes_la_SOURCES = notes.c
notes_la_LDFLAGS = -module -avoid-version $(WIN32_LDFLAGS)
notes_la_LIBADD = $(WIN32_LIBADD)

middle_la_SOURCES = middle.c
middle_la_LDFLAGS = -module -avoid-version $(WIN32_LDFLAGS)
middle_la_LIBADD = $(WIN32_LIBADD)

rainbow_la_SOURCES = rainbow.c
rainbow_la_LDFLAGS = -module -avoid-version $(WIN32_LDFLAGS)
rainbow_la_LIBADD = $(WIN32_LIBADD)

autotrans_la_SOURCES = autotrans.c
autotrans_la_LDFLAGS = -module -avoid-version $(WIN32_LDFLAGS)
autotrans_la_LIBADD = $(WIN32_LIBADD)

custmsg_la_SOURCES = custom_message.c
custmsg_la_LDFLAGS = -module -avoid-version $(WIN32_LDFLAGS)
custmsg_la_LIBADD = 
