EXTRA_DIST = aim-toc.la autotrans.la config.h gdk-pixbuf.m4 giles_smiles.la glib.m4 gtk.m4 icq.la icq-toc.la import_gaim.la import_gnomeicu.la importicq.la importlicq.la irc.la jabber.la Makefile.am middle.la msn2.la msn_smileys.la notes.la rainbow.la smileysc.la yahoo2.la yahoo_smileys.la gdk/win32/gdkprivate-win32.h

@PACKAGE@rc: Makefile
	@rm -f @PACKAGE@rc >/dev/null 2>&1;
	@echo version=\"@VERSION@-@RELEASE@\" > @PACKAGE@rc
	@echo default_arrive_snd=\\\\Program\\ Files\\\\Ayttm\\\\BuddyArrive.au >> @PACKAGE@rc
	@echo default_leave_snd=\\\\Program\\ Files\\\\Ayttm\\\\BuddyLeave.au >> @PACKAGE@rc
	@echo default_send_snd=\\\\Program\\ Files\\\\Ayttm\\\\Send.au >> @PACKAGE@rc
	@echo default_recv_snd=\\\\Program\\ Files\\\\Ayttm\\\\Receive.au >> @PACKAGE@rc
	@echo default_module_path=\\\\Program\\ Files\\\\Ayttm\\\\modules >> @PACKAGE@rc

all: ayttm.exe plugins

ayttm.exe: ayttmrc
	./makeebdll
	cp ayttmrc /cygdrive/c
	cp ayttm.exe /cygdrive/c/Program\ Files/ayttm

plugins: modules sound importers utilities aim-toc icq-toc irc yahoo2 jabber smtp smileys

modules:
	rm -f /cygdrive/c/Program\ Files/ayttm/modules/*.la
#	cp *.la /cygdrive/c/Program\ Files/ayttm/modules

sound:
	cp sounds/*.au /cygdrive/c/Program\ Files/ayttm

importers:
	for x in import_gaim import_gnomeicu importicq importlicq import_everybuddy
	do
                ./makedll ../modules/importers/$x $(CC)
		cp $x.dll /cygdrive/c/Program\ Files/ayttm/modules
		cp $x.la /cygdrive/c/Program\ Files/ayttm/modules
	done

utilities:
	for x in rainbow autotrans # notes middle
	do
                ./makedll ../modules/utility/$x $(CC)
                cp $x.dll /cygdrive/c/Program\ Files/ayttm/modules
		cp $x.la /cygdrive/c/Program\ Files/ayttm/modules
	done

aim-toc:
	./makedll ../modules/aim-toc/aim-toc $(CC) ../modules/aim-toc/libtoc/libtoc.o
	cp aim-toc.dll /cygdrive/c/Program\ Files/ayttm/modules
	cp aim-toc.la /cygdrive/c/Program\ Files/ayttm/modules

icq-toc:
	./makedll ../modules/icq-toc/icq-toc $(CC) ../modules/icq-toc/libtoc/libtoc.o
	cp icq-toc.dll /cygdrive/c/Program\ Files/ayttm/modules
	cp icq-toc.la /cygdrive/c/Program\ Files/ayttm/modules

yahoo2:
	./makedll ../modules/yahoo/yahoo $(CC) ../modules/yahoo/libyahoo2/*.o
	cp yahoo.dll /cygdrive/c/Program\ Files/ayttm/modules
	cp yahoo2.la /cygdrive/c/Program\ Files/ayttm/modules

irc:
	./makedll ../modules/irc/irc $(CC)
	cp irc.dll /cygdrive/c/Program\ Files/ayttm/modules
	cp irc.la /cygdrive/c/Program\ Files/ayttm/modules

	./makedll ../modules/msn2/msn c++ ../modules/msn2/libmsn2/*.o
	cp msn.dll /cygdrive/c/Program\ Files/ayttm/modules
	cp msn2.la /cygdrive/c/Program\ Files/ayttm/modules

jabber:
	./makedll ../modules/jabber/jabber $(CC) ../modules/jabber/libEBjabber.o ../modules/jabber/libjabber/*.o ../modules/jabber/libxode/*.o
	cp jabber.dll /cygdrive/c/Program\ Files/ayttm/modules
	cp jabber.la /cygdrive/c/Program\ Files/ayttm/modules

smtp:

smileys:
