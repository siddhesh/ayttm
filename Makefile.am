DIST_SUBDIRS = pixmaps po m4 intl libproxy sounds doc src modules mingw32
SUBDIRS = pixmaps po m4 intl libproxy sounds doc src modules @MINGW32@
ACLOCAL_AMFLAGS = -I m4

Utilitiesdir = $(datadir)/applnk/Internet
Utilities_DATA = @PACKAGE@.desktop

gnomedir = $(datadir)/gnome/apps/Internet
gnome_DATA = @PACKAGE@.desktop

sysconf_DATA = @PACKAGE@rc

# distdir = $(PACKAGE)-$(VERSION)-$(RELEASE)

# most of these are for building on windows
EXTRA_DIST = ayttm.spec.in gen README.WIN32 mingw
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

@PACKAGE@rc: Makefile
	@rm -f @PACKAGE@rc >/dev/null 2>&1;
	@echo version=\"@VERSION@-@RELEASE@\" > @PACKAGE@rc
	@echo default_arrive_snd=${pkgdatadir}/sounds/BuddyArrive.au >> @PACKAGE@rc
	@echo default_leave_snd=${pkgdatadir}/sounds/BuddyLeave.au >> @PACKAGE@rc
	@echo default_send_snd=${pkgdatadir}/sounds/Send.au >> @PACKAGE@rc
	@echo default_recv_snd=${pkgdatadir}/sounds/Receive.au >> @PACKAGE@rc
	@echo default_module_path=${libdir}/@PACKAGE@ >> @PACKAGE@rc

ayttm.spec: ChangeLog

dist: ayttm.spec

