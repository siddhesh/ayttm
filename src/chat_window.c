/*
 * Ayttm
 *
 * Copyright (C) 2003, the Ayttm team
 * 
 * Ayttm is derivative of Everybuddy
 * Copyright (C) 1999-2002, Torrey Searle <tsearle@uci.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*
 * chat_window.c
 * implementation for the conversation window
 * This is the window where you will be doing most of your talking :)
 *
 */

#include "intl.h"

#include <string.h>
#include <stdlib.h>
#include <gdk/gdkkeysyms.h>
#include <ctype.h>

#ifdef HAVE_ICONV_H
#include <iconv.h>
#include <errno.h>
#endif

#include "util.h"
#include "add_contact_window.h"
#include "sound.h"
#include "dialog.h"
#include "prefs.h"
#include "gtk_globals.h"
#include "status.h"
#include "away_window.h"
#include "message_parse.h"
#include "plugin.h"
#include "contact_actions.h"
#include "smileys.h"
#include "service.h"
#include "action.h"
#include "mem_util.h"
#include "chat_room.h"

#include "gtk/gtk_eb_html.h"
#include "gtk/gtkutils.h"

#ifdef HAVE_LIBPSPELL
#include "gtk/gtkspell.h"
#endif

#include "pixmaps/tb_book_red.xpm"
#include "pixmaps/tb_open.xpm"
#include "pixmaps/tb_volume.xpm"
#include "pixmaps/tb_edit.xpm"
#include "pixmaps/tb_search.xpm"
#include "pixmaps/tb_no.xpm"
#include "pixmaps/tb_mail_send.xpm"
#include "pixmaps/cancel.xpm"
#include "pixmaps/smiley_button.xpm"
#include "pixmaps/action.xpm"
#include "pixmaps/invite_btn.xpm"

#define BUF_SIZE 1024  /* Maximum message length */
#ifndef NAME_MAX
#define NAME_MAX 4096
#endif


#define GET_CHAT_WINDOW(cur_cw) {\
	if( iGetLocalPref("do_tabbed_chat") ) { \
		chat_window *bck = cur_cw; \
		if (cur_cw->notebook) \
			cur_cw = find_tabbed_chat_window_index(gtk_notebook_get_current_page(GTK_NOTEBOOK(cur_cw->notebook))); \
		if (cur_cw == NULL) \
			cur_cw = bck; \
	} \
}

/* forward declaration */
static void eb_update_window_title(chat_window * cw, gboolean new_message);
static void eb_update_window_title_to_tab(int tab, gboolean new_message);
static void handle_focus(GtkWidget *widget, GdkEventFocus * event, gpointer userdata);

LList *outgoing_message_filters=NULL;
LList *incoming_message_filters=NULL;


#define ENTRY_FOCUS(x) { chat_window *x2 = x; \
			 GET_CHAT_WINDOW(x2); \
			 gtk_widget_grab_focus(x2->entry); \
}
#define ENTRY_FORCE_FOCUS(x) { gtk_widget_grab_focus(x->entry); }

#ifdef HAVE_ICONV_H

/*
    Recodes source text with iconv() and puts it in translated_text
    returns pointer to recoded text
*/
enum
{
	RECODE_TO_LOCAL = 0,
	RECODE_TO_REMOTE
};

static char	*recode_if_needed( const char *source_text, int direction)
{
	size_t inleft;
	size_t outleft;
	const char		*ptr_src_text = source_text;
	char * const	recoded_text = g_new0(char, strlen(source_text)*2 + 1);
	char 			*ptr_recoded_text = recoded_text;
	iconv_t conv_desc;
	int tries;

	if( iGetLocalPref( "use_recoding" ) == 1 ) {
		if( direction == RECODE_TO_REMOTE )
			conv_desc = iconv_open( cGetLocalPref("remote_encoding"), cGetLocalPref("local_encoding") );
		else
			conv_desc = iconv_open( cGetLocalPref("local_encoding"), cGetLocalPref("remote_encoding") );
	
		if( conv_desc != (iconv_t)(-1) ) {
			ptr_src_text = source_text;
			inleft = strlen(source_text) + 1;
			/* 'inleft*2' e.g. for 'Latin-x' to 'UTF-8', 
			which is 1-byte to 2-byte recoding */
			outleft = inleft * 2 + 1;

			for(tries = 0; tries < 4; tries++ ) {
				if( iconv(conv_desc, (char **)&ptr_src_text, &inleft, &ptr_recoded_text, &outleft) == (size_t)(-1) ){
					if( inleft && errno == EILSEQ ) {
						if( tries == 3 ) {
							strncpy( ptr_recoded_text, ptr_src_text,
								strlen(source_text)*2 );
							fprintf( stderr, "Ayttm: recoding broke 3 times,"
							   " leaving the rest of the line as it is...\n");
							break;
						} else {
							/* trying to skip offending character
							this may break input stream if it's multibyte
							*/
							*ptr_recoded_text = '.'; /*  *ptr_src_text;  */
							ptr_recoded_text++;
							ptr_src_text++;
							inleft--;
							outleft--;
							fprintf( stderr, "Ayttm: charachter cannot be recoded, "
								"trying to skip it...\n");
							continue;
			  			}
					} else if (errno == EINVAL ) {
						fprintf( stderr, "Ayttm: recoding broke - "
							"incomplete multibyte sequence at the end of the line.\n");
						break;
					} else if (errno == E2BIG ) {
						fprintf( stderr, "Ayttm: recoding buffer too small?!! Oops! :(\n");
						break;
					} else {
						fprintf( stderr, "Ayttm: unknown recoding error.\n");
						break;
		    			}
				}
			}
			*(ptr_recoded_text + strlen(ptr_src_text)) = '\0'; /*  just in case :) */

			iconv_close(conv_desc);
			return recoded_text;
		} else {
			fprintf( stderr, "Ayttm: recoding from %s to %s is not valid, sorry!\n"
			   "Turning recoding off.\n",
			   cGetLocalPref("local_encoding"), cGetLocalPref("remote_encoding"));
			iSetLocalPref( "use_recoding", 0 );
			
			if ( recoded_text != NULL )
				g_free( recoded_text );
				
			return g_strdup(source_text);
		}
	}
			
	if ( recoded_text != NULL )
		g_free( recoded_text );
		
	return g_strdup(source_text);
}

#endif	/* HAVE_ICONV_H */

#ifdef __MINGW32__
static void redraw_chat_window(GtkWidget *text)
{
	GdkRectangle update_rect;
	update_rect.x = 0;
	update_rect.y = 0;
	update_rect.width = text->allocation.width;
	update_rect.height = text->allocation.height;
	gtk_widget_draw(text,&update_rect);
}
#endif

void set_tab_red(chat_window *cw)
{
	GtkStyle *style;
	GdkColor color;
	GtkWidget *notebook = NULL;
	GtkWidget *child = NULL;
	GtkWidget *label = NULL;

	if (!cw)
		return;
	notebook = cw->notebook;
	child = cw->notebook_child;
	
	if (!notebook || !child)
		return;
	
	label = gtk_notebook_get_tab_label(GTK_NOTEBOOK(notebook), child);

	gtk_widget_realize(label);

	color.red = 65000;
	color.green = 0;
	color.blue = 0;


	style = gtk_style_new();
	gdk_font_unref(style->font);
	style->font = gdk_font_ref(label->style->font);
	style->fg[0] = color;

	gtk_widget_set_style(label, style);
}

void set_tab_normal(chat_window *cw)
{
	GtkStyle *style;
	GdkColor color;

	GtkWidget *notebook = NULL;
	GtkWidget *child = NULL;
	GtkWidget *label = NULL;

	if (!cw)
		return;
	notebook = cw->notebook;
	child = cw->notebook_child;
	
	if (!notebook || !child)
		return;
	
	label = gtk_notebook_get_tab_label(GTK_NOTEBOOK(notebook), child);
	gtk_widget_realize(label);

	color.red = 0;
	color.green = 0;
	color.blue = 0;

	style = gtk_style_new();
	gdk_font_unref(style->font);
	style->font = gdk_font_ref(label->style->font);
	style->fg[0] = GTK_WIDGET(notebook)->style->fg[0];

	gtk_widget_set_style(label, style);
	gtk_style_unref(style);
}


static void end_conversation(chat_window* cw)
{
	LList * node;
	LList * node2;

	/* cw->window=NULL; */

	/* will this fix the weird tabbed chat segfault? */
	if (cw->contact != NULL)
		cw->contact->chatwindow = NULL;
	
	/*
	* Some protocols like MSN and jabber require that something
	* needs to be done when you stop a conversation
	* for every protocol that requires it we call their terminate
	* method
	*/

	for(node = cw->contact->accounts; node; node = node->next) {
		eb_account *ea = (eb_account*)(node->data);

		if(eb_services[ea->service_id].sc->terminate_chat) 
			RUN_SERVICE(ea)->terminate_chat(ea);
	}

	for(node2=cw->history; node2!=NULL; node2=node2->next) {
		free(node2->data);
		node2->data=NULL;
	}

	l_list_free(cw->history);

	/*
	* if we are logging conversations, time stamp when the conversation
	* has ended
	*/

	ay_log_file_close( cw->logfile );
	
	/*
	* and free the memory we allocated to the chat window
	* NOTE: a memset is done to flush out bugs related to the
	* tabbed chat window
	*/

	memset(cw, 0, sizeof(chat_window));
	g_free(cw);
}

static void remove_smiley_window(struct contact *ct)
{
	chat_window *cw = ct->chatwindow;

	GET_CHAT_WINDOW(cw);
	if(cw->smiley_window != NULL) {
		/* close smileys popup */
		gtk_widget_destroy(cw->smiley_window);
		cw->smiley_window = NULL;
	}
	
}

/*
 * They guy closed the chat window, so we need to clean up
 */

static void destroy_event(GtkWidget *widget, gpointer userdata)
{
	chat_window* cw = (chat_window*)userdata;
	/* gotta clean up all of the people we're talking with */
	gtk_signal_disconnect_by_func(GTK_OBJECT(cw->window),
			   GTK_SIGNAL_FUNC(handle_focus), cw);
	remove_smiley_window(cw->contact);

	end_conversation(cw);
}

static void remove_tab(struct contact *ct)
{
	GtkWidget *window = ct->chatwindow->window;
	GtkWidget *notebook = ct->chatwindow->notebook;
	int tab_number;
	
	tab_number = gtk_notebook_page_num (GTK_NOTEBOOK(notebook), ct->chatwindow->notebook_child);
	gtk_notebook_remove_page(GTK_NOTEBOOK(notebook), tab_number);

	if (gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), 0) == NULL) {
		gtk_widget_destroy(window);
	}
}

static void close_tab_callback(GtkWidget *button, gpointer userdata)
{
	chat_window *cw = (chat_window*)userdata;

	remove_smiley_window(cw->contact);
	remove_tab (cw->contact);
}

