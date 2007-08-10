# Escape space with a backslash. make install would otherwise fail
THEME_NAME= Console\ Smileys
smileydir = ${pkgdatadir}/smileys/"${THEME_NAME}"

smiley_DATA = aysmile.rc \
	blank.xpm blush.xpm confused.xpm cooldude.xpm \
	grin.xpm heyy.xpm kiss.xpm laugh.xpm \
	oh.xpm sad.xpm smile.xpm sob.xpm \
	tongue.xpm wink.xpm worried.xpm

# Somehow directories with space in names are not created without -D
INSTALL = install -D

EXTRA_DIST = $(smiley_DATA)
# Gets rid of the datarootdir warning
datarootdir=@datarootdir@
