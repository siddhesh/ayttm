AM_CFLAGS = $(GTK_CFLAGS) -I$(top_srcdir)/src

pkglib_LTLIBRARIES = smtp.la
smtp_la_SOURCES = smtp.c
smtp_la_LDFLAGS = -module -avoid-version

noinst_HEADERS = smtp_online.xpm smtp_away.xpm
# Gets rid of the datarootdir warning
datarootdir=@datarootdir@