static void add_unknown_callback(GtkWidget * add_button, gpointer userdata)
{
	chat_window * cw = userdata;

	/* if something wierd is going on and the unknown contact has multiple
	* accounts, find use the preferred account
	*/

	cw->preferred = find_suitable_remote_account(cw->preferred, cw->contact);

	/* if in the weird case that the unknown user has gone offline
	* just use the first account you see
	*/

	if(!cw->preferred)
		cw->preferred = cw->contact->accounts->data;

	/* if that fails, something is seriously wrong
	* bail out while you can
	*/

	if(!cw->preferred)
		return;

	/* now that we have a valid account, pop up the window already :) */

	add_unknown_account_window_new(cw->preferred);
}

static char *cw_get_message(chat_window *cw)
{
	return gtk_editable_get_chars(GTK_EDITABLE (cw->entry), 0, -1);	
}

static int cw_set_message(chat_window *cw, char *msg)
{
	int p=0;
	gtk_editable_insert_text(GTK_EDITABLE (cw->entry), msg, strlen(msg), &p);
	return p;
	
}

static void cw_reset_message(chat_window *cw)
{
	gtk_editable_delete_text(GTK_EDITABLE (cw->entry), 0, -1);	
}

static void cw_put_message(chat_window *cw, char *text, int fore, int back, int font)
{
	gtk_eb_html_add(EXT_GTK_TEXT(cw->chat), text, fore, back, font);
}

void send_message(GtkWidget *widget, gpointer d)
{
	chat_window * data = (chat_window*)d;
	gchar buff[BUF_SIZE];
	gchar buff2[BUF_SIZE];
	gchar * text, *o_text = NULL;
	gchar * link_message;
	gchar * temp_message;
	struct tm * cur_time;
	time_t t;
	LList * filter_walk;
#ifdef __MINGW32__
	char *recoded;
#endif

	GET_CHAT_WINDOW(data);

	/*determine what is the best account to send to*/
	data->preferred= find_suitable_remote_account(data->preferred, data->contact);

	if(!data->preferred) {
		/*Eventually this will need to become a dialog box that pops up*/

		if(data->contact->send_offline && can_offline_message(data->contact)) {
			data->preferred = can_offline_message(data->contact);
		} else {
			cw_put_message(data, _("<body bgcolor=#F9E589 width=*><b> Cannot send message - user is offline.</b></body>\n"),0,0,0);
			return;
		}
	} else if (!data->preferred->online && !data->contact->send_offline) {
		cw_put_message(data, _("<body bgcolor=#F9E589 width=*><b> Cannot send message - user is offline.</b></body>\n"),0,0,0);
		return;		
	}

	
	if(data->local_user && data->local_user != data->preferred->ela)
		data->local_user = NULL;
	
	if(data->local_user && !data->local_user->connected)
		data->local_user = NULL;
	
	/*determine what is the best local account to use*/

	if(!data->local_user)
		data->local_user = find_suitable_local_account_for_remote(data->preferred, data->preferred->ela); 

	if(!data->local_user) {
		cw_put_message(data, _("<body bgcolor=#F9E589 width=*><b> Cannot send message - no local account found.</b></body>\n"),0,0,0);
		return;		
	}
	
	eb_update_window_title(data, FALSE);

	text = cw_get_message(data);

	if(strlen(text) == 0)
		return;

	if(data->this_msg_in_history) {
		LList * node=NULL;
		LList * node2=NULL;

		for(node=data->history; node!=NULL ; node=node->next)
			node2=node;
		free(node2->data);
		node2->data=strdup(text);
		data->this_msg_in_history=0;
	} else {
		data->history=l_list_append(data->history, strdup(text));
		data->hist_pos=NULL;
	}

	/* TODO make these two filters */
	if(RUN_SERVICE(data->local_user)->get_smileys)
		temp_message = eb_smilify(text, RUN_SERVICE(data->local_user)->get_smileys());
	else
		temp_message = g_strdup(text);
	link_message = linkify(temp_message);
	g_free(temp_message);
	eb_update_window_title(data, FALSE);

	eb_debug(DBG_CORE, "Starting to run outgoing filters\n");

	for(filter_walk = outgoing_message_filters; filter_walk; filter_walk=filter_walk->next) {
		char * (*ifilter)(const eb_local_account *, const eb_account *, const struct contact *, const char *);

		eb_debug(DBG_CORE, "Running an outgoing filter:\n");

		ifilter=filter_walk->data;

		o_text=ifilter(data->local_user, data->preferred, data->contact, text);
		free(text);
		text=o_text;

		if(text==NULL) 
			return;
	}

	eb_debug(DBG_CORE, "Finished outgoing filters\n");

	/*  end outbound filters */

	/* TODO: make this also a filter */
	o_text = convert_eol(text);
	g_free(text);
	text=o_text;

#ifdef __MINGW32__
	recoded = ay_utf8_to_str(text);
	g_free(text);
	text = recoded;
#endif

#ifdef HAVE_ICONV_H
	if(!can_iconvert(eb_services[data->preferred->service_id])) {
		RUN_SERVICE(data->local_user)->send_im(
							data->local_user,
							data->preferred,
							text);
	} else {
		char *recoded = recode_if_needed(text, RECODE_TO_REMOTE);
		RUN_SERVICE(data->local_user)->send_im(
							data->local_user,
							data->preferred,
					 		recoded);
		g_free(recoded);
	}
	/* seems like variable 'text' is not used any more down
	the function, so we don't have to assign it (BTW it's freed in the end)*/
#else
	RUN_SERVICE(data->local_user)->send_im(
					 data->local_user,
					 data->preferred,
					 text);
#endif
	serv_touch_idle();

	if(data->sound_enabled && data->send_enabled)
		play_sound( SOUND_SEND );

	if (iGetLocalPref("do_convo_timestamp")) {
		time(&t);
		cur_time = localtime(&t);
		g_snprintf(buff2, BUF_SIZE, "%d:%.2d:%.2d %s", cur_time->tm_hour,
			 cur_time->tm_min, cur_time->tm_sec,
			 data->local_user->alias);
	} else
		g_snprintf(buff2, BUF_SIZE, "%s", data->local_user->alias);
	
	g_snprintf(buff, BUF_SIZE, "<FONT COLOR=\"#0000ff\"><B>%s: </B></FONT>", buff2);

	cw_put_message(data, buff, 1, 0, 0);
	cw_put_message(data, link_message, iGetLocalPref("do_ignore_back"), iGetLocalPref("do_ignore_fore"), iGetLocalPref("do_ignore_font"));
	cw_put_message(data, "<br>", 0, 0, 0);

	/* If an away message had been sent to this person, reset the away message tracker */
	/* It's probably faster to just do the assignment all the time--the test
	is there for code clarity. */

	if (data->away_msg_sent)
		data->away_msg_sent = (time_t)NULL;
	
	/* Log the message */

	if ( iGetLocalPref("do_logging") ) 
		ay_log_file_message( data->logfile, buff, link_message );

	cw_reset_message(data);
	g_free(link_message);
	g_free(text);
#ifdef __MINGW32__
	redraw_chat_window(data->chat);
#endif


	/* if using tabs, then turn off the chat icon */
	if (data->notebook != NULL) {
		/* no more icons in the tabs */
		/* gtk_widget_hide(data->talk_pixmap); */
		/* printf("chat icon is off... \n"); */
		set_tab_normal(data->contact->chatwindow);
	}
}

/*These are the action handlers for the buttons*/

/*** MIZHI
 * callback function for viewing the log
 */
static void view_log_callback(GtkWidget *widget, gpointer d)
{
	chat_window* data = (chat_window*)d;
	GET_CHAT_WINDOW(data);
	eb_view_log(data->contact);
}

static void action_callback(GtkWidget *widget, gpointer d)
{
	chat_window* data = (chat_window*)d;
	GET_CHAT_WINDOW(data);
	conversation_action( data->logfile, TRUE );
}

/*This is the callback for ignoring a user*/

static void ignore_dialog_callback (gpointer userdata, int response)
{
	struct contact *c = (struct contact *)userdata;

	if (response)  {
		move_contact(_("Ignore"), c);
		update_contact_list ();
		write_contact_list();
	}

}

static void ignore_callback (GtkWidget *ignore_button, gpointer userdata)
{
	chat_window * cw = (chat_window *)userdata;
	gchar *buff = (gchar *)g_new0(gchar *, BUF_SIZE);
	GET_CHAT_WINDOW(cw);

	g_snprintf(buff, BUF_SIZE, _("Do you really want to ignore %s?"), cw->contact->nick);

	eb_do_dialog(buff, _("Ignore Contact"), ignore_dialog_callback, cw->contact);	
	g_free(buff);
}

/*This is the callback for file x-fer*/
static void send_file (GtkWidget * sendf_button, gpointer userdata)
{
	eb_account *ea;
	chat_window *data  = (chat_window*)userdata;
	GET_CHAT_WINDOW(data);

	if ( data->contact->online == 0 ) {
		cw_put_message(data, _("<body bgcolor=#F9E589 width=*><b> Cannot send message - user is offline.</b></body>\n"),0,0,0);
		return;
	}

	ea = find_suitable_remote_account(data->preferred, data->contact);
	if (ea)
		eb_do_send_file(ea);
}

/*These are the callback for setting the sound*/
static void set_sound_callback(GtkWidget * sound_button, gpointer userdata);
static void cw_set_sound_active(chat_window *cw, int active)
{
	cw->sound_enabled = active;
	gtk_signal_handler_block_by_func(GTK_OBJECT(cw->sound_button),
                                         set_sound_callback, cw);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(cw->sound_button), active);
	gtk_signal_handler_unblock_by_func(GTK_OBJECT(cw->sound_button),
                                         set_sound_callback, cw);
}

static int cw_get_sound_active(chat_window *cw)
{
	return cw->sound_enabled;
}

static void set_sound_callback(GtkWidget * sound_button, gpointer userdata)
{
	chat_window * cw = (chat_window *)userdata;
	GET_CHAT_WINDOW(cw);
	cw_set_sound_active(cw, !cw_get_sound_active(cw));
}

/*This is the callback for closing the window*/

static void close_win (GtkWidget * close_button, gpointer userdata)
{
	chat_window * cw = (chat_window *)userdata;
	
	gtk_widget_destroy(cw->window);
}

static void allow_offline_callback(GtkWidget * offline_button, gpointer userdata);
static void cw_set_offline_active(chat_window *cw, int active)
{
	cw->contact->send_offline = active;
	gtk_signal_handler_block_by_func(GTK_OBJECT(cw->offline_button),
                                         allow_offline_callback, cw);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(cw->offline_button), active);
	gtk_signal_handler_unblock_by_func(GTK_OBJECT(cw->offline_button),
                                         allow_offline_callback, cw);
}

static int cw_get_offline_active(chat_window *cw)
{
	return cw->contact->send_offline;
}

static void allow_offline_callback(GtkWidget * offline_button, gpointer userdata)
{
	chat_window * cw = (chat_window *)userdata;
	GET_CHAT_WINDOW(cw);
	cw_set_offline_active(cw, !cw_get_offline_active(cw));
}

static void change_local_account_on_click (GtkWidget * button, gpointer userdata)
{
	GtkLabel *label= GTK_LABEL (GTK_BIN (button)->child);
	chat_window_account *cwa = (chat_window_account *)userdata;
	chat_window * cw;
	gchar *account;
	eb_local_account *ela=NULL;

	/* Should never happen */
	if(!cwa)
		return;
	cw=cwa->cw;
	ela=(eb_local_account *)cwa->data;
	cw->local_user=ela;
	/* don't free it */
	gtk_label_get(label, &account);
	eb_debug(DBG_CORE, "change_local_account_on_click: %s\n", account);
}

