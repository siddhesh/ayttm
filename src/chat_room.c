/*
 * Ayttm 
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

#include "intl.h"

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>

#include <gdk/gdkkeysyms.h>

#include "util.h"
#include "globals.h"
#include "sound.h"
#include "prefs.h"
#include "smileys.h"
#include "service.h"
#include "add_contact_window.h"
#include "action.h"
#include "messages.h"
#include "mem_util.h"

#include "gtk/gtk_eb_html.h"
#include "gtk/gtkutils.h"

#ifdef HAVE_LIBPSPELL
#include "gtk/gtkspell.h"
#endif

#include "pixmaps/tb_mail_send.xpm"
#include "pixmaps/cancel.xpm"
#include "pixmaps/ok.xpm"
#include "pixmaps/tb_volume.xpm"
#include "pixmaps/smiley_button.xpm"
#include "pixmaps/action.xpm"

LList * chat_rooms = NULL;

static LList * get_contacts( eb_chat_room * room );
static void handle_focus(GtkWidget *widget, GdkEventFocus * event, gpointer userdata);
static void eb_chat_room_update_window_title(eb_chat_room *ecb, gboolean new_message);

static void init_chat_room_log_file(eb_chat_room *chat_room);
static void	destroy_chat_room_log_file( eb_chat_room *chat_room );

static char *last_clicked_fellow = NULL;
static guint32 last_time_clicked = 0;

/* Not used yet */
/*
static const char *cr_colors[]={
	"#ff0055",
	"#ff009d",
	"#6e00ff",
	"#0072ff",
	"#00ffaa",
	"#21ff00",
	"#9dff00",
	"#ffff00",
	"#ff9400"
};
*/
static const int nb_cr_colors = 9;


static void	free_chat_room( eb_chat_room *chat_room )
{
	LList	*history = NULL;
	
	if ( chat_room == NULL )
		return;
		 
	chat_rooms = l_list_remove( chat_rooms, chat_room );

	destroy_chat_room_log_file( chat_room );
	
	history = chat_room->history;

	for ( ; history != NULL ; history = history->next)
		free( history->data );
	
	l_list_free( chat_room->history );
	
	memset( chat_room, 0, sizeof( eb_chat_room ) );
	g_free( chat_room );
}

static void handle_fellow_click (char *name, eb_chat_room *cr)
{
	eb_account *ea = NULL;
	LList *node = NULL;
	char *handle = NULL;
	
	for( node = cr->fellows; node; node = node->next )
	{
		eb_chat_room_buddy * ecrb = node->data;
		if (!strcasecmp(ecrb->alias, name)) {
			handle = ecrb->handle;
			break;
		}
	}
	
	if (find_account_by_handle(name, cr->local_user->service_id)) {
		ea = find_account_by_handle(name, cr->local_user->service_id);
	} else if (handle && find_account_by_handle(handle, cr->local_user->service_id)) {
		ea = find_account_by_handle(handle, cr->local_user->service_id);
	} else if (find_contact_by_nick(name)) {
		eb_chat_window_display_contact(find_contact_by_nick(name));
		return;
	} else {
		ea = RUN_SERVICE(cr->local_user)->new_account(cr->local_user, name);
		add_unknown_account_window_new(ea);
		return; /* as add_unknown_account_window_new isn't modal 
			we'll open the chat_window next time */
	}
	eb_chat_window_display_account(ea);
}

static void fellows_click( GtkWidget * widget, int row, int column,
			   GdkEventButton *evt, gpointer data )
{
	eb_chat_room * ecr = (eb_chat_room *)data;
	char *name = NULL;
	gtk_clist_unselect_all(GTK_CLIST(ecr->fellows_widget));
	if (gtk_clist_get_text(GTK_CLIST(ecr->fellows_widget), row, column, &name)) {
		if (last_clicked_fellow) {
			if (evt->time - last_time_clicked < 2000
			&& !strcmp(last_clicked_fellow, name)) {
				/* double-click ! */
				handle_fellow_click(name, ecr);
				free(last_clicked_fellow);
				last_clicked_fellow = NULL;
				return;
			}
			free(last_clicked_fellow);
		}
		last_clicked_fellow = strdup(name);
		last_time_clicked = evt->time;
	} else {
		if (last_clicked_fellow) {
			free(last_clicked_fellow);
		}
		last_clicked_fellow = NULL;
	}
}

static void send_cr_message( GtkWidget * widget, gpointer data )
{
	eb_chat_room * ecr = (eb_chat_room *)data;
	char *text = gtk_editable_get_chars(GTK_EDITABLE(ecr->entry), 0, -1);
	
	if (!text || strlen(text) == 0)
		return;

#ifdef __MINGW32__
	char *recoded = ay_utf8_to_str(text);
	g_free(text);
	text = recoded;
#endif

	RUN_SERVICE(ecr->local_user)->send_chat_room_message(ecr,text);

	if(ecr->this_msg_in_history)
	{
		LList * node=NULL;
		LList * node2=NULL;

		for(node=ecr->history; node!=NULL ; node=node->next)
		{
			node2=node;
		}
		free(node2->data);
		node2->data=strdup(text);
		ecr->this_msg_in_history=0;
	} else {
		ecr->history=l_list_append(ecr->history, strdup(text));
		ecr->hist_pos=NULL;
	}

	
	g_free(text);
	if(ecr->sound_enabled && ecr->send_enabled)
		play_sound( SOUND_SEND );
	gtk_editable_delete_text(GTK_EDITABLE (ecr->entry), 0, -1);
}

static void send_typing_status(eb_chat_room *cr)
{
	time_t now=time(NULL);
	/*LList *others;*/

	if(!iGetLocalPref("do_send_typing_notify"))
		return;

	if(now>=cr->next_typing_send &&
	   RUN_SERVICE(cr->local_user)->send_cr_typing!=NULL) {
		cr->next_typing_send = now+RUN_SERVICE(cr->local_user)->send_cr_typing(cr);
	}
}

