EXTRA_DIST = 

@PACKAGE@rc: Makefile
	@rm -f @PACKAGE@rc >/dev/null 2>&1;
	@echo version=\"@VERSION@-@RELEASE@\" > @PACKAGE@rc
	@echo default_arrive_snd=\\Program Files\\Ayttm\\BuddyArrive.au >> @PACKAGE@rc
	@echo default_leave_snd=\\Program Files\\Ayttm\\BuddyLeave.au >> @PACKAGE@rc
	@echo default_send_snd=\\Program Files\\Ayttm\\Send.au >> @PACKAGE@rc
	@echo default_recv_snd=\\Program Files\\Ayttm\\Receive.au >> @PACKAGE@rc
	@echo default_module_path=\\Program Files\\Ayttm\\modules >> @PACKAGE@rc

all: ayttmrc

install: ayttmrc
	cp ayttmrc /c
	cp ../src/ayttm.exe /c/Program\ Files/ayttm
	cp /usr/local/bin/lib*.dll /c/Program\ Files/ayttm
	cp /usr/local/lib/libXpm-noX4.dll /c/Program\ Files/ayttm
	cp ../COPYING /c/Program\ Files/ayttm
	cp ../sounds/*.au /c/Program\ Files/ayttm
	mkdir -p /c/Program\ Files/ayttm/modules
	cp ../modules/aim-toc/.libs/*.{a,dll,la} /c/Program\ Files/ayttm/modules
	cp ../modules/icq-toc/.libs/*.{a,dll,la} /c/Program\ Files/ayttm/modules
	cp ../modules/importers/.libs/*.{a,dll,la} /c/Program\ Files/ayttm/modules
	cp ../modules/irc/.libs/*.{a,dll,la} /c/Program\ Files/ayttm/modules
	cp ../modules/jabber/.libs/*.{a,dll,la} /c/Program\ Files/ayttm/modules
	cp ../modules/smileys/.libs/*.{a,dll,la} /c/Program\ Files/ayttm/modules
	mkdir -p /c/Program\ Files/ayttm/smileys
	mkdir -p /c/Program\ Files/ayttm/smileys/console_smileys
	mkdir -p /c/Program\ Files/ayttm/smileys/giles_smiles
	cp ../modules/smileys/console_smileys/*.{xpm,rc} /c/Program\ Files/ayttm/smileys/console_smileys
	cp ../modules/smileys/giles_smiles/*.{xpm,rc} /c/Program\ Files/ayttm/smileys/giles_smiles
	cp ../modules/utility/.libs/*.{a,dll,la} /c/Program\ Files/ayttm/modules
	cp ../modules/yahoo2/.libs/*.{a,dll,la} /c/Program\ Files/ayttm/modules
	cp ../modules/msn2/.libs/*.{a,dll,la} /c/Program\ Files/ayttm/modules