static GtkWidget *get_local_accounts(chat_window *cw)
{
	GtkWidget *submenu, *label, *button;
	char *handle=NULL, buff[256];
	eb_local_account *first_act=NULL, *subsequent_act=NULL;
	chat_window_account *cwa=NULL;

	/* Do we have a preferred remote account, no, get one */ 
	if(!cw->preferred) {
		cw->preferred = find_suitable_remote_account(NULL, cw->contact);
		if(!cw->preferred) /* The remote user is not online */
			return(NULL);
	}
	handle=cw->preferred->handle;
	eb_debug(DBG_CORE, "Setting menu item with label: %s\n", handle);
	/* Check to see if we have at least 2 local accounts suitable for the remote account */
	first_act = find_local_account_for_remote(cw->preferred, TRUE);
	subsequent_act = find_local_account_for_remote(NULL, TRUE);
	if(!first_act || !subsequent_act || first_act == subsequent_act)
		return(NULL);

	first_act = find_local_account_for_remote(cw->preferred, TRUE);

	/* Start building the menu */
	label = gtk_menu_item_new_with_label(_("Change Local Account"));
	submenu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(label), submenu);

	subsequent_act = first_act;
	do {
		snprintf(buff, sizeof(buff), "%s:%s", 
			get_service_name(subsequent_act->service_id), subsequent_act->handle);
		button = gtk_menu_item_new_with_label(buff);
		gtk_menu_append(GTK_MENU(submenu), button);
		cwa = g_new0(chat_window_account, 1);
		cwa->cw=cw;
		cwa->data=subsequent_act;
		gtk_signal_connect(GTK_OBJECT(button), "activate",
		GTK_SIGNAL_FUNC(change_local_account_on_click), cwa);
		gtk_widget_show(button);
	} while( (subsequent_act = find_local_account_for_remote(NULL, TRUE)));
	
	gtk_widget_show(label);
	gtk_widget_show(submenu);

	return(label);
}

static void handle_focus(GtkWidget *widget, GdkEventFocus * event, 
			 gpointer userdata)
{
	chat_window * cw = (chat_window *)userdata;
	eb_update_window_title(cw, FALSE);
	if(cw->entry)
		ENTRY_FOCUS(cw);
}

/*This handles the right mouse button clicks*/

static void handle_click(GtkWidget *widget, GdkEventButton * event, 
			 gpointer userdata)
{
	chat_window * cw = (chat_window*)userdata;

	if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
		GtkWidget *menu;
		GtkWidget *button;
		menu_data *md=NULL;

		
		gtk_signal_emit_stop_by_name(GTK_OBJECT(widget),
					       "button_press_event");
		menu = gtk_menu_new();

		/*Add Contact Selection*/
		if(!strcmp(cw->contact->group->name, _("Unknown"))
		|| !strncmp(cw->contact->group->name, "__Ayttm_Dummy_Group__", strlen("__Ayttm_Dummy_Group__"))) {
			button = gtk_menu_item_new_with_label(_("Add Contact"));
			gtk_signal_connect(GTK_OBJECT(button), "activate",
				     GTK_SIGNAL_FUNC(add_unknown_callback), cw);
			gtk_menu_append(GTK_MENU(menu), button);
			gtk_widget_show(button);
		}

		/*Allow Offline Messaging Selection*/

		if(can_offline_message(cw->contact)) {	
			button = gtk_menu_item_new_with_label(_("Offline Messaging"));
			gtk_signal_connect(GTK_OBJECT(button), "activate",
				     GTK_SIGNAL_FUNC(allow_offline_callback), cw);
			gtk_menu_append(GTK_MENU(menu), button);
			gtk_widget_show(button);
		}

		/*Allow account selection when there are multiple accounts for the same protocl */
		button = get_local_accounts(cw);
		if(button)
			gtk_menu_append(GTK_MENU(menu), button);

		/*Sound Selection*/

		if (cw->sound_enabled)
			button = gtk_menu_item_new_with_label(_("Disable Sounds"));
		else
			button = gtk_menu_item_new_with_label(_("Enable Sounds"));

		gtk_signal_connect(GTK_OBJECT(button), "activate",
				 GTK_SIGNAL_FUNC(set_sound_callback), cw);
		gtk_menu_append(GTK_MENU(menu), button);
		gtk_widget_show(button);

		/*View log selection*/

		button = gtk_menu_item_new_with_label(_("View Log"));
		gtk_signal_connect(GTK_OBJECT(button), "activate",
				 GTK_SIGNAL_FUNC(view_log_callback), cw);
		gtk_menu_append(GTK_MENU(menu), button);
		gtk_widget_show(button);

#ifndef __MINGW32__
		button = gtk_menu_item_new_with_label(_("Actions..."));
		gtk_signal_connect(GTK_OBJECT(button), "activate",
				 GTK_SIGNAL_FUNC(action_callback), cw);
		gtk_menu_append(GTK_MENU(menu), button);
		gtk_widget_show(button);
#endif

		gtkut_create_menu_button (GTK_MENU(menu), NULL, NULL, NULL);

		/*Send File Selection*/

		button = gtk_menu_item_new_with_label(_("Send File"));
		gtk_signal_connect(GTK_OBJECT(button), "activate",
				 GTK_SIGNAL_FUNC(send_file), cw);
		gtk_menu_append(GTK_MENU(menu), button);
		gtk_widget_show(button);

		/*Invite Selection*/

		button = gtk_menu_item_new_with_label(_("Invite"));
		gtk_signal_connect(GTK_OBJECT(button), "activate",
				 GTK_SIGNAL_FUNC(do_invite_window), cw);
		gtk_menu_append(GTK_MENU(menu), button);
		gtk_widget_show(button);

		/*Ignore Section*/

		button = gtk_menu_item_new_with_label(_("Ignore Contact"));
		gtk_signal_connect(GTK_OBJECT(button), "activate",
				 GTK_SIGNAL_FUNC(ignore_callback), cw);
		gtk_menu_append(GTK_MENU(menu), button);
		gtk_widget_show(button);

		/*Send message Section*/

		button = gtk_menu_item_new_with_label(_("Send Message"));
		gtk_signal_connect(GTK_OBJECT(button), "activate",
				 GTK_SIGNAL_FUNC(send_message), cw);
		gtk_menu_append(GTK_MENU(menu), button);
		gtk_widget_show(button);	

		/*Add Plugin Menus*/
		md = GetPref(EB_CHAT_WINDOW_MENU);
		if(md) {
			int should_sep=0;
			gtkut_create_menu_button (GTK_MENU(menu), NULL, NULL, NULL);
			should_sep=(add_menu_items(menu,-1, should_sep, NULL, cw->preferred, cw->local_user)>0);
			if (cw->local_user)
				add_menu_items(menu,cw->local_user->service_id, should_sep, NULL, 
						cw->preferred, cw->local_user);
			gtkut_create_menu_button (GTK_MENU(menu), NULL, NULL, NULL);
		}

		/*Close Selection*/

		button = gtk_menu_item_new_with_label(_("Close"));

		if ( iGetLocalPref("do_tabbed_chat") )
			gtk_signal_connect(GTK_OBJECT(button), "activate",
					 GTK_SIGNAL_FUNC(close_tab_callback), cw);
		else
			gtk_signal_connect(GTK_OBJECT(button), "activate",
					 GTK_SIGNAL_FUNC(close_win), cw);

		gtk_menu_append(GTK_MENU(menu), button);
		gtk_widget_show(button);

		gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
			event->button, event->time );
	}
}

static void send_typing_status(chat_window *cw)
{
	/* typing send code */
	time_t now=time(NULL);

	if(!iGetLocalPref("do_send_typing_notify"))
		return;

	if(now>=cw->next_typing_send) {
		if(!cw->preferred) {
			if(!cw->contact)
				return;
			cw->preferred = find_suitable_remote_account(NULL, cw->contact);
			if(!cw->preferred) /* The remote user is not online */
				return;
		}
		cw->local_user=find_suitable_local_account_for_remote(cw->preferred, cw->local_user);
		if(cw->local_user==NULL) 
			return;

		if(RUN_SERVICE(cw->local_user)->send_typing!=NULL)
			cw->next_typing_send=now + RUN_SERVICE(cw->local_user)->send_typing(cw->local_user, cw->preferred);
	}
}

static gboolean	check_tab_accelerators( const GtkWidget *inWidget, const chat_window *inCW, GdkModifierType inModifiers, const GdkEventKey *inEvent )
{
	if ( inCW->notebook != NULL )  /* only change tabs if this window is tabbed */
	{
		GdkDeviceKey 	accel_prev_tab;
		GdkDeviceKey 	accel_next_tab;
		
		
		gtk_accelerator_parse(cGetLocalPref("accel_next_tab"), &(accel_next_tab.keyval), &(accel_next_tab.modifiers));
		gtk_accelerator_parse(cGetLocalPref("accel_prev_tab"), &(accel_prev_tab.keyval), &(accel_prev_tab.modifiers));
	
		/*
		* this does not allow for using the same keyval for both prev and next
		* when next contains every modifier that previous has (with some more)
		* but i really don't think that will be a huge problem =)
		*/
		if ((inModifiers == accel_prev_tab.modifiers) && (inEvent->keyval == accel_prev_tab.keyval))
		{
			gtk_signal_emit_stop_by_name(GTK_OBJECT(inWidget), "key_press_event");
			gtk_notebook_prev_page( GTK_NOTEBOOK(inCW->notebook) );
			return( gtk_true() );
		}
		else if ((inModifiers == accel_next_tab.modifiers) && (inEvent->keyval == accel_next_tab.keyval))
		{
			gtk_signal_emit_stop_by_name(GTK_OBJECT(inWidget), "key_press_event");
			gtk_notebook_next_page( GTK_NOTEBOOK(inCW->notebook) );
			return( gtk_true() );
		}
	}
	
	return( gtk_false() );
}

static gboolean chat_singleline_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	chat_window				*cw = (chat_window *)data;
	const GdkModifierType	modifiers = event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_MOD4_MASK);
	
	if( !modifiers )
		send_typing_status(cw);
	else
		check_tab_accelerators( widget, cw, modifiers, event );
	
	return gtk_true();
}

static void chat_away_set_back(GtkWidget *widget, gpointer data)
{
	if (gtk_object_get_user_data (GTK_OBJECT(widget)) != 0) {
		chat_window *cw = (chat_window *)data;
		cw->away_warn_displayed = (time_t)NULL;
		away_window_set_back();
	}
}

static void chat_warn_if_away(chat_window *cw)
{
	if (is_away && (time(NULL) - cw->away_warn_displayed) > (60*30)) {
		cw->away_warn_displayed = time(NULL);
		do_dialog(_("You are currently away. \n\nDo you want to be back Online?"), 
			  _("Away"), chat_away_set_back, cw);
	}
	
}