static gboolean cr_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
  eb_chat_room *cr = (eb_chat_room *)data;
  GdkModifierType modifiers = event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_MOD4_MASK);

  eb_chat_room_update_window_title(cr, FALSE);

  if (!iGetLocalPref("do_multi_line"))
	  return FALSE;
  
  if (event->keyval == GDK_Return)
    {
      /* Just print a newline on Shift-Return */
      if (event->state & GDK_SHIFT_MASK)
	{
	  event->state = 0;
	}
      else if ( iGetLocalPref("do_enter_send") )
	{
	  /*Prevents a newline from being printed*/
	  gtk_signal_emit_stop_by_name(GTK_OBJECT(widget), "key_press_event");

	  send_cr_message(NULL, cr);
	  return gtk_true();
    	}
    }
  else if (event->keyval == GDK_Up)
    {
      gint p=0;

      if(cr->history==NULL) { return gtk_true(); }

      if(cr->hist_pos==NULL)
      {
        char * s;
        LList * node;

        s= gtk_editable_get_chars(GTK_EDITABLE (cr->entry), 0, -1);

        for(node=cr->history; node!=NULL ; node=node->next)
        {
          cr->hist_pos=node;
        }

        if(strlen(s)>0)
        {
          cr->history=l_list_append(cr->history, strdup(s));
          //cr->hist_pos=cr->history->next;
          g_free(s); // that strdup() followed by g_free() looks stupid, but it isn't
                        // - strdup() uses vanilla malloc(), and the destroy code uses
                        // vanilla free(), so we can't use something that needs to be
                        // g_free()ed (apparently glib uses an incompatible allocation
                        //system
          cr->this_msg_in_history=1;
        }

      } else {
        cr->hist_pos=cr->hist_pos->prev;
        if(cr->hist_pos==NULL)
        {
          LList * node;
          eb_debug(DBG_CORE,"history Wrapped!\n");
          for(node=cr->history; node!=NULL ; node=node->next)
          {
            cr->hist_pos=node;
          }
        }
      }

      gtk_editable_delete_text(GTK_EDITABLE (cr->entry), 0, -1);
      gtk_editable_insert_text(GTK_EDITABLE (cr->entry), cr->hist_pos->data, strlen(cr->hist_pos->data), &p);
    }
  else if (event->keyval == GDK_Down)
    {
      gint p=0;

      if(cr->history==NULL || cr->hist_pos==NULL) { return gtk_true(); }
      cr->hist_pos=cr->hist_pos->next;
      if(cr->hist_pos==NULL)
      {
        gtk_editable_delete_text(GTK_EDITABLE (cr->entry), 0, -1);
      } else {
        gtk_editable_delete_text(GTK_EDITABLE (cr->entry), 0, -1);
        gtk_editable_insert_text(GTK_EDITABLE (cr->entry), cr->hist_pos->data, strlen(cr->hist_pos->data), &p);
      }
    }

  if(modifiers)
  { return gtk_false(); }

  if(!modifiers)
  {
    send_typing_status(cr);
  }

  return gtk_false();
}

static void set_sound_on_toggle(GtkWidget * sound_button, gpointer userdata)
{
  eb_chat_room * cr = (eb_chat_room *)userdata;
   
  /*Set the sound_enable variable depending on the toggle button*/
   
  if (GTK_TOGGLE_BUTTON (sound_button)->active)
    cr->sound_enabled = TRUE;
  else
    cr->sound_enabled = FALSE;
}

static gint strcasecmp_glist(gconstpointer a, gconstpointer b)
{
	return strcasecmp((const char *)a, (const char *)b);
}

static GList * chat_service_list()
{
	GList * list = NULL;
	LList * walk = NULL;

	for (walk = accounts; walk; walk = walk->next) {
		eb_local_account *ela = (eb_local_account *)walk->data;
		if (ela && ela->connected && can_group_chat(eb_services[ela->service_id])) {
			char *str = g_strdup_printf("[%s] %s", get_service_name(ela->service_id), ela->handle);
			list = g_list_insert_sorted(list, str, strcasecmp_glist);
		}
	}
	return list;
}

static void invite_callback( GtkWidget * widget, gpointer data )
{
	eb_chat_room * ecr = data;
	char *acc = NULL;
	char * invited = gtk_editable_get_chars(GTK_EDITABLE(GTK_COMBO(ecr->invite_buddy)->entry),0,-1);
	if (!strstr(invited, "(") || !strstr(invited, ")"))
		return;
	acc = strstr(invited, "(")+1;
	*strstr(acc, ")") = '\0';
	RUN_SERVICE(ecr->local_user)->send_invite(
				ecr->local_user, ecr,
				acc,
				gtk_entry_get_text(GTK_ENTRY(ecr->invite_message)));
	g_free(invited);
	gtk_widget_destroy(ecr->invite_window);
}

static void destroy_invite( GtkWidget * widget, gpointer data )
{
	eb_chat_room * ecr = data;
	ecr->invite_window_is_open = 0;
}

static void do_invite_window(eb_chat_room * room )
{
	GtkWidget * box;
	GtkWidget * box2;
	GtkWidget * label;
	GList * list;
	
	if( !room || room->invite_window_is_open )
		return;

	room->invite_window = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_window_set_position(GTK_WINDOW(room->invite_window), GTK_WIN_POS_MOUSE);
	box = gtk_vbox_new(FALSE, 3);
	box2 = gtk_hbox_new(FALSE, 3);

	label = gtk_label_new(_("Handle: "));
	gtk_container_add(GTK_CONTAINER(box2), label);
	gtk_widget_show(label);

	room->invite_buddy = gtk_combo_new();
	list = llist_to_glist(get_contacts(room), 1);
	gtk_combo_set_popdown_strings(GTK_COMBO(room->invite_buddy), list);
	g_list_free(list);
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(room->invite_buddy)->entry), "");
	gtk_container_add(GTK_CONTAINER(box2), room->invite_buddy);
	gtk_widget_show(room->invite_buddy);

	gtk_container_add(GTK_CONTAINER(box), box2);
	gtk_widget_show(box2);

	box2 = gtk_hbox_new(FALSE,3);
	label = gtk_label_new(_("Message:"));
	gtk_container_add(GTK_CONTAINER(box2), label);
	gtk_widget_show(label);

	room->invite_message = gtk_entry_new();
	gtk_container_add(GTK_CONTAINER(box2), room->invite_message );
	gtk_widget_show(room->invite_message );

	gtk_container_add(GTK_CONTAINER(box), box2);
	gtk_widget_show(box2);

	label = gtk_button_new_with_label(_("Invite"));
	gtk_container_add(GTK_CONTAINER(box), label);
	gtk_widget_show(label);


	gtk_signal_connect(GTK_OBJECT(label), "clicked", invite_callback, room);

	gtk_container_add(GTK_CONTAINER(room->invite_window), box);
	gtk_widget_show(box);

	gtk_widget_show(room->invite_window);
	
	
	gtk_signal_connect( GTK_OBJECT(room->invite_window), "destroy",
						GTK_SIGNAL_FUNC(destroy_invite), room);
}

	

