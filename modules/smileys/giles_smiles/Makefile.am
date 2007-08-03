# Escaped the space with a backslash. make install would otherwise fail
THEME_NAME= Giles\ Smileys
smileydir = ${pkgdatadir}smileys/${THEME_NAME}

smiley_DATA = aysmile.rc \
	dead.xpm embarrased.xpm grin.xpm indifferent.xpm \
	kiss.xpm quiet.xpm sad.xpm shades.xpm \
	shocked.xpm smile.xpm tears.xpm tongue.xpm \
	wink.xpm

# Somehow directories with space in names are not created without -D
INSTALL = install -D

EXTRA_DIST = $(smiley_DATA)
# Gets rid of the datarootdir warning
datarootdir=@datarootdir@