static gboolean	chat_key_press( GtkWidget *widget, GdkEventKey *event, gpointer data )
{
	chat_window				*cw = (chat_window *)data;
	const GdkModifierType	modifiers = event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_MOD4_MASK);

	
	eb_update_window_title( cw, FALSE );

	if ( event->keyval == GDK_Return )
	{
		/* Just print a newline on Shift-Return */
		if ( event->state & GDK_SHIFT_MASK )
		{
			event->state = 0;
		}
		else if ( iGetLocalPref("do_enter_send") )
		{
			/*Prevents a newline from being printed*/
			gtk_signal_emit_stop_by_name( GTK_OBJECT(widget), "key_press_event" );

			chat_warn_if_away(cw);
			send_message( NULL, cw );
			
			return( gtk_true() );
		}
	}
	else if ( (event->keyval == GDK_Up) && (modifiers == 0) )
	{
		if ( cw->history == NULL ) 
			return( gtk_true() );

		if ( cw->hist_pos == NULL )
		{
			LList	*node = NULL;
			char	*s = cw_get_message(cw);

			for ( node = cw->history; node != NULL ; node = node->next )
				cw->hist_pos = node;

			if ( strlen( s ) > 0 )
			{
				cw->history=l_list_append( cw->history, strdup( s ) );
				g_free( s ); 
				cw->this_msg_in_history = 1;
			}
		}
		else
		{
			cw->hist_pos=cw->hist_pos->prev;
			
			if ( cw->hist_pos==NULL )
			{
				LList	*node = NULL;
				
				eb_debug(DBG_CORE,"history Wrapped!\n");
				for ( node = cw->history; node != NULL ; node = node->next )
					cw->hist_pos = node;
			}
		}

		cw_reset_message( cw );
		cw_set_message( cw, cw->hist_pos->data );
	}
	else if ( (event->keyval == GDK_Down) && (modifiers == 0) )
	{
		if ( cw->history == NULL || cw->hist_pos == NULL ) 
			return( gtk_true() );
		
		cw->hist_pos = cw->hist_pos->next;
		
		cw_reset_message( cw );
		
		if ( cw->hist_pos != NULL )
			cw_set_message( cw, cw->hist_pos->data );
	}
	else if (event->keyval == GDK_Page_Up)
	{
		GtkWidget *scwin = cw->chat->parent;
		GtkAdjustment *ga = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scwin));
		if (ga && ga->value > ga->page_size) {
			gtk_adjustment_set_value(ga, ga->value - ga->page_size);
			gtk_scrolled_window_set_vadjustment(GTK_SCROLLED_WINDOW(scwin), ga);
		} else if (ga) {
			gtk_adjustment_set_value(ga, 0);
			gtk_scrolled_window_set_vadjustment(GTK_SCROLLED_WINDOW(scwin), ga);
		}
	}
	else if (event->keyval == GDK_Page_Down)
	{
		GtkWidget *scwin = cw->chat->parent;
		GtkAdjustment *ga = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scwin));
		if (ga && ga->value < ga->upper - ga->page_size) {
			gtk_adjustment_set_value(ga, ga->value + ga->page_size);
			gtk_scrolled_window_set_vadjustment(GTK_SCROLLED_WINDOW(scwin), ga);
		} else if (ga) {
			gtk_adjustment_set_value(ga, ga->upper);
			gtk_scrolled_window_set_vadjustment(GTK_SCROLLED_WINDOW(scwin), ga);
		}		
	}
	else if (cw->notebook != NULL)
	{
		// check tab changes if this is a tabbed chat window
		if ( check_tab_accelerators( widget, cw, modifiers, event ) )
			return( gtk_true() );
	}

	if ( (cw->preferred != NULL) && (modifiers == 0) )
		send_typing_status(cw);

	return( gtk_false() );
}

static void chat_notebook_switch_callback(GtkNotebook *notebook, GtkNotebookPage *page, gint page_num, gpointer user_data)
{
	/* find the contact for the page we just switched to and turn off their talking penguin icon */
	LList * l1;
	LList * l2;
	struct contact* c;

	for(l1 = groups; l1; l1=l1->next ) {
		for(l2 = ((grouplist*)l1->data)->members; l2; l2=l2->next ) {
			/*  if this contact's chatwindow is equal to this one, */
			/*  then we need to end the conversation and remove the */
			/*  notebook tab... */
			c = (struct contact*)l2->data;
			if (c->chatwindow && c->chatwindow->notebook_child == page->child) {
				set_tab_normal(c->chatwindow);
				ENTRY_FORCE_FOCUS(c->chatwindow);
				eb_update_window_title_to_tab (page_num, FALSE);
				return;
			}
		}
	}
	eb_chat_room_notebook_switch(notebook, page, page_num);
}


static chat_window* find_tabbed_chat_window()
{
	LList * l1;
	LList * l2;
	struct contact* c;

	for(l1 = groups; l1; l1=l1->next ) {
		for(l2 = ((grouplist*)l1->data)->members; l2; l2=l2->next ) {
			c = (struct contact*)l2->data;
			if (c->chatwindow != NULL && c->chatwindow->notebook != NULL)
				return c->chatwindow;
		}
	}

	return find_tabbed_chat_room();
}

chat_window *find_tabbed_chat_window_index (int current_page)
{
	LList *l1;
	LList *l2;
	struct contact *c;

	chat_window *notebook_window = find_tabbed_chat_window ();
	if (notebook_window == NULL || notebook_window->notebook == NULL)
		return NULL;

	if (current_page == -1)
		current_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook_window->notebook));

	for (l1 = groups; l1; l1 = l1->next) {
		for (l2 = ((grouplist *) l1->data)->members; l2; l2 = l2->next) {
			c = (struct contact *) l2->data;
			if (c->chatwindow != NULL
			&& gtk_notebook_page_num(GTK_NOTEBOOK (notebook_window->notebook), 
					c->chatwindow->notebook_child) == current_page)
				return c->chatwindow;
		}
	}
	return NULL;
}

static chat_window *find_tabbed_current_chat_window ()
{
	return find_tabbed_chat_window_index(-1);
}

static void _show_smileys_cb(GtkWidget * widget, smiley_callback_data *data)
{
	show_smileys_cb(data);
}

void eb_chat_window_display_error(eb_account * remote, gchar * message)
{
	struct contact * remote_contact = remote->account_contact;

	if(remote_contact->chatwindow) {
		gtk_eb_html_add(EXT_GTK_TEXT(remote_contact->chatwindow->chat),
			      _("<b>Error: </b>"), 0,0,0);
		gtk_eb_html_add(EXT_GTK_TEXT(remote_contact->chatwindow->chat), message, 0,0,0);
		gtk_eb_html_add(EXT_GTK_TEXT(remote_contact->chatwindow->chat), "<br>", 0,0,0);
#ifdef __MINGW32__
		redraw_chat_window(remote_contact->chatwindow->chat);
#endif
	}
}

void eb_chat_window_do_timestamp(struct contact * c, gboolean online)
{
	gchar buff[BUF_SIZE];
	time_t my_time = time(NULL);
	if(!c || !c->chatwindow)
		return;

	if( !iGetLocalPref("do_timestamp") )
		return;
	
	g_snprintf(buff, BUF_SIZE,_("<body bgcolor=#F9E589 width=*><b> %s is logged %s @ %s.\n</b></body>"),
	     c->nick, (online?_("in"):_("out")), g_strchomp(asctime(localtime(&my_time))));
	gtk_eb_html_add(EXT_GTK_TEXT(c->chatwindow->chat), buff, 0,0,0);
}