static gboolean join_service_is_open = 0;

static GtkWidget * chat_room_name;
static GtkWidget * chat_room_type;
static GtkWidget * join_chat_window;
static GtkWidget * public_chkbtn;
static GtkWidget * public_list_btn;

static int total_rooms;

char * next_chatroom_name(void)
{
	return g_strdup_printf(_("Chatroom %d"), ++total_rooms);
}

static void join_chat_callback(GtkWidget * widget, gpointer data )
{
	char *service = gtk_editable_get_chars(GTK_EDITABLE(GTK_COMBO(chat_room_type)->entry),0,-1);
	char *name = gtk_editable_get_chars(GTK_EDITABLE(chat_room_name),0,-1);
	char *mservice = NULL;
	char *local_acc = NULL;
	eb_local_account *ela = NULL;
	int service_id = -1;
	if (!strstr(service, "]") || !strstr(service," ")) {
		ay_do_error(_("Cannot join"), _("No local account specified."));
		g_free(service);
		return;
	}
	
	local_acc = strstr(service, " ") +1;

	*(strstr(service, "]")) = '\0';
	mservice = strstr(service,"[")+1;
	
	service_id = get_service_id( mservice );
	ela = find_local_account_by_handle(local_acc, service_id);
	
	g_free(service);
	
	if (!ela) {
		ay_do_error(_("Cannot join"), _("The local account doesn't exist."));
		return;			
	}

	if (!name || strlen(name) == 0)
		name = next_chatroom_name();
	
	eb_start_chat_room(ela, name, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(public_chkbtn)));
	g_free(name);
	gtk_widget_destroy(join_chat_window);
}

static void update_public_sensitivity(GtkWidget * widget, gpointer data ) {
	char *service = gtk_editable_get_chars(GTK_EDITABLE(GTK_COMBO(chat_room_type)->entry),0,-1);
	char *mservice = NULL;
	char *local_acc = NULL;
	int service_id = -1;
	int has_public = 0;
	
	if (!strstr(service, "]") || !strstr(service," ")) {
		g_free(service);
		return;
	}
	
	local_acc = strstr(service, " ") +1;
	*(strstr(service, "]")) = '\0';
	mservice = strstr(service,"[")+1;
	
	service_id = get_service_id( mservice );
	g_free(service);
	
	has_public = (eb_services[service_id].sc->get_public_chatrooms != NULL);
	gtk_widget_set_sensitive(public_chkbtn, has_public);
	gtk_widget_set_sensitive(public_list_btn, has_public);
	if (has_public) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(public_chkbtn), FALSE);
	}
}

static void choose_list_cb(char *text, gpointer data) {
	int pos;
	
	gtk_editable_delete_text(GTK_EDITABLE(chat_room_name), 0, -1);
	gtk_editable_insert_text(GTK_EDITABLE(chat_room_name), text, strlen(text), &pos);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(public_chkbtn), TRUE);
}

static void list_public_chatrooms (GtkWidget *widget, gpointer data) {
	char *service = gtk_editable_get_chars(GTK_EDITABLE(GTK_COMBO(chat_room_type)->entry),0,-1);
	char *mservice = NULL;
	char *local_acc = NULL;
	int service_id = -1;
	int has_public = 0;
	LList *list = NULL;
	eb_local_account *ela = NULL;
	if (!strstr(service, "]") || !strstr(service," ")) {
		g_free(service);
		return;
	}
	
	local_acc = strstr(service, " ") +1;
	*(strstr(service, "]")) = '\0';
	mservice = strstr(service,"[")+1;
	
	service_id = get_service_id( mservice );
	ela = find_local_account_by_handle(local_acc, service_id);
	
	g_free(service);
	
	if (!ela) {
		ay_do_error(_("Cannot list chatrooms"), _("The local account doesn't exist."));
		return;			
	}

	list = RUN_SERVICE(ela)->get_public_chatrooms(ela);
	
	if (!list) {
		ay_do_error(_("Cannot list chatrooms"), _("The local account doesn't exist."));
		return;			
	}

	do_llist_dialog(_("Select a chatroom."), _("Public chatrooms list"), list, choose_list_cb, NULL);
	
	l_list_free(list);
}

static void join_chat_destroy(GtkWidget * widget, gpointer data )
{
	join_service_is_open = 0;
}

/*
 *  Let's build ourselfs a nice little dialog window to
 *  ask us what chat window we want to join :)
 */

