SUBDIRS = pixmaps po m4 intl libproxy sounds doc src modules
ACLOCAL_AMFLAGS = -I m4

Utilitiesdir = $(datadir)/applnk/Internet
Utilities_DATA = @PACKAGE@.desktop

gnomedir = $(datadir)/gnome/apps/Internet
gnome_DATA = @PACKAGE@.desktop

# distdir = $(PACKAGE)-$(VERSION)-$(RELEASE)

EXTRA_DIST = ayttm.spec.in
CLEANFILES = $(Utilities_DATA) $(gnome_DATA)

@PACKAGE@.desktop: Makefile
	@rm -f @PACKAGE@.desktop >/dev/null 2>&1;
	@echo [Desktop Entry] > @PACKAGE@.desktop;
	@echo Name=@PACKAGE@ >> @PACKAGE@.desktop;
	@echo Exec=${bindir}/@PACKAGE@ >> @PACKAGE@.desktop;
	@echo Icon=${datadir}/pixmaps/ayttm.png >> @PACKAGE@.desktop;
	@echo Info=@PACKAGE@ >> @PACKAGE@.desktop;
	@echo Comment=\"The GTK Unified Messaging Client\">>@PACKAGE@.desktop;
	@echo Terminal=0 >> @PACKAGE@.desktop;
	@echo Type=Application >> @PACKAGE@.desktop;

ayttm.spec: ChangeLog

dist: ayttm.spec