void eb_chat_window_display_remote_message(eb_local_account * account,
					   eb_account * remote,
					   gchar * o_message)
{
	struct contact * remote_contact = remote->account_contact;
	gchar buff[BUF_SIZE];
	gchar buff2[BUF_SIZE];
	struct tm * cur_time;
	time_t t;
	LList * filter_walk;
	gchar * message, *temp_message, *link_message;
	gboolean firstmsg = FALSE; /* init to false so only play if
			      * first msg is one received rather
			      * than sent */
#ifdef __MINGW32__
	char *recoded;
#endif

	if (!o_message || strlen(o_message) == 0)
		return;

	/* do we need to ignore this user? If so, do it BEFORE filters so they can't DoS us */

	{
	char * group_name;
	if(remote_contact && remote_contact->group && remote_contact->group->name)
		group_name = remote_contact->group->name;
	else
		group_name = _("Unknown");

	/* don't translate this string */
	if(!strncmp(group_name, "__Ayttm_Dummy_Group__", strlen("__Ayttm_Dummy_Group__")))
		group_name = _("Unknown");

	if ( (iGetLocalPref("do_ignore_unknown") && !strcmp(_("Unknown"), group_name)) || 
			!strcasecmp(group_name, _("Ignore")) )
		return;
	}

	/* Inbound filters here - Meredydd */

	eb_debug(DBG_CORE, "Starting to run incoming filters\n");

	message=strdup(o_message);

	for(filter_walk=incoming_message_filters; filter_walk; filter_walk=filter_walk->next) {
		char * (*ofilter)(const eb_local_account *, const eb_account *, const struct contact *, const char *);
		char *otext = NULL;

		eb_debug(DBG_CORE, "Running an incoming filter:\n");
		ofilter=filter_walk->data;

		otext=ofilter(account, remote, remote_contact, message);
		free(message);
		message = otext;
		if(message==NULL) 
			return;
	}

	eb_debug(DBG_CORE, "Finished incoming filters\n");

	/* end inbound filters */

	/* TODO make these filters */
	if(RUN_SERVICE(account)->get_smileys)
		temp_message = eb_smilify(message, RUN_SERVICE(account)->get_smileys());
	else
		temp_message = g_strdup(message);
	link_message = linkify(temp_message);
	g_free(temp_message);
	g_free(message);
	message = link_message;

	if(!remote_contact->chatwindow || !remote_contact->chatwindow->window) {
		if(remote_contact->chatwindow)
			g_free(remote_contact->chatwindow);

		eb_chat_window_display_contact(remote_contact);

		if (!remote_contact->chatwindow)
		{
			if ( message != NULL )
				g_free( message );
				
			return;
		}
		
		gtk_widget_show(remote_contact->chatwindow->window);
		firstmsg = TRUE; 
		
		if ( iGetLocalPref("do_restore_last_conv") ) {
			gchar buff[NAME_MAX];
			make_safe_filename(buff, remote_contact->nick, remote_contact->group->name);
			eb_restore_last_conv(buff,remote_contact->chatwindow);
		}
	}

	/*for now we will assume the identity that the person in question talked
	to us last as */
	remote_contact->chatwindow->local_user = account;

	/*also specify that if possible, try to use the same account they used
	to last talk to us with */
	remote_contact->chatwindow->preferred = remote;

	if (remote_contact->chatwindow->notebook != NULL) {
		int remote_num = gtk_notebook_page_num(GTK_NOTEBOOK(remote_contact->chatwindow->notebook),
				      remote_contact->chatwindow->notebook_child);
		int current_num = gtk_notebook_get_current_page(GTK_NOTEBOOK(remote_contact->chatwindow->notebook));
		if (remote_num != current_num)
			set_tab_red(remote_contact->chatwindow);

	} else {
		if(iGetLocalPref("do_raise_window"))
			gdk_window_show(remote_contact->chatwindow->window->window);	  
	}

	eb_update_window_title(remote_contact->chatwindow, TRUE);

	if(remote_contact->chatwindow->sound_enabled) {
		if (firstmsg) {
			if (remote_contact->chatwindow->first_enabled)
				play_sound( SOUND_FIRSTMSG );
			else
				play_sound( SOUND_RECEIVE );
			firstmsg = FALSE; 
		}
		else if (remote_contact->chatwindow->receive_enabled)
			play_sound( SOUND_RECEIVE );
	}

	/*for raising the window*/

	if ( iGetLocalPref("do_raise_window") )
		gdk_window_show(remote_contact->chatwindow->window->window);

	if ( iGetLocalPref("do_convo_timestamp") ) {
		gchar * color;

		color=RUN_SERVICE(account)->get_color(); /* note do not free() afterwards, may be static */

		time(&t);
		cur_time = localtime(&t);
		g_snprintf(buff2, BUF_SIZE, "<FONT COLOR=\"%s\">%d:%.2d:%.2d</FONT> <FONT COLOR=\"%s\">%s:</FONT>",
			 color, cur_time->tm_hour, cur_time->tm_min,
			 cur_time->tm_sec, color, remote_contact->nick);
	}
	else
		g_snprintf(buff2, BUF_SIZE, "%s", remote_contact->nick);

#ifdef __MINGW32__
	recoded = ay_str_to_utf8(message);
	g_free(message);
	message = recoded;
#endif

#ifdef HAVE_ICONV_H
	{
		char *recoded = recode_if_needed(message, RECODE_TO_LOCAL);
		g_free(message);
		message = recoded;
	}
#endif

	g_snprintf(buff, BUF_SIZE, "<B>%s </B>",buff2);

	gtk_eb_html_add(EXT_GTK_TEXT(remote_contact->chatwindow->chat), buff,0,0,0);
	gtk_eb_html_add(EXT_GTK_TEXT(remote_contact->chatwindow->chat), message,
	iGetLocalPref("do_ignore_back"), iGetLocalPref("do_ignore_fore"), iGetLocalPref("do_ignore_font"));
	gtk_eb_html_add(EXT_GTK_TEXT(remote_contact->chatwindow->chat), "<BR>",0,0,0);
#ifdef __MINGW32__
	redraw_chat_window(remote_contact->chatwindow->chat);
#endif

	/* Log the message */
	if ( iGetLocalPref("do_logging") )
		ay_log_file_message( remote_contact->chatwindow->logfile, buff, message );

	/* If user's away and hasn't yet sent the away message in the last 5 minutes,
	send, display, & log his away message.
	We might want to give the option of whether or not to always send the message.*/

	if(is_away && (time(NULL) - remote_contact->chatwindow->away_msg_sent) > 300) {
		char *awaymsg = get_away_message();
		send_message(NULL, remote_contact->chatwindow);
		RUN_SERVICE(account)->send_im(account, remote, awaymsg);
		time(&t);
		cur_time = localtime(&t);
		g_snprintf(buff, BUF_SIZE, "<FONT COLOR=\"#0000ff\"><B>%d:%.2d:%.2d %s: </B></FONT>",
			 cur_time->tm_hour, cur_time->tm_min, cur_time->tm_sec,
			 account->alias);
		gtk_eb_html_add(EXT_GTK_TEXT(remote_contact->chatwindow->chat), buff,0,0,0);
		gtk_eb_html_add(EXT_GTK_TEXT(remote_contact->chatwindow->chat), awaymsg,
				iGetLocalPref("do_ignore_back"), iGetLocalPref("do_ignore_fore"), iGetLocalPref("do_ignore_font"));
		gtk_eb_html_add(EXT_GTK_TEXT(remote_contact->chatwindow->chat), "<br>", 0,0,0);

		/* Note that the time the last away message has been sent */

		remote_contact->chatwindow->away_msg_sent = time(NULL);

		/* Log it */

		if ( iGetLocalPref("do_logging") )
			ay_log_file_message( remote_contact->chatwindow->logfile, buff, awaymsg );
		
		g_free(awaymsg);
	}
#ifdef __MINGW32__
	redraw_chat_window(remote_contact->chatwindow->chat);
#endif
	g_free(message);
}

void eb_chat_window_display_contact(struct contact * remote_contact)
{
	eb_account *remote_account = find_suitable_remote_account (NULL, remote_contact);
	eb_local_account *account = NULL;

	if (remote_account)
		account = find_suitable_local_account_for_remote (remote_account, NULL);

	if (!remote_contact->chatwindow || !remote_contact->chatwindow->window) {
		if (remote_contact->chatwindow)
			g_free (remote_contact->chatwindow);

		remote_contact->chatwindow = eb_chat_window_new (account, remote_contact);

		if (remote_contact->chatwindow) {
			gtk_widget_show (remote_contact->chatwindow->window);
			if ( iGetLocalPref("do_restore_last_conv") ) {
				gchar buff[NAME_MAX];
				make_safe_filename (buff, remote_contact->nick, remote_contact->group->name);
				eb_restore_last_conv (buff, remote_contact->chatwindow);
			}
			gdk_window_raise(remote_contact->chatwindow->window->window);
			/*ENTRY_FOCUS(remote_contact->chatwindow);*/
		}

	} else {
		gdk_window_raise(remote_contact->chatwindow->window->window);
		/*ENTRY_FOCUS(remote_contact->chatwindow);*/
	}

	if (remote_contact->chatwindow->notebook != NULL) {
		int page_num = gtk_notebook_page_num (GTK_NOTEBOOK
				     (remote_contact->chatwindow->notebook),
				     remote_contact->chatwindow->notebook_child);
		chat_window *current = NULL;
		set_tab_normal (remote_contact->chatwindow);

		current = find_tabbed_chat_window();
		if (current)
			GET_CHAT_WINDOW(current);
		gtk_signal_handler_block_by_func(GTK_OBJECT(remote_contact->chatwindow->notebook),
                                         chat_notebook_switch_callback, remote_contact->chatwindow);
		if (current && current!=remote_contact->chatwindow) {
			char *text = gtk_editable_get_chars(GTK_EDITABLE(current->entry),0,-1);
			if (strlen(text) == 0) {
				gtk_notebook_set_page (GTK_NOTEBOOK
					   (remote_contact->chatwindow->notebook), page_num);
				ENTRY_FOCUS(remote_contact->chatwindow);
			}
			g_free(text);
		} else {
			gtk_notebook_set_page (GTK_NOTEBOOK
				   (remote_contact->chatwindow->notebook), page_num);
			ENTRY_FOCUS(remote_contact->chatwindow);
		}
		gtk_signal_handler_unblock_by_func(GTK_OBJECT(remote_contact->chatwindow->notebook),
                                         chat_notebook_switch_callback, remote_contact->chatwindow);
	}
}

void eb_chat_window_display_account(eb_account * remote_account)
{
	struct contact * remote_contact = NULL;
	eb_local_account * account = NULL;
	
	if (!remote_account)
		return;
	
	remote_contact = remote_account->account_contact;
	
	account = find_suitable_local_account_for_remote(remote_account, NULL);

	if(!remote_contact)
		return;
	
	if(!remote_contact->chatwindow || !remote_contact->chatwindow->window) {
		if(remote_contact->chatwindow)
			g_free(remote_contact->chatwindow);

		remote_contact->chatwindow = eb_chat_window_new(account, remote_contact);

		if(remote_contact->chatwindow) {
			gtk_widget_show(remote_contact->chatwindow->window);
			if ( iGetLocalPref("do_restore_last_conv") )  {
				gchar buff[NAME_MAX];
				make_safe_filename(buff, remote_contact->nick, remote_contact->group->name);
				eb_restore_last_conv(buff,remote_contact->chatwindow);	
			}
			gdk_window_raise(remote_contact->chatwindow->window->window);
			/*ENTRY_FOCUS(remote_contact->chatwindow);*/
		} else /* Did they get denied because they're in the Unknown group? */
			return;
	} else if (remote_contact->chatwindow && remote_contact->chatwindow->window) {
		gdk_window_raise(remote_contact->chatwindow->window->window);
		/*ENTRY_FOCUS(remote_contact->chatwindow);*/
	}       
    
	eb_update_window_title(remote_contact->chatwindow, FALSE);
	
	if (remote_contact->chatwindow->notebook != NULL)  {
		int page_num = gtk_notebook_page_num(GTK_NOTEBOOK(remote_contact->chatwindow->notebook),
			                		 remote_contact->chatwindow->notebook_child);

		chat_window *current = NULL;
		set_tab_normal (remote_contact->chatwindow);

		current = find_tabbed_chat_window();
		GET_CHAT_WINDOW(current);
		gtk_signal_handler_block_by_func(GTK_OBJECT(remote_contact->chatwindow->notebook),
                                         chat_notebook_switch_callback, remote_contact->chatwindow);
		if (current && current!=remote_contact->chatwindow) {
			char *text = gtk_editable_get_chars(GTK_EDITABLE(current->entry),0,-1);
			if (strlen(text) == 0) {
				gtk_notebook_set_page (GTK_NOTEBOOK
					   (remote_contact->chatwindow->notebook), page_num);
				ENTRY_FOCUS(remote_contact->chatwindow);
			}
			g_free(text);
		} else {
			gtk_notebook_set_page (GTK_NOTEBOOK
				   (remote_contact->chatwindow->notebook), page_num);
			ENTRY_FOCUS(remote_contact->chatwindow);
		}
		gtk_signal_handler_unblock_by_func(GTK_OBJECT(remote_contact->chatwindow->notebook),
                                         chat_notebook_switch_callback, remote_contact->chatwindow);
	}
	remote_contact->chatwindow->preferred = remote_account;
}

void eb_log_status_changed(eb_account *ea, gchar *status)
{
	char buff[BUF_SIZE];
	time_t my_time = time(NULL);
	
	if(ea == NULL || ea->account_contact == NULL || 
			ea->account_contact->chatwindow == NULL ||
			ea->account_contact->chatwindow->logfile == NULL)
		return;
	
	g_snprintf(buff, BUF_SIZE,_("<body bgcolor=#F9E589 width=*><b> %s changed status to %s @ %s.</b></body>"),
		   ea->account_contact->nick, ((status && status[0])?status:_("(Online)")), 
		   g_strchomp(asctime(localtime(&my_time))));

	ay_log_file_message( ea->account_contact->chatwindow->logfile, buff, "" );
}

