SUBDIRS=console_smileys giles_smiles

AM_CFLAGS = $(GTK_CFLAGS) -I$(top_srcdir)/src -DAYTTM_SMILEY_DIR=\"${smileydir}\"

if MINGW32
smileydir = smileys
WIN32_LDFLAGS = -no-undefined
WIN32_LIBADD = -L../../src -layttm -lintl -lglib-2.0
else
smileydir = ${pkgdatadir}/smileys
endif

smiley_DATA =

pkglib_LTLIBRARIES = smiley-themer.la 

smiley_themer_la_SOURCES = smiley-themer.c 
smiley_themer_la_LDFLAGS = -module -avoid-version $(WIN32_LDFLAGS)
smiley_themer_la_LIBADD = -lXpm -L/usr/X11R6/lib $(WIN32_LIBADD)