void open_join_chat_window()
{
	GtkWidget * label;
	GtkWidget * frame;
	GtkWidget * table;
	GtkWidget * vbox;
	GtkWidget * hbox;
	GtkWidget * hbox2;
	GtkWidget * button;
	GtkWidget * separator;
	
	GList * list;

	if(join_service_is_open)
	{
		return;
	}
	join_service_is_open = 1;

	join_chat_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	gtk_widget_realize(join_chat_window);

	vbox = gtk_vbox_new(FALSE, 5);

	gtk_container_add(GTK_CONTAINER(join_chat_window), vbox);

	table = gtk_table_new(2, 4, FALSE);

	label = gtk_label_new(_("Chat Room Name: "));
	gtk_table_attach_defaults (GTK_TABLE(table), label, 0, 1, 0, 1);
	gtk_widget_show(label);

	chat_room_name = gtk_entry_new();
	gtk_table_attach_defaults (GTK_TABLE(table), chat_room_name, 1, 2, 0, 1);
	gtk_widget_show(chat_room_name);

	public_chkbtn = gtk_check_button_new_with_label(_("Chatroom is public"));
	gtk_table_attach_defaults (GTK_TABLE(table), public_chkbtn, 1, 2, 1, 2);
	gtk_widget_set_sensitive(public_chkbtn, FALSE);
	gtk_widget_show(public_chkbtn);

	public_list_btn = gtk_button_new_with_label(_("List public chatrooms..."));
	gtk_table_attach_defaults (GTK_TABLE(table), public_list_btn, 1, 2, 2, 3);
	gtk_widget_set_sensitive(public_list_btn, FALSE);
	gtk_widget_show(public_list_btn);

	label = gtk_label_new(_("Local account: "));
	gtk_table_attach_defaults (GTK_TABLE(table), label, 0, 1, 3, 4);
	gtk_widget_show(label);

	chat_room_type = gtk_combo_new();
	list = chat_service_list();
	list = g_list_prepend(list, strdup(""));
	gtk_combo_set_popdown_strings(GTK_COMBO(chat_room_type), list);
	g_list_free(list);
	gtk_table_attach_defaults (GTK_TABLE(table), chat_room_type, 1, 2, 3, 4);
	gtk_widget_show(chat_room_type);

	/* set up a frame (hopefully) */

	frame = gtk_frame_new(NULL);
	gtk_frame_set_label(GTK_FRAME(frame), _("Join a Chat Room"));

	gtk_container_add(GTK_CONTAINER(frame), table);
	gtk_widget_show(table);

	gtk_table_set_row_spacings (GTK_TABLE(table), 5);
	gtk_table_set_col_spacings (GTK_TABLE(table), 5);
	
	/* This is a test to see if I can make some padding action happen */

	gtk_container_set_border_width(GTK_CONTAINER(join_chat_window), 5);
	gtk_container_set_border_width(GTK_CONTAINER(table), 5);
	
	gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 5);
	
	/* Window resize BAD, Same size GOOD */

	gtk_window_set_policy (GTK_WINDOW(join_chat_window), FALSE, FALSE, TRUE);
	gtk_window_set_position(GTK_WINDOW(join_chat_window), GTK_WIN_POS_MOUSE);
	
	/* Show the frame and window */
	
	gtk_widget_show(frame);
	
	/* add in that nice separator */
	
	separator = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), separator, FALSE, FALSE, 5);
	gtk_widget_show(separator);
	
	/* Add in the pretty buttons with pixmaps on them */

	hbox2 = gtk_hbox_new(TRUE, 5);

	gtk_widget_set_usize(hbox2, 200, 25);
	
	/* stuff for the join button */
	
	button = gtkut_create_icon_button( _("Join"), ok_xpm, join_chat_window );

	gtk_signal_connect(GTK_OBJECT(button), "clicked",
					GTK_SIGNAL_FUNC(join_chat_callback),
					NULL);

	gtk_signal_connect(GTK_OBJECT(public_list_btn), "clicked",
					GTK_SIGNAL_FUNC(list_public_chatrooms),
					NULL);

	gtk_signal_connect(GTK_OBJECT(GTK_COMBO(chat_room_type)->list), "selection-changed",
					GTK_SIGNAL_FUNC(update_public_sensitivity),
					NULL);

	gtk_box_pack_start(GTK_BOX(hbox2), button, TRUE, TRUE, 0);
	gtk_widget_show(button);
	
	/* stuff for the cancel button */

	button = gtkut_create_icon_button( _("Cancel"), cancel_xpm, join_chat_window );

	gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
						GTK_SIGNAL_FUNC(gtk_widget_destroy),
						GTK_OBJECT(join_chat_window));
	gtk_box_pack_start(GTK_BOX(hbox2), button, TRUE, TRUE, 0);
	gtk_widget_show(button);

	/* now put the hbox with all the buttons into the vbox? */

	hbox = gtk_hbox_new(FALSE, 0);
	
	gtk_box_pack_end(GTK_BOX(hbox), hbox2, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	gtk_widget_show(hbox);
	gtk_widget_show(hbox2);
	gtk_widget_show(vbox);
	
	/* show the window */

	gtk_widget_show(join_chat_window);

	gtk_widget_grab_focus(chat_room_name);
	
	gtk_signal_connect( GTK_OBJECT(join_chat_window), "destroy",
				GTK_SIGNAL_FUNC(join_chat_destroy), NULL );
}

void eb_destroy_chat_room (eb_chat_room *room) 
{
	gtk_widget_destroy(room->window);
}

static void destroy(GtkWidget * widget, gpointer data)
{
	eb_chat_room * ecr = data;

	if ( ecr == NULL )
		return;

	while (l_list_find(chat_rooms, data)) {
		chat_rooms = l_list_remove(chat_rooms, data);
	}
	RUN_SERVICE(ecr->local_user)->leave_chat_room(ecr);
		
	free_chat_room( ecr );
}

static LList * find_chat_room_buddy( eb_chat_room * room, gchar * user )
{
	LList * node;
	for( node = room->fellows; node; node = node->next)
	{
		eb_chat_room_buddy * ecrb = node->data;
		if( !strcmp(user, ecrb->handle) )
		{
			return node;
		}
	}
	return NULL;
}

static void eb_chat_room_refresh_list(eb_chat_room * room )
{
	LList * node;
	gtk_clist_clear(GTK_CLIST(room->fellows_widget));

	for( node = room->fellows; node; node = node->next )
	{
		eb_chat_room_buddy * ecrb = node->data;
		gchar * list[1] = {ecrb->alias};
		gtk_clist_append(GTK_CLIST(room->fellows_widget), list);
	}
}