void eb_restore_last_conv(gchar *file_name, chat_window* cw)
{
	FILE * fp;
	gchar buff[1024];
	gchar buff2[1024];
	gchar *buff3;
	gchar color[8];
	gchar name[512];
	gchar *token;
	long location = -1;
	long lastlocation = -1;
	long beforeget;
	gboolean endreached = FALSE;

	if ( (fp = fopen(file_name, "r")) == NULL)
		return;

	/*find last conversation */
	while(!feof(fp)) {
		beforeget = ftell(fp);
		fgets(buff, 1024, fp);
		if(feof(fp))
			break;
		g_strchomp(buff);
		if(!strncmp(buff,_("Conversation started"),strlen(_("Conversation started")))
		|| !strncmp(buff,_("<HR WIDTH=\"100%%\"><P ALIGN=\"CENTER\"><B>Conversation started"),strlen(_("<HR WIDTH=\"100%%\"><P ALIGN=\"CENTER\"><B>Conversation started")))
		|| !strncmp(buff,_("<HR WIDTH=\"100%\"><B>Conversation started"),strlen(_("<HR WIDTH=\"100%\"><B>Conversation started")))) {
			lastlocation = location;
			location = beforeget;
		}
	}

	if(lastlocation == -1 && location == -1) {
		fclose(fp);
		return;
	}

	if (lastlocation < location)
		lastlocation = location;  
	
	fseek(fp,lastlocation, SEEK_SET);

	if ( cw->logfile ) {
		cw->logfile->filepos = lastlocation;
		eb_debug(DBG_CORE,"set cw->logfile->filepos to %lu\n",cw->logfile->filepos);
	} 
	
	/* now we display the log */

	ext_gtk_text_freeze(EXT_GTK_TEXT(cw->chat));
  
	while(!feof(fp)) {
		fgets(buff,1024,fp);
		if(feof(fp))
			break;
		
		g_strchomp(buff);

		if(buff[0] == '<') /*this is html*/ {

			if(!strncmp(buff,"<HR WIDTH=\"100%\">", strlen("<HR WIDTH=\"100%\">"))) {
				char buff2[1024];
				snprintf(buff2, 1024, _("<body bgcolor=#F9E589 width=*><b> %s</b></body>"), 
						buff+strlen("<HR WIDTH=\"100%\">"));
				gtk_eb_html_add(EXT_GTK_TEXT(cw->chat), buff2, 0,0,0);
			} else if(!strncmp(buff,"<HR WIDTH=\"100%%\"><P ALIGN=\"CENTER\">", strlen("<HR WIDTH=\"100%%\"><P ALIGN=\"CENTER\">"))) {
				char buff2[1024];
				snprintf(buff2, 1024, _("<body bgcolor=#F9E589 width=*><b> %s</b></body>"), 
						buff+strlen("<HR WIDTH=\"100%%\"><P ALIGN=\"CENTER\">"));
				gtk_eb_html_add(EXT_GTK_TEXT(cw->chat), buff2, 0,0,0);
			} else if(!strncmp(buff,"<P ALIGN=\"CENTER\">", strlen("<P ALIGN=\"CENTER\">"))) {
				char buff2[1024];
				snprintf(buff2, 1024, _("<body bgcolor=#F9E589 width=*><b> %s</b></body>"), 
						buff+strlen("<P ALIGN=\"CENTER\">"));
				gtk_eb_html_add(EXT_GTK_TEXT(cw->chat), buff2, 0,0,0);
			} else if(strlen(buff) > strlen(_("<B>Conversation ")) 
			     && !strncmp(buff+strlen(_("<B>Conversation ")),_("ended on"),8)) {
				char buff2[1024];
				snprintf(buff2, 1024, _("<body bgcolor=#F9E589 width=*><b> %s</b></body>\n"), 
						buff);
				gtk_eb_html_add(EXT_GTK_TEXT(cw->chat), buff2, 0,0,0);
				endreached = TRUE;	
				break;
			} else if(strlen(buff) > strlen(_("<P ALIGN=\"CENTER\"><B>Conversation "))
			     && !strncmp(buff+strlen(_("<P ALIGN=\"CENTER\"><B>Conversation ")),_("ended on"),8) ) {
				char buff2[1024];
				snprintf(buff2, 1024, _("<body bgcolor=#F9E589 width=*><b> %s</b></body>\n"), 
						buff+strlen("<P ALIGN=\"CENTER\">"));
				gtk_eb_html_add(EXT_GTK_TEXT(cw->chat), buff2, 0,0,0);
				endreached = TRUE;	
				break;
			} else if (strlen(buff) > strlen(_("<P><hr><b>"))
			  &&  !strncmp(buff, _("<P><hr><b>"), strlen(_("<P><hr><b>")))
			  &&  strstr(buff, _("changed status to"))) {
				char buff2[1024];
				char *temp = strdup(buff);
				char *itemp = temp;
				itemp += strlen(_("<P><hr><b>"));
				*strstr(itemp, "<hr>") = '\0';
				snprintf(buff2, 1024, _("<body bgcolor=#F9E589 width=*><b> %s</b></body>"), 
						itemp);
				gtk_eb_html_add(EXT_GTK_TEXT(cw->chat), buff2, 0,0,0);
				free(temp);
			} else {
				gtk_eb_html_log_parse_and_add( EXT_GTK_TEXT(cw->chat), buff );
			}

			gtk_eb_html_add(EXT_GTK_TEXT(cw->chat), "<br>",0,0,0);
		} else if(!strncmp(buff,_("Conversation started"),strlen(_("Conversation started")))) {
			char buff2[1024];
			snprintf(buff2, 1024, _("<body bgcolor=#F9E589 width=*><b> %s</b></body>"), buff);
			gtk_eb_html_add(EXT_GTK_TEXT(cw->chat), buff2, 0,0,0);

		} else if(!strncmp(buff,_("Conversation ended"),strlen(_("Conversation ended")))) {
			char buff2[1024];
			snprintf(buff2, 1024, _("<body bgcolor=#F9E589 width=*><b> %s</b></body>"), buff);
			gtk_eb_html_add(EXT_GTK_TEXT(cw->chat), buff2, 0,0,0);
			endreached = TRUE;	    
			break;
		} else {
			int has_space = 0, i=0;
			strip_html(buff); /*better safe than sorry */
			strncpy(buff2, buff, sizeof(buff2));

			token = strtok(buff2,":");
			while (token && token[i]) {
				if(isspace(token[i])) {
					has_space=1;
					break;
				}
				i++;
			}
			
			if(token && !has_space && (strcmp(buff,token) != 0)) {
				/* not happy with this if statement at all! */
				if(((strlen(token)==3)&&isdigit((int)token[1])&&isdigit(token[2]))
				|| ((strlen(token)==2) && isdigit((int)token[1]))) {
					/* we must have time stamps */
					/* allready have hours */
					token = strtok(NULL,":"); /*minutes*/
					if(token == NULL)
						break; /* need to test this */

					token = strtok(NULL,":"); /*seconds + name*/

					if(token == NULL) /* we were wrong, this isn't a time stamp */
						break; /* need to test this */
					buff3 = token + strlen(token)+1; /* should be the end
					of the screen name */
					token+=3;
				} else {
					/* no time stamps */
					buff3 = buff2+strlen(token)+1;
					token++;
				}
				if( !strncmp(token,cw->contact->nick,strlen(cw->contact->nick)))
					strcpy(color,"#ff0000");
				else
					strcpy(color,"#0000ff");

				strncpy(name,buff,buff3-buff2);
				name[buff3-buff2] = '\0';
				g_snprintf(buff, BUF_SIZE, "<FONT COLOR=\"%s\"><B>%s </B></FONT>",color, name);
				gtk_eb_html_add(EXT_GTK_TEXT(cw->chat), buff,0,0,0);
				gtk_eb_html_add(EXT_GTK_TEXT(cw->chat), buff3,
						iGetLocalPref("do_ignore_back"), 
						iGetLocalPref("do_ignore_fore"), 
						iGetLocalPref("do_ignore_font"));
				gtk_eb_html_add(EXT_GTK_TEXT(cw->chat), "<br>",0,0,0);
			} else {
				/* hmm, no ':' must be a non start/blank line */
				gtk_eb_html_add(EXT_GTK_TEXT(cw->chat),buff,	
						iGetLocalPref("do_ignore_back"), 
						iGetLocalPref("do_ignore_fore"), 
						iGetLocalPref("do_ignore_font"));
				gtk_eb_html_add(EXT_GTK_TEXT(cw->chat), "<br>",0,0,0);
			}
   		}
	}
	if (!endreached) {
		char *endbuf = g_strdup_printf(_("<body bgcolor=#F9E589 width=*>%sConversation ended%s</body>\n"),
			(iGetLocalPref("do_strip_html") ? "" : "<B>"),
			(iGetLocalPref("do_strip_html") ? "" : "</B>")); 
		gtk_eb_html_add(EXT_GTK_TEXT(cw->chat), endbuf,0,0,0);
		g_free(endbuf);
	}  
	ext_gtk_text_thaw(EXT_GTK_TEXT(cw->chat));
	gtk_editable_set_position(GTK_EDITABLE(cw->chat), 
		  ext_gtk_text_get_length(EXT_GTK_TEXT(cw->chat)));
	fclose(fp);
}

void eb_chat_window_display_status(eb_account * remote,
				   gchar * message)
{
	struct contact * remote_contact = remote->account_contact;
	char * tmp = NULL;

	if(remote_contact == NULL)
		remote_contact = find_contact_by_handle(remote->handle);

	/* trim @.* part for User is typing */
	if (remote_contact == NULL || remote_contact->chatwindow == NULL) {
		gchar ** tmp_ct = NULL;
		if (strchr (remote->handle,'@') ) {
			tmp_ct = g_strsplit (remote->handle,"@",2);
			remote_contact = find_contact_by_handle(tmp_ct[0]);
			g_strfreev (tmp_ct);
		}
	}

	if (remote_contact == NULL || remote_contact->chatwindow == NULL)
		return;

	if (message != NULL && strlen(message) > 0)
		tmp = g_strdup_printf("%s", message);
	else
		tmp = g_strdup_printf(" ");

	gtk_label_set_text( GTK_LABEL(remote_contact->chatwindow->status_label), tmp );
	g_free(tmp);
}

static void eb_update_window_title_to_tab (int tab, gboolean new_message)
{
	char buff[BUF_SIZE];
	chat_window * cw = NULL;

	cw = find_tabbed_chat_window_index(tab);
	if (cw && cw->notebook && cw->contact && cw->local_user
	&& cw->preferred && GET_SERVICE (cw->local_user).name) {
		g_snprintf (buff, BUF_SIZE, "%s%s (%s <=> %s via %s)",
			new_message == TRUE ? "* " : "",
			cw->contact->nick, cw->local_user->alias,
			cw->preferred->handle,
			GET_SERVICE (cw->local_user).name);

		gtk_window_set_title (GTK_WINDOW (cw->contact->chatwindow->window), buff);
	} else if(cw && cw->contact != NULL)
      		gtk_window_set_title (GTK_WINDOW (cw->contact->chatwindow->window),
			cw->contact->nick);
}

static void eb_update_window_title (chat_window * cw, gboolean new_message)
{
	char buff[BUF_SIZE];
	const int tabbedChat = iGetLocalPref("do_tabbed_chat");

	if (!tabbedChat && cw && cw->contact && cw->preferred
	&& cw->local_user && GET_SERVICE (cw->local_user).name) {
		g_snprintf (buff, BUF_SIZE, "%s%s (%s <=> %s via %s)",
			new_message == TRUE ? "* " : "",
			cw->contact->nick, cw->local_user->alias,
			cw->preferred->handle, GET_SERVICE (cw->local_user).name);

		gtk_window_set_title (GTK_WINDOW (cw->contact->chatwindow->window),
			buff);
	} else if ( tabbedChat ) {
		if (!new_message) 	  
			cw = find_tabbed_current_chat_window ();
		if (cw && cw->notebook != NULL && cw->contact  && cw->local_user 
		&& cw->preferred && GET_SERVICE (cw->local_user).name) {
			g_snprintf (buff, BUF_SIZE, "%s%s (%s <=> %s via %s)",
				new_message == TRUE ? "* " : "",
				cw->contact->nick, cw->local_user->alias,
				cw->preferred->handle,
				GET_SERVICE (cw->local_user).name);

			gtk_window_set_title (GTK_WINDOW (cw->contact->chatwindow->window),
				buff);
		}
	}
	if(new_message)
		flash_title(cw->contact->chatwindow->window->window);
}


