ACLOCAL_AMFLAGS = -I m4


@PACKAGE@rc: Makefile
	@rm -f @PACKAGE@rc >/dev/null 2>&1;
	@echo version=\"@VERSION@-@RELEASE@\" > @PACKAGE@rc
	@echo default_arrive_snd=\\\\Program\\ Files\\\\Ayttm\\\\BuddyArrive.au >> @PACKAGE@rc
	@echo default_leave_snd=\\\\Program\\ Files\\\\Ayttm\\\\BuddyLeave.au >> @PACKAGE@rc
	@echo default_send_snd=\\\\Program\\ Files\\\\Ayttm\\\\Send.au >> @PACKAGE@rc
	@echo default_recv_snd=\\\\Program\\ Files\\\\Ayttm\\\\Receive.au >> @PACKAGE@rc
	@echo default_module_path=\\\\Program\\ Files\\\\Ayttm\\\\modules >> @PACKAGE@rc

