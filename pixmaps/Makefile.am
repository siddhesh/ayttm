PIXMAP_DIRS = msn

# we won't use SUBDIRS because there's no point putting a makefile in
# each pixmap directory just for EXTRA_DIST
# instead we'll use a dist-hook target and hope it's supported by all
# versions of automake

EXTRA_DIST = penguin_talk.xpm ayttm.xpm ayttmlogo.xpm ayttm_32x32.xpm \
        ok.xpm tb_yes.xpm tb_no.xpm cancel.xpm tb_volume.xpm tb_edit.xpm \
	tb_open.xpm tb_book_red.xpm tb_preferences.xpm \
	tb_trash.xpm tb_book_open.xpm ignore.xpm blank_icon.xpm \
	login_icon.xpm logoff_icon.xpm yahoo_away.xpm yahoo_online.xpm \
	icq_online.xpm icq_away.xpm irc_away.xpm irc_online.xpm \
	aim_online.xpm aim_away.xpm yahoo_sms.xpm \
	msn_online.xpm msn_away.xpm tb_mail_send.xpm tb_search.xpm \
	ayttm.png jabber_online.xpm jabber_away.xpm \
	nomodule_away.xpm note.xpm dude.xpm \
	aol_icon.xpm free_icon.xpm dt_icon.xpm admin_icon.xpm \
	biglaugh.xpm blankface.xpm blush.xpm confused.xpm cooldude.xpm \
	grin.xpm heyyy.xpm laugh.xpm lovey.xpm oh.xpm sad.xpm smile.xpm \
	tongue.xpm wink.xpm worried.xpm angry.xpm beer.xpm cry.xpm log.xpm \
	no_such_smiley.xpm nomodule_online.xpm wine.xpm smiley_button.xpm \
	workwizu_online.xpm workwizu_away.xpm \
	warning.xpm question.xpm action.xpm print.xpm \
	checkbox_on.xpm checkbox_off.xpm info.xpm error.xpm \
	invite_btn.xpm ayttm.ico reconnect.xpm help.xpm \
	away.png buddy.png group.png group-chat.png smileys.png


Utilitiesdir = $(datadir)/pixmaps
Utilities_DATA = ayttm.png away.png buddy.png group.png group-chat.png smileys.png
# Gets rid of the datarootdir warning
datarootdir=@datarootdir@

dist-hook:
	for pixmap_dir in $(PIXMAP_DIRS); do \
		mkdir $(distdir)/$$pixmap_dir; \
		cp -p $$pixmap_dir/*.xpm $(distdir)/$$pixmap_dir; \
	done
