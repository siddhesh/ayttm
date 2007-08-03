noinst_LTLIBRARIES = libayttmgtk.la

libayttmgtk_la_SOURCES = \
	html_text_buffer.c about_window.c \
	gtkspell.c gtkutils.c gtk_tree_view_tooltip.c \
	log_window.C prefs_window.C

noinst_HEADERS = html_text_buffer.h gtkspell.h gtkutils.h \
		 gtk_tree_view_tooltip.h

INCLUDES = -I.. $(GTK_CFLAGS)
AM_CXXFLAGS = $(CFLAGS)

# Gets rid of the datarootdir warning
datarootdir=@datarootdir@