gboolean eb_chat_room_buddy_connected(eb_chat_room * room, gchar * user)
{
	return find_chat_room_buddy(room, user) != NULL;
}

static void eb_chat_room_private_log_reference(eb_chat_room *room, char *alias, char *handle)
{
	struct contact	*con = find_contact_by_handle(handle);
	const int		buff_size = 1024;
	char			buff[buff_size];
	log_file		*log = NULL;
	int				err = 0;
	
	
	if ( con == NULL )
		return;
	
	if ( !strcmp( handle, room->local_user->handle ) )
		return;

	make_safe_filename( buff, con->nick, con->group->name );
	log = ay_log_file_create( buff );

	err = ay_log_file_open( log, "a" );
		
	if ( err != 0 )
	{
		eb_debug( DBG_CORE,"eb_chat_room_private_log_reference: could not open log file [%s]\n", buff );
		ay_log_file_destroy( &log );
		return;
	}
		
	memset( &buff, 0, buff_size );
	if ( room->logfile == NULL )
		init_chat_room_log_file( room );
	
	g_snprintf( buff, buff_size, "You had a <a href=\"log://%s\">group chat with %s (%s)</a>.\n",
			room->logfile->filename,
			alias, handle );
	
	ay_log_file_message( log, "", buff );
	ay_log_file_destroy( &log );
}

void eb_chat_room_buddy_arrive( eb_chat_room * room, gchar * alias, gchar * handle )
{
	eb_chat_room_buddy * ecrb = NULL;
        gchar *buf;
	LList *t;
	ecrb = g_new0(eb_chat_room_buddy, 1 );
	strncpy( ecrb->alias, alias, sizeof(ecrb->alias));
	strncpy( ecrb->handle, handle, sizeof(ecrb->handle));
	room->total_arrivals++;
	ecrb->color = room->total_arrivals % nb_cr_colors;
	
	for (t = room->fellows; t && t->data; t = t->next) {
		if(!strcasecmp(handle, ((eb_chat_room_buddy *)t->data)->handle))
			return;
	}

        buf = g_strdup_printf(_("<body bgcolor=#F9E589 width=*><b> %s (%s) has joined the chat.</b></body>"), alias, handle);
        eb_chat_room_show_3rdperson(room, buf);
	
	eb_chat_room_private_log_reference(room, alias, handle);
	
	g_free(buf);

	room->fellows = l_list_append(room->fellows, ecrb);

	eb_chat_room_refresh_list(room);
}

void eb_chat_room_buddy_leave( eb_chat_room * room, gchar * handle )
{
	LList * node = find_chat_room_buddy(room, handle);

        gchar *buf;
	if (node) {
		eb_chat_room_buddy * ecrb = node->data;
	        buf = g_strdup_printf(_("<body bgcolor=#F9E589 width=*><b> %s (%s) has left the chat.</b></body>"), ecrb->alias, handle);
	} else
		buf = g_strdup_printf(_("<body bgcolor=#F9E589 width=*><b> %s has left the chat.</b></body>"), handle);
        eb_chat_room_show_3rdperson(room, buf);
	g_free(buf);

	if(node && room->fellows)
	{
		eb_chat_room_buddy * ecrb = node->data;
		eb_account *ea = find_account_by_handle(ecrb->handle, room->local_user->service_id);
		if(ea)
			eb_chat_room_display_status (ea, NULL);
		room->fellows = l_list_remove(room->fellows, ecrb);
		g_free(ecrb);
	}
	eb_chat_room_refresh_list(room);
}

static void handle_focus(GtkWidget *widget, GdkEventFocus * event, gpointer userdata)
{
	eb_chat_room * cr = (eb_chat_room *)userdata;
	eb_chat_room_update_window_title(cr, FALSE);

}

static void eb_chat_room_update_window_title(eb_chat_room *ecb, gboolean new_message)
{
	char *room_title;
	room_title = g_strdup_printf("%s%s [%s]", 
			new_message?"* ":"",
			ecb->room_name, 
			get_service_name(ecb->local_user->service_id));
	gtk_window_set_title(GTK_WINDOW(ecb->window), room_title);
	g_free(room_title);
}

void eb_start_chat_room( eb_local_account *ela, gchar * name, int is_public )
{
	eb_chat_room * ecb = NULL;

	/* if we don't have a working account just bail right now */

	if( !ela )
		return;

	ecb = RUN_SERVICE(ela)->make_chat_room(name, ela, is_public);

	if( ecb )
	{
		if (!l_list_find(chat_rooms,ecb)) {
			chat_rooms = l_list_append(chat_rooms, ecb);
		}
		eb_chat_room_update_window_title(ecb, FALSE);
	}
}

void eb_chat_room_show_3rdperson( eb_chat_room * chat_room, gchar * message)
{
	gtk_eb_html_add(EXT_GTK_TEXT(chat_room->chat), message,0,0,0 );
	gtk_eb_html_add(EXT_GTK_TEXT(chat_room->chat), "\n",0,0,0 );
	ay_log_file_message( chat_room->logfile, "", message );
}

