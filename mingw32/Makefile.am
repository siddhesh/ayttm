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
	mkdir -p /c/Program\ Files/ayttm/etc
	cp ../ayttmrc /c/Program\ Files/ayttm/etc
	cp ../src/ayttm.exe /c/Program\ Files/ayttm
	cp /usr/local/bin/lib*.dll /c/Program\ Files/ayttm
	cp /usr/local/lib/libXpm-noX4.dll /c/Program\ Files/ayttm
	mkdir -p /c/Program\ Files/ayttm/lib/gdk-pixbuf/loaders
	cp /usr/local/lib/gdk-pixbuf/loaders/*.dll /c/Program\ Files/ayttm/lib/gdk-pixbuf/loaders
	cp ../README.WIN32 /c/Program\ Files/ayttm
	cp ../COPYING /c/Program\ Files/ayttm
	mkdir -p /c/Program\ Files/ayttm/sounds
	cp ../sounds/*.au /c/Program\ Files/ayttm/sounds
	mkdir -p /c/Program\ Files/ayttm/modules
	cp ../modules/aim-toc/.libs/*.{dll,la} /c/Program\ Files/ayttm/modules
	cp ../modules/icq-toc/.libs/*.{dll,la} /c/Program\ Files/ayttm/modules
	cp ../modules/importers/.libs/*.{dll,la} /c/Program\ Files/ayttm/modules
	cp ../modules/irc/.libs/*.{dll,la} /c/Program\ Files/ayttm/modules
	cp ../modules/jabber/.libs/*.{dll,la} /c/Program\ Files/ayttm/modules
	cp ../modules/smileys/.libs/*.{dll,la} /c/Program\ Files/ayttm/modules
	mkdir -p /c/Program\ Files/ayttm/smileys
	mkdir -p /c/Program\ Files/ayttm/smileys/Console\ Smileys
	mkdir -p /c/Program\ Files/ayttm/smileys/Giles\ Smiles
	cp ../modules/smileys/console_smileys/*.{xpm,rc} /c/Program\ Files/ayttm/smileys/Console\ Smileys
	cp ../modules/smileys/giles_smiles/*.{xpm,rc} /c/Program\ Files/ayttm/smileys/Giles\ Smiles
	cp ../modules/utility/.libs/*.{dll,la} /c/Program\ Files/ayttm/modules
	cp ../modules/yahoo2/.libs/*.{dll,la} /c/Program\ Files/ayttm/modules
	cp ../modules/msn2/.libs/*.{dll,la} /c/Program\ Files/ayttm/modules
