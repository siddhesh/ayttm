DIST_SUBDIRS = pixmaps po m4 libproxy sounds doc src modules
SUBDIRS = pixmaps po m4 libproxy sounds doc src modules
ACLOCAL_AMFLAGS = -I m4

Utilitiesdir = $(datadir)/applnk/Internet
Utilities_DATA = @PACKAGE@.desktop

gnomedir = $(datadir)/gnome/apps/Internet
gnome_DATA = @PACKAGE@.desktop

sysconf_DATA = @PACKAGE@rc

# distdir = $(PACKAGE)-$(VERSION)-$(RELEASE)

# most of these are for building on windows
EXTRA_DIST = config.rpath  ayttm.spec.in ayttm.spec gen README.AIX README.LINUX README.WIN32 mingw # mingw32
CLEANFILES = $(Utilities_DATA) $(gnome_DATA) $(sysconf_DATA)

@PACKAGE@.desktop: Makefile
	@rm -f @PACKAGE@.desktop >/dev/null 2>&1;
	@echo [Desktop Entry] > @PACKAGE@.desktop;
	@echo Name=@PACKAGE@ >> @PACKAGE@.desktop;
	@echo Exec=${bindir}/@PACKAGE@ >> @PACKAGE@.desktop;
	@echo Icon=${datadir}/pixmaps/ayttm.png >> @PACKAGE@.desktop;
	@echo Comment=\"The GTK Unified Messaging Client\">>@PACKAGE@.desktop;
	@echo Terminal=0 >> @PACKAGE@.desktop;
	@echo Type=Application >> @PACKAGE@.desktop;
	@echo "Categories=Network;InstantMessaging" >> @PACKAGE@.desktop;

@PACKAGE@rc: Makefile
	@rm -f @PACKAGE@rc >/dev/null 2>&1;
	@echo version=\"@VERSION@-@RELEASE@\" > @PACKAGE@rc
if MINGW32
	@echo default_arrive_snd=sounds\\BuddyArrive.au >> @PACKAGE@rc
	@echo default_leave_snd=sounds\\BuddyLeave.au >> @PACKAGE@rc
	@echo default_send_snd=sounds\\Send.au >> @PACKAGE@rc
	@echo default_recv_snd=sounds\\Receive.au >> @PACKAGE@rc
	@echo default_module_path=modules >> @PACKAGE@rc
else
	@echo default_arrive_snd=${pkgdatadir}/sounds/BuddyArrive.au >> @PACKAGE@rc
	@echo default_leave_snd=${pkgdatadir}/sounds/BuddyLeave.au >> @PACKAGE@rc
	@echo default_send_snd=${pkgdatadir}/sounds/Send.au >> @PACKAGE@rc
	@echo default_recv_snd=${pkgdatadir}/sounds/Receive.au >> @PACKAGE@rc
	@echo default_module_path=${libdir}/@PACKAGE@ >> @PACKAGE@rc
endif

# Gets rid of the datarootdir warning
datarootdir=@datarootdir@

win32-install:
	cd mingw32; make install

win32-release:
	cd mingw32; make release

ayttm.spec: ChangeLog

dist: ayttm.spec