void eb_chat_room_show_message( eb_chat_room * chat_room,
				gchar * user, gchar * message )
{
	gchar buff[2048];
	gchar *temp_message, *link_message;

	if (!message || strlen(message) == 0)
		return;

	if(!strcmp(chat_room->local_user->handle, user))
	{
		time_t t;
		struct tm * cur_time;
		char *color;
		
		color = "#0000ff";
	
		time(&t);
		cur_time = localtime(&t);
		if (iGetLocalPref("do_convo_timestamp")) {
			g_snprintf(buff, 2048, "<B><FONT COLOR=\"%s\">%d:%.2d:%.2d</FONT> <FONT COLOR=\"%s\">%s: </FONT></B>",
			 color, 
			 cur_time->tm_hour, cur_time->tm_min,
			 cur_time->tm_sec, color, chat_room->local_user->alias);
		} else {
			g_snprintf(buff, 2048, "<FONT COLOR=\"%s\"><B>%s: </B></FONT>", color, chat_room->local_user->alias);
		}
	}
	else
	{
		LList *walk;
		eb_account *acc = NULL;
		gchar * color = NULL;
		time_t t;
		struct tm * cur_time;
		
		if (RUN_SERVICE(chat_room->local_user)->get_color)
			color = RUN_SERVICE(chat_room->local_user)->get_color();
		else 
			color = "#ff0000";

		time(&t);
		cur_time = localtime(&t);
		if (iGetLocalPref("do_convo_timestamp")) {
			g_snprintf(buff, 2048, "<B><FONT COLOR=\"%s\">%d:%.2d:%.2d</FONT> <FONT COLOR=\"%s\">%s: </FONT></B>",
			 color,cur_time->tm_hour, cur_time->tm_min,
			 cur_time->tm_sec, color, user);
		} else {
			g_snprintf(buff, 2048, "<FONT COLOR=\"%s\"><B>%s: </B></FONT>", color, user);
		}
		for (walk = chat_room->typing_fellows; walk && walk->data; walk = walk->next) {
			acc = walk->data;
			if (!strcasecmp(acc->handle, user)
			||  !strcasecmp(acc->account_contact->nick, user))
				break;
			else
				acc = NULL;
		}
		if (acc && chat_room->typing_fellows) {
			chat_room->typing_fellows = l_list_remove(chat_room->typing_fellows,acc);
			eb_chat_room_display_status (acc, NULL);
		}
		eb_chat_room_update_window_title(chat_room, TRUE);
		if (iGetLocalPref("do_raise_window"))
			gdk_window_raise(chat_room->window->window);
		 
	}
	if(RUN_SERVICE(chat_room->local_user)->get_smileys)
	 	temp_message = eb_smilify(message, RUN_SERVICE(chat_room->local_user)->get_smileys());
	else
		temp_message = g_strdup(message);
	
	link_message = linkify(temp_message);

	g_free(temp_message);

#ifdef __MINGW32__
	char *recoded = ay_str_to_utf8(link_message);
	g_free(link_message);
	link_message = recoded;
#endif
	
	gtk_eb_html_add(EXT_GTK_TEXT(chat_room->chat), buff,0,0,0 );
	gtk_eb_html_add(EXT_GTK_TEXT(chat_room->chat), link_message,
		iGetLocalPref("do_ignore_back"), iGetLocalPref("do_ignore_fore"), iGetLocalPref("do_ignore_font"));
	gtk_eb_html_add(EXT_GTK_TEXT(chat_room->chat), "\n",0,0,0 );
	
	ay_log_file_message( chat_room->logfile, buff, link_message );
	
	g_free(link_message);
	
	if(chat_room->sound_enabled 
	&& strcmp(chat_room->local_user->handle, user))
	{   
		if (chat_room->receive_enabled)
			play_sound( SOUND_RECEIVE );
	}	
}

static void invite_button_callback( GtkWidget * widget, gpointer data )
{
	do_invite_window(data);
}

static void destroy_chat_window(GtkWidget * widget, gpointer data)
{
	eb_chat_room *ecr = (eb_chat_room *)data;
	gtk_widget_destroy(ecr->window);
}

static void	destroy_smiley_cb_data(GtkWidget *widget, gpointer data)
{
	smiley_callback_data *scd = data;
	if ( !data )
		return;

	if(scd->c_room->smiley_window ) {
		gtk_widget_destroy(scd->c_room->smiley_window);
		scd->c_room->smiley_window = NULL;
	}
	
	g_free( scd );
}

static void action_callback(GtkWidget *widget, gpointer d)
{
	eb_chat_room * ecr = (eb_chat_room *)d;
	conversation_action( ecr->logfile, TRUE );
}

static void _show_smileys_cb(GtkWidget * widget, smiley_callback_data *data)
{
	show_smileys_cb(data);
}