static void	destroy_smiley_cb_data(GtkWidget *widget, gpointer data)
{
	smiley_callback_data *scd = data;
	if ( !data )
		return;

	g_free( scd );
}

void layout_chatwindow (chat_window *cw, GtkWidget *vbox, char *name)
{
	chat_window *tab_cw = NULL;
	char buff[1024];
	int pos;
	GtkWidget *contact_label = NULL;
	
	/* we're doing a tabbed chat */
	if (iGetLocalPref("do_tabbed_chat")) {
		/* look for an already open tabbed chat window */
		tab_cw = find_tabbed_chat_window();
		if (tab_cw == NULL) {
			/* none exists, create one */
			cw->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
			gtk_window_set_wmclass(GTK_WINDOW(cw->window), "ayttm-chat", "Ayttm");
			gtk_window_set_policy(GTK_WINDOW(cw->window), TRUE, TRUE, TRUE);
			gtk_widget_realize(cw->window);

			cw->notebook = gtk_notebook_new();

			/* Set tab orientation.... */
			pos = GTK_POS_BOTTOM;
			switch ( iGetLocalPref("do_tabbed_chat_orient") ) {
				case 1: 
					pos = GTK_POS_TOP; break;
				case 2:
					pos = GTK_POS_LEFT; break;
				case 3:
					pos = GTK_POS_RIGHT; break;
				case 0:
				default:
					pos = GTK_POS_BOTTOM; break;
			}
			gtk_notebook_set_tab_pos(GTK_NOTEBOOK(cw->notebook), pos);
			/* End tab orientation */

			gtk_notebook_set_scrollable(GTK_NOTEBOOK(cw->notebook), TRUE);
			gtk_container_add(GTK_CONTAINER(cw->window), cw->notebook);

			/* setup a signal handler for the notebook to handle page switches */
			gtk_signal_connect(GTK_OBJECT(cw->notebook), "switch-page",
				       GTK_SIGNAL_FUNC(chat_notebook_switch_callback),
				       cw);

			gtk_widget_show(cw->notebook);
		} else {
			cw->window = tab_cw->window;
			cw->notebook = tab_cw->notebook;
		}

		if (strlen(name) > 20) {
			strncpy(buff, name, 17);
			buff[17]='\0';
			strcat(buff, "..."); /* buff is large enough */
		} else
			strncpy(buff, name, sizeof(buff));

		/* set up the text and close button */
		contact_label = gtk_label_new(buff);
		gtk_widget_show(contact_label);

		/* we use vbox as our child. */
		gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
		cw->notebook_child = vbox;
		gtk_notebook_append_page(GTK_NOTEBOOK(cw->notebook), cw->notebook_child, contact_label);
		gtk_widget_show(cw->notebook_child);
	} else {
		/* setup like normal */
		cw->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

		gtk_window_set_wmclass(GTK_WINDOW(cw->window), "ayttm-chat", "Ayttm");
		gtk_window_set_policy(GTK_WINDOW(cw->window), TRUE, TRUE, TRUE);
		gtk_widget_realize(cw->window);

		cw->notebook = NULL;
		cw->notebook_child = NULL;
		gtk_container_add(GTK_CONTAINER(cw->window), vbox);
		gtk_widget_show(vbox);
	}
}

chat_window * eb_chat_window_new(eb_local_account * local, struct contact * remote)
{
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *scrollwindow;
	GtkWidget *toolbar;
	GtkWidget *add_button;
	GtkWidget *sendf_button;
	GtkWidget *send_button;
	GtkWidget *view_log_button;
	GtkWidget *print_button;
	GtkWidget *close_button;
	GtkWidget *ignore_button;
	GtkWidget *iconwid;
	GdkPixmap *icon;
	GdkBitmap *mask;
	GtkAccelGroup *accel_group;
	GtkWidget *menu;
	GtkWidget *button;
	GtkWidget *separator;
	GtkWidget *resize_bar;
	gchar buff[NAME_MAX];
	chat_window *cw;
	chat_window *tab_cw=NULL;
	GtkPositionType pos;
	GtkWidget *contact_label;
	gboolean	enableSoundButton = FALSE;
	const int	tabbedChat = iGetLocalPref("do_tabbed_chat");

	if ( iGetLocalPref("do_ignore_unknown") && !strcmp(_("Unknown"), remote->group->name))
		return NULL;

	/* first we allocate room for the new chat window */
	cw = g_new0(chat_window,1);
	cw->contact = remote;
	cw->away_msg_sent = (time_t)NULL;
	cw->away_warn_displayed = (time_t)NULL;
	cw->preferred = NULL;
	cw->local_user = NULL;
	cw->smiley_window = NULL;

	vbox = gtk_vbox_new(FALSE,0);	

	layout_chatwindow(cw, vbox, remote->nick);
	
	/* Next line allows making window smaller than orig. size */
	cw->chat = ext_gtk_text_new(NULL,NULL);
	gtk_eb_html_init(EXT_GTK_TEXT(cw->chat));
	scrollwindow = gtk_scrolled_window_new(NULL, EXT_GTK_TEXT(cw->chat)->vadj);

	gtk_window_set_title(GTK_WINDOW(cw->window), remote->nick);    

	gtkut_set_window_icon(cw->window->window, NULL);	

	if (tab_cw)
		gtk_widget_set_usize(cw->chat, 
					tab_cw->chat->allocation.width, 
					tab_cw->chat->allocation.height);
	else
		gtk_widget_set_usize(cw->chat, 400, 150);

	gtk_container_add(GTK_CONTAINER(scrollwindow), cw->chat);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwindow), 
				       GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_widget_show(scrollwindow);

	/* Create the bar for resizing chat/window box */

	/*Add stuff for multi-line*/

	if( iGetLocalPref("do_multi_line") ) {
		resize_bar = gtk_vpaned_new();
		gtk_paned_set_gutter_size(GTK_PANED(resize_bar), 20);
		gtk_paned_pack1(GTK_PANED(resize_bar), scrollwindow, TRUE, TRUE);
		gtk_widget_show(scrollwindow);

		scrollwindow = gtk_scrolled_window_new(NULL, NULL);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (scrollwindow), 
			   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

		cw->entry = gtk_text_new(NULL, NULL);

		if (tab_cw)
			gtk_widget_set_usize(cw->entry, 
						tab_cw->entry->allocation.width, 
						tab_cw->entry->allocation.height);
		else
			gtk_widget_set_usize(cw->entry, 400, 50);
		gtk_container_add(GTK_CONTAINER(scrollwindow), cw->entry);

		gtk_text_set_editable(GTK_TEXT(cw->entry), TRUE);
		gtk_text_set_word_wrap(GTK_TEXT(cw->entry), TRUE);
		gtk_text_set_line_wrap(GTK_TEXT(cw->entry), TRUE);

#ifdef HAVE_LIBPSPELL
		if( iGetLocalPref("do_spell_checking") ) {
			gtkspell_attach(GTK_TEXT(cw->entry));
		}
#endif

		gtk_signal_connect(GTK_OBJECT(cw->entry), "key_press_event",
		GTK_SIGNAL_FUNC(chat_key_press), cw);

		gtk_paned_pack2(GTK_PANED(resize_bar), scrollwindow, FALSE, FALSE);
		gtk_widget_show(scrollwindow);
		gtk_box_pack_start(GTK_BOX(vbox),resize_bar, TRUE, TRUE, 5);
		gtk_widget_show(resize_bar);
	}

	/*Or not multi-line*/

	else {
		cw->entry = gtk_entry_new();
		gtk_signal_connect(GTK_OBJECT(cw->entry), "key_press_event",
			       GTK_SIGNAL_FUNC(chat_singleline_key_press),
			       cw);

		gtk_box_pack_start(GTK_BOX(vbox), scrollwindow, TRUE,TRUE, 5);
		gtk_box_pack_start(GTK_BOX(vbox), cw->entry, FALSE,FALSE, 5);
	}

	gtk_container_set_border_width(GTK_CONTAINER(cw->window), tabbedChat ? 2 : 5);

	gtk_signal_connect(GTK_OBJECT(cw->entry), "activate",
			   GTK_SIGNAL_FUNC(send_message), cw);

	gtk_signal_connect(GTK_OBJECT(cw->chat), "button_press_event",
			   GTK_SIGNAL_FUNC(handle_click), cw);                     	
	gtk_signal_connect(GTK_OBJECT(cw->window), "focus_in_event",
			   GTK_SIGNAL_FUNC(handle_focus), cw);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_widget_set_usize(hbox, 200, 25);

	toolbar = gtk_toolbar_new(GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_ICONS);
	gtk_toolbar_set_button_relief(GTK_TOOLBAR(toolbar), GTK_RELIEF_NONE);
	gtk_container_set_border_width(GTK_CONTAINER(toolbar), 0);
	gtk_toolbar_set_space_size(GTK_TOOLBAR(toolbar), 5);

	/* Adding accelerators to windows*/

	accel_group = gtk_accel_group_new();
	gtk_window_add_accel_group(GTK_WINDOW(cw->window), accel_group);
	menu = gtk_menu_new();

	/* This is the same as handle_clicks, without showing the menu for
	   accelerators to hopefully work better now. 
	*/

	/*Add Contact Selection*/

	if(!strcmp(cw->contact->group->name, _("Unknown"))
	|| !strncmp(cw->contact->group->name, "__Ayttm_Dummy_Group__", strlen("__Ayttm_Dummy_Group__"))) {
		button = gtk_menu_item_new_with_label(_("Add Contact"));
		gtk_signal_connect(GTK_OBJECT(button), "activate",
			       GTK_SIGNAL_FUNC(add_unknown_callback),
			       cw);
		gtk_menu_append(GTK_MENU(menu), button);
	}

	/*Allow Offline Messaging Selection*/

	if(can_offline_message(cw->contact)) {	
		button = gtk_menu_item_new_with_label(_("Offline Messaging"));
		gtk_signal_connect(GTK_OBJECT(button), "activate",
			       GTK_SIGNAL_FUNC(allow_offline_callback),
			       cw);
		gtk_widget_add_accelerator(button, "activate", accel_group, 
				       GDK_o, GDK_CONTROL_MASK,
				       GTK_ACCEL_VISIBLE);
		gtk_menu_append(GTK_MENU(menu), button);
	}

	/*Sound Selection*/

	if (cw->sound_enabled)
		button = gtk_menu_item_new_with_label(_("Disable Sounds"));
	else
		button = gtk_menu_item_new_with_label(_("Enable Sounds"));

	gtk_signal_connect(GTK_OBJECT(button), "activate",
			   GTK_SIGNAL_FUNC(set_sound_callback),
			   cw); 
	gtk_widget_add_accelerator(button, "activate", accel_group, 
				   GDK_s, GDK_CONTROL_MASK,
				   GTK_ACCEL_VISIBLE);
	gtk_menu_append(GTK_MENU(menu), button);

	/* Ignore button section */

	button = gtk_menu_item_new_with_label(_("Ignore Contact"));
	gtk_signal_connect(GTK_OBJECT(button), "activate",
			   GTK_SIGNAL_FUNC(ignore_callback), cw);
	gtk_widget_add_accelerator(button, "activate", accel_group,
				   GDK_g, GDK_CONTROL_MASK,
				   GTK_ACCEL_VISIBLE);
	gtk_menu_append(GTK_MENU(menu), button);

	/*Send File Selection*/ 

	button = gtk_menu_item_new_with_label(_("Send File"));
	gtk_signal_connect(GTK_OBJECT(button), "activate",
			   GTK_SIGNAL_FUNC(send_file),
			   cw);
	gtk_widget_add_accelerator(button, "activate", accel_group, 
				   GDK_t, GDK_CONTROL_MASK,
				   GTK_ACCEL_VISIBLE);
	gtk_menu_append(GTK_MENU(menu), button);

	/*Invite Selection*/ 

	button = gtk_menu_item_new_with_label(_("Invite"));
	gtk_signal_connect(GTK_OBJECT(button), "activate",
			   GTK_SIGNAL_FUNC(do_invite_window),
			   cw);
	gtk_widget_add_accelerator(button, "activate", accel_group, 
				   GDK_i, GDK_CONTROL_MASK,
				   GTK_ACCEL_VISIBLE);
	gtk_menu_append(GTK_MENU(menu), button);

	/*Send Selection*/

	button = gtk_menu_item_new_with_label(_("Send Message"));
	gtk_signal_connect(GTK_OBJECT(button), "activate",
			   GTK_SIGNAL_FUNC(send_message),
			   cw);
	gtk_widget_add_accelerator(button, "activate", accel_group, 
				   GDK_r, GDK_CONTROL_MASK,
				   GTK_ACCEL_VISIBLE);
	gtk_menu_append(GTK_MENU(menu), button);

	button = gtk_menu_item_new_with_label(_("View log"));
	gtk_signal_connect(GTK_OBJECT(button), "activate",
			   GTK_SIGNAL_FUNC(view_log_callback),
			   cw);

	gtk_widget_add_accelerator(button, "activate", accel_group, 
				   GDK_l, GDK_CONTROL_MASK,
				   GTK_ACCEL_VISIBLE);
	gtk_menu_append(GTK_MENU(menu), button);

