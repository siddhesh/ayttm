AM_CFLAGS = $(GTK_CFLAGS) -I$(top_srcdir)/src -DAYTTM_SMILEY_DIR=\"${smileydir}\"

smileydir = ${pkgdatadir}/smileys
smiley_DATA =

pkglib_LTLIBRARIES = smiley-themer.la

smiley_themer_la_SOURCES = smiley-themer.c 
smiley_themer_la_LDFLAGS = -module -avoid-version
smiley_themer_la_LIBADD = -lXpm -L/usr/X11R6/lib