void eb_join_chat_room( eb_chat_room * chat_room )
{
	GtkWidget * vbox;
	GtkWidget * vbox2;
	GtkWidget * hbox;
	GtkWidget * hbox2;
	GtkWidget * label;
	GtkWidget * scrollwindow = gtk_scrolled_window_new(NULL, NULL);
	GtkWidget * toolbar;
	GdkPixmap * icon;
	GdkBitmap * mask;
	GtkWidget * iconwid;
	GtkWidget * send_button;
	GtkWidget * close_button;
	GtkWidget * print_button;
	GtkWidget * separator;
	GtkWidget * entry_box;
	gboolean    enableSoundButton = FALSE;
	char      * room_title = NULL;
	
	/*if we are already here, just leave*/
	if(chat_room->connected)
		return;
	
	if (!l_list_find(chat_rooms, chat_room)) {
		chat_rooms = l_list_append(chat_rooms, chat_room);
	}
		
	/*otherwise we are going to make ourselves a gui right here*/

	vbox = gtk_vbox_new(FALSE, 1);
	hbox = gtk_hpaned_new();

	gtk_paned_set_gutter_size(GTK_PANED(hbox), 20);
	
	chat_room->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	chat_room->fellows_widget = gtk_clist_new(1);
	gtk_clist_set_column_title(GTK_CLIST(chat_room->fellows_widget), 0, _("Online"));
	gtk_widget_set_usize(chat_room->fellows_widget, 100, 20 );
	chat_room->chat = ext_gtk_text_new(NULL, NULL);
	gtk_widget_set_usize(chat_room->chat, 400, 200);
	gtk_eb_html_init(EXT_GTK_TEXT(chat_room->chat));
	gtk_container_add(GTK_CONTAINER(scrollwindow), chat_room->chat);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwindow), 
								   GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);

	gtk_signal_connect(GTK_OBJECT(chat_room->fellows_widget), "select_row",
					GTK_SIGNAL_FUNC(fellows_click), chat_room );
	/*make ourselves something to type into*/
	entry_box = gtk_vbox_new(FALSE,0);
	if ( iGetLocalPref("do_multi_line") ) {
		GtkWidget *scrollwindow2 = gtk_scrolled_window_new(NULL, NULL);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (scrollwindow2), 
					     GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
		
		chat_room->entry = gtk_text_new(NULL, NULL);

		gtk_widget_set_usize(chat_room->entry, -1, 50);

		gtk_text_set_editable(GTK_TEXT(chat_room->entry), TRUE);
		gtk_text_set_word_wrap(GTK_TEXT(chat_room->entry), TRUE);
		gtk_text_set_line_wrap(GTK_TEXT(chat_room->entry), TRUE);

#ifdef HAVE_LIBPSPELL
		if( iGetLocalPref("do_spell_checking") )
			gtkspell_attach(GTK_TEXT(chat_room->entry));
#endif

		gtk_container_add(GTK_CONTAINER(scrollwindow2),chat_room->entry);
		gtk_widget_show(scrollwindow2);
		gtk_box_pack_start(GTK_BOX(entry_box), scrollwindow2, TRUE, TRUE, 3);
	} else {
		chat_room->entry = gtk_entry_new();

		gtk_signal_connect(GTK_OBJECT(chat_room->entry), "activate",
						GTK_SIGNAL_FUNC(send_cr_message), chat_room );
		gtk_box_pack_start(GTK_BOX(entry_box), chat_room->entry, TRUE, TRUE, 3);
	}
	
	gtk_signal_connect(GTK_OBJECT(chat_room->entry), "key_press_event",
			 GTK_SIGNAL_FUNC(cr_key_press),
			 chat_room);
	
	gtk_paned_add1(GTK_PANED(hbox), scrollwindow);
	gtk_widget_show(scrollwindow);

	vbox2 = gtk_vbox_new(FALSE, 5);

	scrollwindow = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwindow), 
								   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	gtk_container_add(GTK_CONTAINER(scrollwindow), chat_room->fellows_widget );
	gtk_widget_show(chat_room->fellows_widget);
	

	gtk_box_pack_start(GTK_BOX(vbox2), scrollwindow, TRUE, TRUE, 3 );
	gtk_widget_show(scrollwindow);

	label = gtk_button_new_with_label(_("Invite User"));
	gtk_signal_connect(GTK_OBJECT(label), "clicked", invite_button_callback, 
						chat_room);

	gtk_box_pack_start(GTK_BOX(vbox2), label, FALSE, FALSE, 3);
	gtk_widget_show(label);

	gtk_paned_add2(GTK_PANED(hbox), vbox2);
	gtk_widget_show(vbox2);


	gtk_paned_add2(GTK_PANED(hbox), chat_room->fellows_widget);
	gtk_widget_show(chat_room->chat);

	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwindow),GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE,TRUE, 3);
	gtk_box_pack_start(GTK_BOX(vbox), entry_box, FALSE, FALSE, 1);
	gtk_widget_show(hbox);
	gtk_widget_show(entry_box);
	gtk_widget_show(chat_room->entry);

	gtk_widget_realize(chat_room->window);
	if (chat_room->local_user)
		room_title = g_strdup_printf("%s [%s]", chat_room->room_name, 
					get_service_name(chat_room->local_user->service_id));
	else
		room_title = g_strdup_printf("%s", chat_room->room_name);
	
	gtk_window_set_title(GTK_WINDOW(chat_room->window), room_title);
	g_free(room_title);
	gtkut_set_window_icon(chat_room->window->window, NULL);
	gtk_signal_connect(GTK_OBJECT(chat_room->window), "focus_in_event",
					   GTK_SIGNAL_FUNC(handle_focus), chat_room);

	
	/* start a toolbar here */
	
	hbox2 = gtk_hbox_new(FALSE, 0);
	gtk_widget_set_usize(hbox2, 200, 25);
	
	toolbar = gtk_toolbar_new(GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_ICONS);
	gtk_toolbar_set_button_relief(GTK_TOOLBAR(toolbar), GTK_RELIEF_NONE);
	gtk_container_set_border_width(GTK_CONTAINER(toolbar), 0);
	gtk_toolbar_set_space_size(GTK_TOOLBAR(toolbar), 5);
	
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
	icon = gdk_pixmap_create_from_xpm_d(chat_room->window->window, &mask, NULL, xpm); \
	iconwid = gtk_pixmap_new(icon, mask); \
	gtk_widget_show(iconwid); }

	/* smileys */
	if ( iGetLocalPref("do_smiley") ) {
		smiley_callback_data * scd = g_new0(smiley_callback_data,1);
		scd->c_room = chat_room;
		scd->c_window = NULL;
		ICON_CREATE(icon, iconwid, smiley_button_xpm);
		chat_room->smiley_button = 
			TOOLBAR_APPEND(_("Insert Smiley"), iconwid, _show_smileys_cb, scd);
	
		gtk_signal_connect(GTK_OBJECT(chat_room->smiley_button), "destroy",
					   GTK_SIGNAL_FUNC(destroy_smiley_cb_data), scd);
		/*Create the separator for the toolbar*/

		TOOLBAR_SEPARATOR();
	}

	ICON_CREATE(icon, iconwid, tb_volume_xpm);
	
  	chat_room->sound_button = gtk_toolbar_append_element(GTK_TOOLBAR(toolbar),
						GTK_TOOLBAR_CHILD_TOGGLEBUTTON,
						NULL,
						_("Sound"),
						_("Enable Sounds"),
						_("Sound"),
						iconwid,
						GTK_SIGNAL_FUNC(set_sound_on_toggle),
						chat_room);
  /*Toggle the sound button based on preferences*/

	/* Toggle the sound button based on preferences */
	if ( iGetLocalPref("do_play_send") )
	{
		chat_room->send_enabled = TRUE;
		enableSoundButton = TRUE;
	}

	if ( iGetLocalPref("do_play_receive") )
	{
		chat_room->receive_enabled = TRUE;
		enableSoundButton = TRUE;
	}

	if ( iGetLocalPref("do_play_first") )
	{
		chat_room->first_enabled = TRUE;
		enableSoundButton = TRUE;
	}

	gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON( chat_room->sound_button ), 
				     enableSoundButton );

	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar)); 

	ICON_CREATE(icon, iconwid, action_xpm);
	print_button = TOOLBAR_APPEND(_("Actions..."), iconwid, action_callback, chat_room);

	TOOLBAR_SEPARATOR();

	ICON_CREATE(icon, iconwid, tb_mail_send_xpm);
	send_button = TOOLBAR_APPEND(_("Send Message"), iconwid, send_cr_message, chat_room);
	
	ICON_CREATE(icon, iconwid, cancel_xpm);

	TOOLBAR_SEPARATOR();
	
	close_button = TOOLBAR_APPEND(_("Close"), iconwid, destroy_chat_window, chat_room);
	
	chat_room->status_label = gtk_label_new(" ");
	gtk_box_pack_start(GTK_BOX(hbox2), chat_room->status_label, FALSE, FALSE, 0);
	gtk_widget_show(chat_room->status_label);
	chat_room->typing_fellows = NULL;
	
	gtk_box_pack_end(GTK_BOX(hbox2), toolbar, FALSE, FALSE, 0);
	gtk_widget_show(toolbar);
	
	gtk_box_pack_start(GTK_BOX(vbox), hbox2, FALSE, FALSE, 5);
	gtk_widget_show(hbox2);
	
	gtk_container_add(GTK_CONTAINER(chat_room->window), vbox);
	gtk_widget_show(vbox);

	gtk_container_set_border_width(GTK_CONTAINER(chat_room->window), 5);
	
	gtk_signal_connect(GTK_OBJECT(chat_room->window), "destroy",
					   GTK_SIGNAL_FUNC(destroy), chat_room );
	gtk_widget_show(chat_room->window);

	/*then mark the fact that we have joined that room*/
	chat_room->connected = 1;

