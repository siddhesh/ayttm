AM_CFLAGS = $(GTK_CFLAGS) -I$(top_srcdir)/src -DAYTTM_SMILEY_DIR=\"${smileydir}\"

smileydir = ${pkgdatadir}/smileys
smiley_DATA =

pkglib_LTLIBRARIES = smiley-themer.la smileysc.la giles_smiles.la

smiley_themer_la_SOURCES = smiley-themer.c 
smiley_themer_la_LDFLAGS = -module -avoid-version
smiley_themer_la_LIBADD = -lXpm -L/usr/X11R6/lib

smileysc_la_SOURCES = smileysc.c
smileysc_la_LDFLAGS = -module -avoid-version

giles_smiles_la_SOURCES = giles_smiles.c
giles_smiles_la_LDFLAGS = -module -avoid-version