#ifndef __MINGW32__
	button = gtk_menu_item_new_with_label(_("Actions..."));
	gtk_signal_connect(GTK_OBJECT(button), "activate",
			   GTK_SIGNAL_FUNC(action_callback),
			   cw);

	gtk_widget_add_accelerator(button, "activate", accel_group, 
				   GDK_p, GDK_CONTROL_MASK,
				   GTK_ACCEL_VISIBLE);
	gtk_menu_append(GTK_MENU(menu), button);
#endif
	
	/*Close Selection*/

	button = gtk_menu_item_new_with_label(_("Close"));

	gtk_signal_connect(GTK_OBJECT(button), "activate",
			   GTK_SIGNAL_FUNC(close_win),
			   cw);

	gtk_widget_add_accelerator(button, "activate", accel_group,
				   GDK_q, GDK_CONTROL_MASK,
				   GTK_ACCEL_VISIBLE);
	gtk_menu_append(GTK_MENU(menu), button);  

	/*This is we decide whether or not the add button should be displayed*/

#define TOOLBAR_APPEND(txt,icn,cbk,cwx) \
	gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),txt,txt,txt,icn,GTK_SIGNAL_FUNC(cbk),cwx); 
#define TOOLBAR_SEPARATOR() {\
	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));\
	separator = gtk_vseparator_new();\
	gtk_widget_set_usize(GTK_WIDGET(separator), 0, 20);\
	gtk_toolbar_append_widget(GTK_TOOLBAR(toolbar), separator, NULL, NULL);\
	gtk_widget_show(separator);\
	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar)); }
#define ICON_CREATE(icon,iconwid,xpm) {\
	icon = gdk_pixmap_create_from_xpm_d(cw->window->window, &mask, NULL, xpm); \
	iconwid = gtk_pixmap_new(icon, mask); \
	gtk_widget_show(iconwid); }


	if (!strcmp(cw->contact->group->name, _("Unknown"))
	|| !strncmp(cw->contact->group->name, "__Ayttm_Dummy_Group__", strlen("__Ayttm_Dummy_Group__"))) {
		ICON_CREATE(icon, iconwid, tb_book_red_xpm);
		add_button = TOOLBAR_APPEND(_("Add Contact"),iconwid,add_unknown_callback,cw);
		gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));
	}

	/*Decide whether the offline messaging button should be displayed*/

	if(can_offline_message(remote)) {
		ICON_CREATE(icon, iconwid, tb_edit_xpm);
		cw->offline_button = gtk_toolbar_append_element(GTK_TOOLBAR(toolbar),
							  GTK_TOOLBAR_CHILD_TOGGLEBUTTON,
							  NULL,
							  _("Allow"),
							  _("Allow Offline Messaging CTRL+O"),
							  _("Allow"),
							  iconwid,
							  GTK_SIGNAL_FUNC(allow_offline_callback),
							  cw);
		gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));
		cw_set_offline_active(cw, FALSE);
	}

	/* smileys */
	if( iGetLocalPref("do_smiley") ) {
		smiley_callback_data * scd = g_new0(smiley_callback_data,1);
		scd->c_window = cw;

		ICON_CREATE(icon, iconwid, smiley_button_xpm);
		cw->smiley_button = TOOLBAR_APPEND(_("Insert Smiley"), iconwid, _show_smileys_cb, scd);
		gtk_signal_connect(GTK_OBJECT(cw->smiley_button), "destroy",
			   GTK_SIGNAL_FUNC(destroy_smiley_cb_data), scd);
		/*Create the separator for the toolbar*/

		TOOLBAR_SEPARATOR();
	}
	/*This is the sound toggle button*/

	ICON_CREATE(icon, iconwid, tb_volume_xpm);
	cw->sound_button = gtk_toolbar_append_element(GTK_TOOLBAR(toolbar),
						      GTK_TOOLBAR_CHILD_TOGGLEBUTTON,
						      NULL,
						      _("Sound"),
						      _("Enable Sounds CTRL+S"),
						      _("Sound"),
						      iconwid,
						      GTK_SIGNAL_FUNC(set_sound_callback),
						      cw);

	/* Toggle the sound button based on preferences */
	cw->send_enabled    = iGetLocalPref("do_play_send");
	cw->receive_enabled = iGetLocalPref("do_play_receive");
	cw->first_enabled   = iGetLocalPref("do_play_first");

	enableSoundButton  = iGetLocalPref("do_play_send");
	enableSoundButton |= iGetLocalPref("do_play_receive");
	enableSoundButton |= iGetLocalPref("do_play_first");

	cw_set_sound_active(cw, enableSoundButton);

	ICON_CREATE(icon, iconwid, tb_search_xpm);
	view_log_button = TOOLBAR_APPEND(_("View Log CTRL+L"), iconwid, view_log_callback, cw);
	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar)); 

#ifndef __MINGW32__
	ICON_CREATE(icon, iconwid, action_xpm);
	print_button = TOOLBAR_APPEND(_("Actions..."), iconwid, action_callback, cw);
	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar)); 
#endif
	
	TOOLBAR_SEPARATOR();

	/*This is the send file button*/

	ICON_CREATE(icon, iconwid, tb_open_xpm);
	sendf_button = TOOLBAR_APPEND(_("Send File CTRL+T"), iconwid, send_file, cw);
	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar)); 

	/*This is the invite button*/

	ICON_CREATE(icon, iconwid, invite_btn_xpm);
	sendf_button = TOOLBAR_APPEND(_("Invite CTRL+I"), iconwid, do_invite_window, cw);
	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar)); 

	/*This is the ignore button*/

	ICON_CREATE(icon, iconwid, tb_no_xpm);
	ignore_button = TOOLBAR_APPEND(_("Ignore CTRL+G"), iconwid, ignore_callback, cw);
	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar)); 

	/*This is the send button*/

	ICON_CREATE(icon, iconwid, tb_mail_send_xpm);
	send_button = TOOLBAR_APPEND(_("Send Message CTRL+R"), iconwid, send_message, cw);

	TOOLBAR_SEPARATOR();

	/*This is the close button*/

	ICON_CREATE(icon, iconwid, cancel_xpm);

	if ( tabbedChat ) {
		close_button = TOOLBAR_APPEND(_("Close CTRL+Q"), iconwid, close_tab_callback, cw);
	} else {
		close_button = TOOLBAR_APPEND(_("Close CTRL+Q"), iconwid, close_win, cw);
	}
	
#undef TOOLBAR_APPEND
#undef ICON_CREATE
#undef TOOLBAR_SEPARATOR
	
	cw->status_label = gtk_label_new(" ");
	gtk_box_pack_start(GTK_BOX(hbox), cw->status_label, FALSE, FALSE, 0);
	gtk_widget_show(cw->status_label);

	gtk_box_pack_end(GTK_BOX(hbox), toolbar, FALSE, FALSE, 0);
	gtk_widget_show(toolbar);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	make_safe_filename(buff, remote->nick, remote->group->name);

	cw->logfile = ay_log_file_create( buff );
	ay_log_file_open( cw->logfile, "a" );
	
	gtk_widget_show(cw->chat);
	gtk_widget_show(cw->entry);

	gtk_signal_connect(GTK_OBJECT(vbox), "destroy",
			   GTK_SIGNAL_FUNC(destroy_event), cw);	

	return cw;
}

typedef struct _cr_wait
{
	eb_chat_room *cr;
	eb_account *second;
	eb_account *third;
	char *msg;
} cr_wait;

int wait_chatroom_connected(cr_wait *crw)
{
	if (crw->cr->connected == 0) {
		eb_debug(DBG_CORE, "chatroom still not ready\n");
		return 1;
	} else {
		RUN_SERVICE(crw->cr->local_user)->send_invite(crw->cr->local_user, crw->cr,crw->second->handle, crw->msg);
		RUN_SERVICE(crw->cr->local_user)->send_invite(crw->cr->local_user, crw->cr,crw->third->handle, crw->msg);
		free(crw->msg);
		free(crw);
		crw = NULL;
		return 0;
	}		
}

void chat_window_to_chat_room(chat_window *cw, eb_account *third_party, char *msg)
{
	eb_account *second_party = cw->preferred;
	eb_local_account *ela = cw->local_user;
	eb_chat_room *cr = NULL;
	cr_wait *crw = g_new0(cr_wait, 1);
	
	if (second_party == NULL) {
		return;
	}
	
	cr = eb_start_chat_room(ela, next_chatroom_name(), 0);
	crw->cr = cr;
	crw->second = second_party;
	crw->third = third_party;
	crw->msg = strdup(msg);
	eb_timeout_add(1000, (GtkFunction)wait_chatroom_connected, (gpointer)crw);
}