#undef TOOLBAR_APPEND
#undef ICON_CREATE
#undef TOOLBAR_SEPARATOR
	
	/* actually call the callback :P .... */
	if (!chat_room) {
		eb_debug(DBG_CORE,"!chat_room\n");
		return;
	}
	if (!chat_room->local_user) {
		eb_debug(DBG_CORE,"!chat_room->account\n");
		return;
	}
	if (RUN_SERVICE(chat_room->local_user) == NULL) {
		eb_debug(DBG_CORE,"!RUN_SERVICE(chat_room->local_user)\n");
		return;
	}
	if (RUN_SERVICE(chat_room->local_user)->join_chat_room == NULL) {
		eb_debug(DBG_CORE,"!RUN_SERVICE(chat_room->local_user)->join_chat_room\n");
		return;
	}
		
	init_chat_room_log_file(chat_room);
	RUN_SERVICE(chat_room->local_user)->join_chat_room(chat_room);
	gtk_widget_grab_focus(chat_room->entry);
}

static void init_chat_room_log_file( eb_chat_room *chat_room )
{
	assert( chat_room != NULL );
	
	if ( chat_room->logfile == NULL )
	{
		time_t      mytime = time(NULL);
		char        buff[2048];
		char	    tmpnam[128];
		
		g_snprintf( tmpnam, 128, "cr_log%lu%d", mytime, total_rooms );
		make_safe_filename( buff, tmpnam, NULL );
		
		chat_room->logfile = ay_log_file_create( buff );
		
		ay_log_file_open( chat_room->logfile, "a" );
	}
}

static void	destroy_chat_room_log_file( eb_chat_room *chat_room )
{
	if ( (chat_room == NULL) || (chat_room->logfile == NULL) )
		return;

	ay_log_file_destroy( &(chat_room->logfile) );
}

static LList * get_group_contacts(gchar *group, eb_chat_room * room)
{
	LList *node = NULL, *newlist = NULL, *accounts = NULL;
	grouplist *g;
	
	g = find_grouplist_by_name(group);

	if(g)
		node = g->members;
	
	while(node)
	{
		struct contact * contact = (struct contact *)node->data;
		accounts = contact->accounts;
		while (accounts) {
			if( ((struct account *)accounts->data)->ela == room->local_user
			&&  ((struct account *)accounts->data)->online) {
				char *buf = g_strdup_printf("%s (%s)", contact->nick, 
						((struct account *)accounts->data)->handle);
				newlist = l_list_append(newlist, buf);	
			}
			accounts = accounts->next;
		}
		node = node->next;
	}
	
	return newlist;
}

static LList * get_contacts(eb_chat_room * room)
{
	LList *node = groups;
	LList *newlist = NULL;
	while(node)
	{
		LList * g = get_group_contacts(node->data, room);
		newlist = l_list_concat(newlist, g);	
		node = node->next;
	}
	return newlist;
}

void eb_chat_room_display_status (eb_account *remote, char *message)
{
	LList *rooms = NULL;
	if (!iGetLocalPref("do_typing_notify"))
		return;
	
	rooms = find_chatrooms_with_remote_account(remote);
	for (; rooms && rooms->data; rooms = rooms->next) {

		eb_chat_room *ecr = rooms->data;
		gchar *tmp = NULL;
		gchar *typing = NULL, *typing_old = NULL;
		LList *walk2 = NULL;

		if (message && !strcmp(_("typing..."), message)) {
			if (!ecr->typing_fellows || !l_list_find(ecr->typing_fellows, remote))
				ecr->typing_fellows = l_list_append(ecr->typing_fellows, remote);
		} else if (ecr->typing_fellows) {
			ecr->typing_fellows = l_list_remove(ecr->typing_fellows, remote);
		}
		
		for (walk2 = ecr->typing_fellows; walk2 && walk2->data; walk2 = walk2->next) {
			eb_account *acc = walk2->data;
			typing_old = typing;
			typing = g_strdup_printf("%s%s%s", 
					acc->account_contact->nick,
					typing_old?", ":"",
					typing_old?typing_old:"");
			g_free(typing_old);
		}
		
		if (typing != NULL && strlen(typing) > 0)
			tmp = g_strdup_printf("%s: %s", typing, _("typing..."));
		else
			tmp = g_strdup_printf(" ");

		if (ecr && ecr->status_label)
			gtk_label_set_text( GTK_LABEL(ecr->status_label), tmp );
		g_free(tmp);
	}
}
