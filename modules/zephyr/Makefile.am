SUBDIRS = zephyr
libdir = $(pkgdatadir)/modules

AM_CFLAGS = $(ZEPHYR_CFLAGS) $(GTK_CFLAGS) -I$(top_srcdir)/src -Izephyr/h

lib_LTLIBRARIES = zephyr.la
zephyr_la_SOURCES = zephyr_mod.c

zephyr_la_LDFLAGS = -module zephyr/*.lo -avoid-version
