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
 * status.c
 * code for making the status changing widgets
 *
 */

#include "intl.h"

#include <string.h>
#include <assert.h>
#include <stdlib.h>

#ifndef __MINGW32__
#include <X11/Xlib.h>
#endif

#include "service.h"
#include "util.h"
#include "globals.h"
#include "status.h"
#include "chat_window.h"
#include "help_menu.h"
#include "add_contact_window.h"
#include "dialog.h"
#include "away_window.h"
#include "message_parse.h"
#include "contact_actions.h"
#include "sound.h"
#include "plugin.h"
#include "plugin_api.h"
#include "prefs.h"
#include "offline_queue_mgmt.h"
#include "about.h"
#include "edit_local_accounts.h"

#include "gtk/gtkutils.h"

#include "pixmaps/login_icon.xpm"
#include "pixmaps/blank_icon.xpm"
#include "pixmaps/logoff_icon.xpm"


/* define to add debug stuff to 'Help' Menu */
/*
#define	ADD_DEBUG_TO_MENU	1
*/

void update_contact_list();

enum {
	TARGET_STRING,
	TARGET_ROOTWIN,
	TARGET_URL
};

struct acctStatus
{
	eb_local_account * ela;
	gint status;
};


/* globals - referenced in gtk_globals.h */
GtkWidget *statuswindow = NULL;
GtkWidget *away_menu = NULL;

static int window_title_handler = -1;

static GtkTooltips * status_tips=NULL;

static GtkWidget * contact_list = NULL;
static GtkWidget * contact_window = NULL;
static GtkWidget * status_bar = 0;
static GtkWidget * status_message = NULL;

/* status_show is:
   0 => show all accounts,
   1 => show all contacts,
   2 => show online contacts
*/
static int status_show = -1;

static time_t last_sound_played = 0;
static void eb_save_size( GtkWidget * widget, gpointer data );

void set_tooltips_active(int active)
{
	if(active)
		gtk_tooltips_enable(status_tips);
	else
		gtk_tooltips_disable(status_tips);
}

void focus_statuswindow (void) 
{
	gdk_window_raise(statuswindow->window);
}

static void delete_event( GtkWidget *widget,
				   GdkEvent *event,
				   gpointer data )
{
	gchar *userrc = NULL;
	
	eb_debug(DBG_CORE, "Signing out...\n");
	eb_sign_off_all();	
	eb_debug(DBG_CORE, "Signed out\n");
	
	eb_save_size(contact_window, NULL);

	userrc = g_strconcat(config_dir, G_DIR_SEPARATOR_S, "menurc", NULL);
	gtk_item_factory_dump_rc(userrc, NULL, TRUE);
	g_free(userrc);
	gtk_main_quit();
}


static void send_file_callback(GtkWidget * w, eb_account * ea )
{
	eb_do_send_file(ea);
}

static void send_file_with_contact_callback(GtkWidget * w, struct contact * conn )
{
	eb_account * ea = find_suitable_remote_account( NULL, conn );
	if (ea)
		eb_do_send_file(ea);
}

/*** MIZHI
 * For displaying the logs
 */
static void view_log_callback(GtkWidget * w, struct contact * conn)
{  
	eb_view_log(conn);
}

static void edit_trigger_callback(GtkWidget * w, struct contact * conn)
{
	show_trigger_window(conn);
}


static void edit_group_callback(GtkWidget * w, grouplist * g)
{
	edit_group_window_new(g);
}

static void sort_group_callback(GtkWidget *w, grouplist *g)
{
	LList *l=g->members, *sorted=NULL;

	while(l) {
		sorted = l_list_insert_sorted(sorted, l->data, (LListCompFunc)contact_cmp);
		l=l_list_remove_link(l, l);
	}

	g->members = sorted;
	update_contact_list();
}

static void add_to_group_callback(GtkWidget *widget, grouplist *g)
{
	show_add_contact_to_group_window(g);
}

static void edit_contact_callback(GtkWidget * w, struct contact * conn)
{
	edit_contact_window_new(conn);
}

static void add_account_to_contact_callback(GtkWidget *w, struct contact *conn)
{
	show_add_account_to_contact_window(conn);
}

static void edit_account_callback(GtkWidget * w, gpointer a)
{
	edit_account_window_new(a);
}


static void remove_group_callback(GtkWidget *w, gpointer d)
{
	if (gtk_object_get_user_data (GTK_OBJECT(w)) != 0) {
		grouplist * g = (grouplist *) d;

		eb_debug(DBG_CORE, "Delete Group\n");
		remove_group(g);

		update_contact_list ();
		write_contact_list();
	}
}

static void offer_remove_group_callback(GtkWidget *w, gpointer d)
{
	if (d) {
		grouplist * g = (grouplist *) d;
		char buff [1024];

		snprintf(buff, sizeof(buff), _("Do you really want to delete group \"%s\"?"), g->name);
		do_dialog (buff, _("Delete Group"), remove_group_callback, d);
	}
}

static void remove_contact_callback(GtkWidget *w, gpointer d)
{
	if (gtk_object_get_user_data (GTK_OBJECT(w)) != 0) {
		struct contact * conn = (struct contact *) d;

		eb_debug(DBG_CORE, "Delete Contact\n");
		remove_contact (conn);

		update_contact_list ();
		write_contact_list();
	}
}

static void offer_remove_contact_callback(GtkWidget *w, gpointer d)
{
	if (d) {
		struct contact * c = (struct contact *) d;
		char buff [1024];
	
		snprintf(buff, sizeof(buff), _("Do you really want to delete contact \"%s\"?"), c->nick);
		do_dialog (buff, _("Delete Contact"), remove_contact_callback, d);
	}
}

static void remove_account_callback(GtkWidget *w, gpointer d)
{
	if (gtk_object_get_user_data (GTK_OBJECT(w)) != 0) {
		eb_debug(DBG_CORE, "Delete Account\n");
		remove_account(d);

		update_contact_list ();
		write_contact_list();
	}
}

static void offer_remove_account_callback(GtkWidget *w, gpointer d)
{
	if (d) {
		struct account * a = (struct account *) d;
		char buff [1024];

		snprintf(buff, sizeof(buff), _("Do you really want to delete account \"%s\"?"), a->handle);
		do_dialog (buff, _("Delete Account"), remove_account_callback, d);
	}
}

static void eb_save_size( GtkWidget * widget, gpointer data ) 
{
	int h = widget->allocation.height;
	int w = widget->allocation.width;
	int x, y;
	
	if (h<10 || w < 10) 
		return;
		
	iSetLocalPref("length_contact_window",h);
	iSetLocalPref("width_contact_window",w);

	gtkut_widget_get_uposition(statuswindow, &x, &y);
	iSetLocalPref("x_contact_window",x);
	iSetLocalPref("y_contact_window",y);
	iSetLocalPref("status_show_level",status_show);
	eb_debug(DBG_CORE,"saving window size\n");
	
	ayttm_prefs_write();
}

static void get_info(GtkWidget * w, eb_account *ea )
{
	eb_local_account * ela;

	if(ea && RUN_SERVICE(ea)->get_info) {
		ela = find_suitable_local_account_for_remote(ea, NULL);
		if (ela)
			RUN_SERVICE(ea)->get_info(ela ,ea);  
		else
			eb_debug(DBG_CORE, "Couldn't find suitable local account for info request");
	} else {
		eb_debug(DBG_CORE, "ea is null or get_info not supported\n");
	}  
}

/* FIXME: use gtk_label_set_text() */
static void update_status_message(gchar * message )
{
	if(status_bar) {
		gtk_widget_destroy(status_message);
		status_message = gtk_label_new(message);
		gtk_container_add(GTK_CONTAINER(status_bar), status_message);
		gtk_widget_show(status_message);
	}
}


static void collapse_contact( GtkTreeItem * treeItem, gpointer data )
{
	struct contact * c = data;
	c->expanded = FALSE;
}

static void expand_contact( GtkTreeItem * treeItem, gpointer data )
{
	struct contact * c = data;
	c->expanded = TRUE;
}

static void status_show_callback(GtkWidget *w, gpointer data)
{
	status_show = (int)data;
	update_contact_list(); 
	eb_save_size(contact_window, NULL);
}

static GtkWidget *make_info_menu(struct contact *c, int *nb)
{
	LList *iterator;
	GtkWidget *InfoMenu = gtk_menu_new();
	GtkWidget *button;
	char *buff = NULL;
	
	for(iterator=c->accounts; iterator; iterator=iterator->next) {
		eb_account * account = (eb_account*)iterator->data;
		
		if(account->online && RUN_SERVICE(account)->get_info) {
			buff = g_strdup_printf("%s [%s]", 
					account->handle, 
					get_service_name(account->service_id));	    
			button = gtk_menu_item_new_with_label(buff);
			g_free(buff);
			gtk_signal_connect(GTK_OBJECT(button), "activate", 
					GTK_SIGNAL_FUNC(get_info),account);
			gtk_menu_append(GTK_MENU(InfoMenu), button);
			gtk_widget_show(button);
			(*nb)++;
		}
	}
	return InfoMenu;
}

/*
 * Menus raised by buttons
 */
    
static void group_menu(GdkEventButton * event, gpointer d )
{
	GtkWidget *menu;
	grouplist *grp = (grouplist *)d;
	menu = gtk_menu_new();
	
	if (contact_list && grp->list_item)
		gtk_tree_select_child(GTK_TREE(contact_list), grp->list_item);
 
	gtkut_create_menu_button (GTK_MENU(menu), _("Add contact to group..."),
			GTK_SIGNAL_FUNC(add_to_group_callback), d);

	gtkut_create_menu_button (GTK_MENU(menu), NULL, NULL, NULL);

	gtkut_create_menu_button (GTK_MENU(menu), _("Edit Group..."),
			GTK_SIGNAL_FUNC(edit_group_callback), d);

	gtkut_create_menu_button (GTK_MENU(menu), _("Delete Group..."),
			GTK_SIGNAL_FUNC(offer_remove_group_callback), d);
	
	gtkut_create_menu_button (GTK_MENU(menu), _("Sort"),
			GTK_SIGNAL_FUNC(sort_group_callback), grp);
	
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
		 	event->button, event->time );
}

int add_menu_items(void *vmenu, int cur_service, int should_sep,
			struct contact *conn, eb_account *acc, eb_local_account *ela)
{
	GtkWidget *menu = (GtkWidget *)vmenu;
	GtkWidget *submenu = menu, *button;
	menu_data *md=NULL;
	menu_item_data *mid=NULL;
	ebmContactData *ecd=NULL;
	LList *list=NULL;
	int next_sep=0;
	int last=-2;
	eb_local_account *l_ela = ela;
	
	if (!l_ela && acc) 
		l_ela = acc->ela;

	md = GetPref(EB_CONTACT_MENU);
	for(list = md->menu_items; list; list  = list->next ) {
		ecd=ebmContactData_new();
		if (conn)
			ecd->contact=conn->nick;
		else if (acc) {
			ecd->contact=acc->account_contact->nick;
			ecd->remote_account=acc->handle;
		}
		mid=(menu_item_data *)list->data;
		mid->data=(ebmCallbackData *)ecd;
		if (mid->protocol != NULL) {
			eb_local_account *a = l_ela?l_ela:find_suitable_local_account(NULL, 
					get_service_id(mid->protocol));
			ecd->local_account = (a && a->connected) ? a->handle:NULL;
		}

		if ( (mid->protocol == NULL 
			&& cur_service==-1)
		||   (mid->protocol != NULL 
			&& cur_service == get_service_id(mid->protocol) 
			&& find_account_for_protocol(conn?conn:acc->account_contact, 
							get_service_id(mid->protocol)))
		) {
			if(last != cur_service) {
				if(should_sep)
					gtkut_create_menu_button (GTK_MENU(menu), NULL, NULL, NULL);
				if (!acc && mid->protocol) {
					char *title=g_strdup_printf(_("%s options"),mid->protocol);
					submenu = gtk_menu_new();
					gtkut_attach_submenu(GTK_MENU(menu), title, submenu,
							 ecd->local_account!=NULL);
					gtk_widget_show(submenu);
					g_free(title);
				} else {
					submenu = menu;
				}
			}				
			eb_debug(DBG_CORE, "adding contact menu item: %s (%s)\n", mid->label, mid->protocol);
			eb_debug(DBG_CORE, "ecd->contact:%s, ecd->ra=%s, ecd->la=%s\n",ecd->contact,ecd->remote_account,ecd->local_account);
			button = gtk_menu_item_new_with_label(mid->label);
			gtk_widget_set_sensitive(button, (mid->protocol==NULL || ecd->local_account!=NULL));
			gtk_menu_append(GTK_MENU(submenu), button);
			gtk_signal_connect(GTK_OBJECT(button), "activate",
			eb_generic_menu_function, mid);
			gtk_widget_show(button);
			last=cur_service;
			next_sep=(mid->protocol==NULL);
		}  
	}	
	return next_sep;
}

static void contact_menu(GdkEventButton * event, gpointer d )
{
	struct contact *conn=d;
	GtkWidget *menu = NULL;
	GtkWidget *submenu = NULL;
	menu_data *md=NULL;

	int nbitems = 0;
	int cur_service = -1;
	
	menu = gtk_menu_new();
	
	if (contact_list && conn->list_item)
		gtk_tree_select_child(GTK_TREE(contact_list), conn->list_item);
	
	gtkut_create_menu_button (GTK_MENU(menu), _("Add Account to Contact..."),
			GTK_SIGNAL_FUNC(add_account_to_contact_callback), d);

	gtkut_create_menu_button (GTK_MENU(menu), _("Edit Contact..."),
			GTK_SIGNAL_FUNC(edit_contact_callback), d);

	gtkut_create_menu_button (GTK_MENU(menu), _("Delete Contact..."),
			GTK_SIGNAL_FUNC(offer_remove_contact_callback), d);
	
	gtkut_create_menu_button (GTK_MENU(menu), NULL, NULL, NULL); /* sep */
	
	gtkut_create_menu_button (GTK_MENU(menu), _("Send File..."),
			GTK_SIGNAL_FUNC(send_file_with_contact_callback), d);
	
	gtkut_create_menu_button (GTK_MENU(menu), _("Edit Trigger..."),
			GTK_SIGNAL_FUNC(edit_trigger_callback), d);

	gtkut_create_menu_button (GTK_MENU(menu), NULL, NULL, NULL); /* sep */

	gtkut_create_menu_button (GTK_MENU(menu), _("View Log..."),
			GTK_SIGNAL_FUNC(view_log_callback), d);

	submenu = make_info_menu((struct contact *)d, &nbitems);
	gtkut_attach_submenu (GTK_MENU(menu), _("Info"), submenu, nbitems);

	/*** MIZHI
	 * code for viewing the logs
	 */
	
	gtkut_create_menu_button (GTK_MENU(menu), NULL, NULL, NULL); /* sep */

	md = GetPref(EB_CONTACT_MENU);
	if(md) {
		int should_sep=0;
		for (cur_service=-1; cur_service<0 || strcmp(get_service_name(cur_service),"unknown"); cur_service++) {
			should_sep=(add_menu_items(menu,cur_service, should_sep, conn, NULL,NULL)>0);
		}
	}

	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
			event->button, event->time );
}


static void account_menu(GdkEventButton * event, gpointer d )
{
	GtkWidget * menu = NULL;
	eb_account *acc = (eb_account *)d;
	menu_data *md=NULL;

	
	menu = gtk_menu_new();
	if (contact_list && acc->list_item)
		gtk_tree_select_child(GTK_TREE(contact_list), acc->list_item);

	gtkut_create_menu_button (GTK_MENU(menu), _("Edit Account..."),
			GTK_SIGNAL_FUNC(edit_account_callback), d);

	gtkut_create_menu_button (GTK_MENU(menu), _("Delete Account..."),
			GTK_SIGNAL_FUNC(offer_remove_account_callback), d);

	gtkut_create_menu_button (GTK_MENU(menu), NULL, NULL, NULL); /* sep */

	if (CAN(acc, send_file))
		 gtkut_create_menu_button (GTK_MENU(menu), _("Send File..."),
			GTK_SIGNAL_FUNC(send_file_callback), d);

	gtkut_create_menu_button (GTK_MENU(menu), _("Info..."),
			GTK_SIGNAL_FUNC(get_info),d);

	gtkut_create_menu_button (GTK_MENU(menu), NULL, NULL, NULL); /* sep */

	md = GetPref(EB_CONTACT_MENU);
	if(md) {
		int should_sep=0;
		
		should_sep=(add_menu_items(menu,-1, should_sep, NULL, acc, NULL)>0);
		add_menu_items(menu,acc->service_id, should_sep, NULL, acc, NULL);
	}

	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
		 event->button, event->time );

}

/*
 * End accounts raised by buttons
 */


/*
 * Mouse button event handlers for elements of the group/contact/account list
 */

static void group_click (GtkWidget *widget, GdkEventButton * event,
                         gpointer d)
{
	if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
		gtk_signal_emit_stop_by_name(GTK_OBJECT(widget),
				   "button_press_event");
		group_menu (event, d);
	}
}

static void contact_click (GtkWidget *widget, GdkEventButton * event,
                         gpointer d)
{
	if (event->type == GDK_2BUTTON_PRESS && event->button == 1)
    		eb_chat_window_display_contact((struct contact *)d);
	else if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
		gtk_signal_emit_stop_by_name(GTK_OBJECT(widget),
				       "button_press_event");
		contact_menu (event, d);
	}
}


static void account_click (GtkWidget *widget, GdkEventButton * event,
                         gpointer d)
{
	if (event->type == GDK_2BUTTON_PRESS && event->button == 1)
		eb_chat_window_display_account((eb_account *)d);
	else if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
		gtk_signal_emit_stop_by_name(GTK_OBJECT(widget),
					"button_press_event");
		account_menu (event, d);
	}
}

/*
 * End Mouse button event handlers for elements of the group/contact/account
 * list
 */

static void add_callback(GtkWidget *widget, GtkTree *tree)
{
	show_add_contact_window();
}

static void add_group_callback(GtkWidget *widget, GtkTree *tree)
{
	show_add_group_window();
}

static void eb_add_accounts( GtkWidget * widget, gpointer stats )
{
	ay_edit_local_accounts();
}

static void eb_edit_accounts( GtkWidget * widget, gpointer stats )
{
	ayttm_prefs_show_window();
}

static void build_prefs_callback( GtkWidget * widget, gpointer stats )
{
	ayttm_prefs_show_window();
}

static void launch_group_chat( GtkWidget * widget, gpointer userdata )
{
	open_join_chat_window();
}

#if ADD_DEBUG_TO_MENU
static void ay_dump_structures_cb ( GtkWidget * widget, gpointer userdata )
{
	ay_dump_cts();	
	ay_dump_elas();	
}
#endif

static void ay_compare_version (const char *version, int warn_again)
{
	char *warned = cGetLocalPref("last_warned_version");
	
	if (strcmp(version, VERSION)) {
		/* versions differ, should I warn ? */
		if (warn_again || !warned || strcmp(warned, version)) {
			char * buf = g_strdup_printf(_("A new release of ayttm is available.\n"
					"Last version is %s, while you have %s.\n"), version, VERSION);
			ay_do_info(_("New release available !"), buf);
			cSetLocalPref("last_warned_version", (char *)version);
			ayttm_prefs_write();
		}
	}
	else if (warn_again) {
		ay_do_info(_("No new release"), _("You have the latest release of ayttm installed."));
		cSetLocalPref("last_warned_version", (char *)version);
		ayttm_prefs_write();
	}
}

static void ay_check_release ( GtkWidget * widget, gpointer userdata )
{
	char *rss = NULL;
	int found = 0;
	char *version = NULL;
	int is_auto = GPOINTER_TO_INT(userdata);
	rss = ay_http_get("http://sourceforge.net/export/rss2_projfiles.php?group_id=77614");
	if (rss != NULL) {
		if (strstr(rss, "\t\t\t<title>ayttm ")) {
			/* beginning matches */
			char *released = NULL;
			released = strstr(rss, "\t\t\t<title>ayttm ");
			if (released && strstr(released, " released (")) {
				/* end matches */
				version = g_strndup(released + strlen("\t\t\t<title>ayttm "), 
							strlen("x.x.x"));
				found = 1;
			}
		}
	}
	free(rss);
	if (version) {
		eb_debug(DBG_CORE, "Last version: %s\n", version);
		ay_compare_version(version, !is_auto);
		free(version);
	}
}

/* Be sure to free the menu items in the status menu */
static void eb_status_remove(GtkContainer *container,  GtkWidget * widget, gpointer stats )
{
	g_free(stats);
}

static void eb_status( GtkCheckMenuItem * widget, gpointer stats )
{
	struct acctStatus *s;
	int current_state=0, new_state=0;;

	s = (struct acctStatus *)stats;
	eb_debug(DBG_CORE, "Status of radio button for state[%i]: %i\n", s->status, widget->active);
	current_state = eb_services[s->ela->service_id].sc->get_current_state(s->ela);
	/* We were called for the deactivating state, ignore it */
	if(!widget->active) {
		eb_debug(DBG_CORE, "Current state is %i\n", current_state);
		return;
	}
	eb_debug(DBG_CORE, "Setting %s to state %d\n", eb_services[s->ela->service_id].name, s->status);
	eb_debug(DBG_CORE, "Current state is %i\n", current_state);
	if (current_state != s->status) {
		eb_debug(DBG_CORE, "Calling set_current_state: %i\n", s->status);
		eb_services[s->ela->service_id].sc->set_current_state(s->ela, s->status);
		new_state = eb_services[s->ela->service_id].sc->get_current_state(s->ela);
		/* Did the state change work? */
		if(new_state != s->status)
			eb_set_active_menu_status(s->ela->status_menu, new_state);
	}
	eb_debug(DBG_CORE, "%s set to state %d.\n", eb_services[s->ela->service_id].name, s->status );
	/* in about 10 seconds try to flush contact mgmt queue */
	if (s->ela->mgmt_flush_tag == 0 
	&& (s->ela->connecting || s->ela->connected)) {
		s->ela->mgmt_flush_tag = 
			eb_timeout_add(10000 + (1000*s->ela->service_id),
				 (GtkFunction)contact_mgmt_flush, (gpointer)s->ela);
	}
	if (s->ela->mgmt_flush_tag 
	&& !s->ela->connecting && !s->ela->connected) {
		eb_timeout_remove(s->ela->mgmt_flush_tag);
		s->ela->mgmt_flush_tag=0;
	}
	set_menu_sensitivity();
}

static void eb_sign_on_predef(int all) 
{
	LList *node = accounts ;
	while(node) {
		eb_local_account *ac = (eb_local_account*)(node->data);
		if (!ac->connected && (all || ac->connect_at_startup))
			RUN_SERVICE(ac)->login(ac) ;
		node = node->next ;
	}
	set_menu_sensitivity();
}

void eb_sign_on_all() 
{
	eb_sign_on_predef(1);
	if (iGetLocalPref("do_version_check")) {
		ay_check_release(NULL, GINT_TO_POINTER(1));
	}
}

void eb_sign_on_startup() 
{
	eb_sign_on_predef(0);
}

void eb_sign_off_all() 
{
	LList *node = accounts ;
	while(node) {
		eb_local_account *ac = (eb_local_account*)(node->data);
		if (ac && ac->connected)
			RUN_SERVICE(ac)->logout(ac) ;
		if (ac && ac->mgmt_flush_tag) {
			eb_timeout_remove(ac->mgmt_flush_tag);
			ac->mgmt_flush_tag=0;
		}
		node = node->next ;
	}
	set_menu_sensitivity();

}

static gint get_contact_position( struct contact * ec)
{
	gint i=0;
	LList *l;
	
	for (l = ec->group->members; l && (l->data != ec); l=l->next)  {
		struct contact * contact = l->data;
		if (contact->list_item)
			i++;
	}
	return i;
}

static gint get_account_position( eb_account * ea)
{
	gint i=0;
	LList *l;
	
	for (l = ea->account_contact->accounts; l && (l->data != ea); l=l->next) {
		eb_account * account = l->data;
		if (account->list_item)
			i++;
	}
	return i;
}

void reset_list (void)
{
	LList *grps = groups;
	for (grps = groups; grps; grps = grps->next)
		remove_group_line(grps->data);
}
/* General purpose update Contact List */
void update_contact_list ()
{
	LList * grps;
	LList * contacts;
	LList * accounts;

	grouplist * grp;
	struct contact * con;
	eb_account * ea;

	/* Error Check */
	if ( (status_show < 0) || (status_show > 2) )
		status_show = 2;

	for (grps = groups; grps; grps = grps->next) {
		grp = grps->data;

		/* Currently, groups are always visible so we never touch them :) */

		for (contacts = grp->members; contacts; contacts = contacts->next) {
			con = contacts->data;

			/* Visibility of contact */

			/* show_all_accounts presumes show_all_contacts */
			if ((status_show == 1) || (status_show == 0)) {
				/* MUST show the contact */
				add_contact_line (con);
				contact_update_status (con);
			} else {
				/* defer to whether it's online or not */

				if (! con->online)
					remove_contact_line (con);
				else
					contact_update_status (con);
			}

			for (accounts = con->accounts; accounts; accounts = accounts->next) {
				ea = accounts->data;

				if ( (status_show == 0) || (status_show == 1) ) {
					/* definitely visible */

					add_account_line(ea);
					buddy_update_status(ea);

					if (con->list_item == NULL)
						fprintf (stderr, _("Account vanished after add_account_line.\n"));
					else if (status_show == 0)
						gtk_tree_item_expand (GTK_TREE_ITEM(con->list_item));
					else
						gtk_tree_item_collapse (GTK_TREE_ITEM(con->list_item));
				} else {
					/* Close it up */
					if (ea->online) {
						buddy_update_status(ea);

						if (con->list_item != NULL)
							gtk_tree_item_collapse (GTK_TREE_ITEM(con->list_item));
						else
							fprintf (stderr, _("Account missing while online?\n"));
					}
					else
						remove_account_line(ea);
				}
			} /* End for loop for accounts */

		} /* End for loop for contacts */

	} /* End for loop for groups */

}

/* makes all accounts visible on the contact list */

/* shows a contact tree and its accounts and explicitly draws
   each account rather than waiting for a login message from
   the server.  Useful for reshowing a contact after removing
   it from the buddy list. */
void add_contact_and_accounts(struct contact * c)
{
	LList *l;
	for (l = c->accounts; l; l = l->next)  {
		eb_account * ea = l->data;
		if ((status_show == 0) || (status_show == 1) || ea->online)  {
			add_account_line(ea);
			buddy_update_status(ea);
		}
	}
}

static GdkPixmap * iconlogin_pm = NULL;
static GdkBitmap * iconlogin_bm = NULL;
static GdkPixmap * iconblank_pm = NULL;
static GdkBitmap * iconblank_bm = NULL;
static GdkPixmap * iconlogoff_pm = NULL;
static GdkBitmap * iconlogoff_bm = NULL;

static GtkTargetEntry drag_types[1] =
{
	{"text/plain", GTK_TARGET_SAME_APP, 0}
};

static gpointer dndtarget = NULL;
static gboolean drag_motion_cb(GtkWidget      *widget,
			       GdkDragContext *context,
			       gint            x,
			       gint            y,
			       guint           time,
			       gpointer        data)
{
	dndtarget = data;
	return 1;
}

static void start_drag(GtkWidget *widget, GdkDragContext *dc, gpointer data)
{
	dndtarget=NULL;
}

static void drag_data_get(GtkWidget        *widget,
			  GdkDragContext   *drag_context,
			  GtkSelectionData *selection_data,
			  guint             info,
			  guint             time,
			  gpointer	       data)
{
	grouplist *gl=(grouplist *)dndtarget;
	struct contact *ec = data;
	if(gl == NULL || ec == NULL) {
		gtk_drag_finish(drag_context, FALSE, FALSE, time);
		return;
	}
	move_contact(gl->name, ec);
	update_contact_list ();
	write_contact_list();
	gtk_drag_finish(drag_context, TRUE, TRUE, time);
}

/*
 * Add/Remove entries to/from box -- group, contact, account
 */

/* makes a group visible on the contact list */
void add_group_line(grouplist * eg)
{
	GtkWidget * box;

	/* Might call add_group() - which calls add_group_line() 
	   before contact_list exists - this is OK so just return */
	if (eg->list_item || !contact_list)
		return;
		
	eg->list_item = gtk_tree_item_new();

	box = gtk_hbox_new(FALSE, 1);

	eg->label = gtk_label_new(eg->name);
	gtk_label_set_justify(GTK_LABEL(eg->label), GTK_JUSTIFY_LEFT);

	gtk_box_pack_start(GTK_BOX(box), eg->label, FALSE, FALSE, 1);
	gtk_widget_show(eg->label);

	gtk_container_add(GTK_CONTAINER(eg->list_item), box);
	gtk_widget_show(box);

	gtk_object_set_user_data(GTK_OBJECT(eg->list_item), (gpointer)eg);
	eg->contacts_online = 0;
	eg->contacts_shown = 0;
	eg->tree = NULL;
	gtk_tree_append(GTK_TREE(contact_list), eg->list_item);

	gtk_signal_connect(GTK_OBJECT(eg->list_item),  "button_press_event",
			   GTK_SIGNAL_FUNC(group_click),
			   (grouplist *)eg );
	gtk_drag_dest_set(eg->list_item, GTK_DEST_DEFAULT_ALL &
			  ~GTK_DEST_DEFAULT_HIGHLIGHT,
			  drag_types, 1,
			  GDK_ACTION_MOVE|GDK_ACTION_DEFAULT);
	gtk_signal_connect(GTK_OBJECT(eg->list_item), "drag_motion",
			   GTK_SIGNAL_FUNC(drag_motion_cb), eg);
	
	gtk_widget_show(eg->list_item);
}

static void set_status_label(eb_account *ea, int update_contact)
{
	char * c = NULL, *tmp = NULL;
	int need_tooltip_update = 0;
	int need_tooltip_display = 0;
	tmp = g_strndup(RUN_SERVICE(ea)->get_status_string(ea), 20);
	c = g_strdup_printf("%s%s%s",
			strlen(tmp)?"(":"",
			tmp,
			strlen(tmp)?")":"");
	
	while (strchr(c,'\n') != NULL) {
		char *t = strchr(c,'\n');
		*t = ' ';
	}
	if(strlen(c) == 20) {
		c[19] = c[18] = c[17] = '.';
		if(!status_tips)
			status_tips = gtk_tooltips_new();
		/*
		 * that 3rd parameter is not a bug, it really is a useless
		 * parameter
		 */

		gtk_tooltips_set_tip(GTK_TOOLTIPS(status_tips), ea->list_item,
				RUN_SERVICE(ea)->get_status_string(ea),
				"");
	}
	if (!ea->status)
		ea->status = gtk_label_new(c);
	else
		gtk_label_set_text(GTK_LABEL(ea->status),c);
	
	if (update_contact && !ea->account_contact->status) {
		ea->account_contact->status = gtk_label_new(c);
		need_tooltip_update = 1;
	} else if (update_contact) {
		need_tooltip_update = (ea->account_contact->last_status 
					&& strcmp(c, ea->account_contact->last_status));
		need_tooltip_display = !gtk_tooltips_data_get(ea->account_contact->list_item);
		
		gtk_label_set_text(ea->account_contact->status, c);
	}
	
	if (update_contact && (need_tooltip_display || need_tooltip_update)) {
		struct tm *mytime;
		char buff[128];
		char *status = RUN_SERVICE(ea)->get_status_string(ea);
		char *status_line = NULL;
		
		if (need_tooltip_update) {
			time(&ea->account_contact->last_status_change);
			if (ea->account_contact->last_status)
				free(ea->account_contact->last_status);
			ea->account_contact->last_status = strdup(c);
		}
			
		if (ea->account_contact->last_status_change != 0) {
			mytime = localtime(&ea->account_contact->last_status_change);
			strftime(buff, 128, "%H:%M (%b %d)", mytime);
			status_line = g_strdup_printf(
					_("%s since %s"),
					strlen(status)?status:"Online", buff);

			if(!status_tips)
				status_tips = gtk_tooltips_new();
			gtk_tooltips_set_tip(GTK_TOOLTIPS(status_tips), 
				ea->account_contact->list_item,
				status_line,
				"");

			g_free(status_line);
		}
	}
	g_free(c);
	g_free(tmp);
}
	
/* makes an account visible on the buddy list, making the contact visible
   if necessary */
void add_account_line( eb_account * ea)
{
	GtkWidget * box, * label;
	
	if (ea->list_item)
		return;
	
	add_contact_line(ea->account_contact);

	ea->list_item = gtk_tree_item_new();

	box = gtk_hbox_new(FALSE, 1);
	ea->pix = gtk_pixmap_new(iconblank_pm, iconblank_bm);
	label = gtk_label_new(ea->handle);
	set_status_label(ea, TRUE);

	gtk_box_pack_start(GTK_BOX(box), ea->pix, FALSE, FALSE, 1);
	gtk_widget_show(ea->pix);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 1);
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(box), ea->status, FALSE, FALSE, 1);
	gtk_widget_show(ea->status);

	gtk_container_add(GTK_CONTAINER(ea->list_item), box);
	gtk_widget_show(box);

	ea->icon_handler = -1;

	gtk_object_set_user_data(GTK_OBJECT(ea->list_item), ea);
	gtk_tree_insert(GTK_TREE(ea->account_contact->tree), ea->list_item,
		get_account_position(ea));

	gtk_signal_connect(GTK_OBJECT(ea->list_item),  "button_press_event",
				  GTK_SIGNAL_FUNC(account_click),
				  (eb_account *)ea );

	gtk_widget_show(ea->list_item);

}

/* makes a contact visible on the buddy list */
void add_contact_line( struct contact * ec)
{
	GtkWidget * box;
	
	if (ec->list_item)
		return;
	
	ec->list_item = gtk_tree_item_new();
	ec->tree = gtk_tree_new();

	box = gtk_hbox_new(FALSE, 1);
	ec->pix = gtk_pixmap_new(iconblank_pm, iconblank_bm);
	ec->label = gtk_label_new(ec->nick);
	ec->status = gtk_label_new("");

	gtk_box_pack_start(GTK_BOX(box), ec->pix, FALSE, FALSE, 1);
	gtk_widget_show(ec->pix);
	gtk_misc_set_alignment(GTK_MISC(ec->label), 0.0, 0.5);
	gtk_box_pack_start(GTK_BOX(box), ec->label, TRUE, TRUE, 1);
	gtk_widget_show(ec->label);
	gtk_box_pack_start(GTK_BOX(box), ec->status, FALSE, FALSE, 1);
	gtk_widget_show(ec->status);

	gtk_container_add(GTK_CONTAINER(ec->list_item), box);
	gtk_widget_show(box);

	ec->icon_handler = -1;

	gtk_object_set_user_data(GTK_OBJECT(ec->list_item), ec);
	
	if (!ec->group->contacts_shown) {
		ec->group->tree = gtk_tree_new();
		gtk_tree_item_set_subtree(GTK_TREE_ITEM(ec->group->list_item),
			ec->group->tree);
		if(strcmp(_("Unknown"),ec->group->name) !=0 &&
		   strcmp(_("Ignore"),ec->group->name) !=0)
			gtk_tree_item_expand(GTK_TREE_ITEM(ec->group->list_item));
	}
	gtk_drag_source_set(ec->list_item,
		GDK_BUTTON1_MASK | GDK_BUTTON2_MASK,
                drag_types, 1,
		GDK_ACTION_MOVE|GDK_ACTION_DEFAULT);
	gtk_signal_connect(GTK_OBJECT(ec->list_item), "drag_begin",
			   GTK_SIGNAL_FUNC(start_drag), ec);
	gtk_signal_connect(GTK_OBJECT(ec->list_item), "drag_data_get",
			   GTK_SIGNAL_FUNC(drag_data_get),
			   ec);
	
	ec->group->contacts_shown++;
	gtk_tree_insert(GTK_TREE(ec->group->tree), ec->list_item,
		get_contact_position(ec));

	gtk_tree_item_set_subtree(GTK_TREE_ITEM(ec->list_item), ec->tree);
	if(ec->expanded)
		gtk_tree_item_expand(GTK_TREE_ITEM(ec->list_item));
	else
		gtk_tree_item_collapse(GTK_TREE_ITEM(ec->list_item));

	gtk_signal_connect(GTK_OBJECT(ec->list_item),  "button_press_event",
				  GTK_SIGNAL_FUNC(contact_click),
				  (struct contact*)ec );
	gtk_signal_connect(GTK_OBJECT(ec->list_item), "expand",
				  GTK_SIGNAL_FUNC(expand_contact), (struct contact*)ec);
	gtk_signal_connect(GTK_OBJECT(ec->list_item), "collapse",
				  GTK_SIGNAL_FUNC(collapse_contact), (struct contact*)ec );
		
	gtk_widget_show(ec->list_item);	
}

/* hides a group on the buddy list */
void remove_group_line( grouplist * eg)
{
	LList * contacts;
	
	if (!eg->list_item)
		return;
		
	for (contacts = eg->members; contacts; contacts = contacts->next) {
		struct contact * ec = contacts->data;
		if (ec->list_item)
			remove_contact_line(ec);
	}
		
	gtk_container_remove(GTK_CONTAINER(contact_list), eg->list_item);
	eg->list_item = NULL;
	eg->tree = NULL;
	eg->label = NULL;
}



/* hides an account on the buddy list, hiding any child accounts in the
   process */
void remove_contact_line( struct contact * ec)
{
	LList * accounts;
	
	if (!ec->list_item)
		return;
		
	for (accounts = ec->accounts; accounts; accounts = accounts->next) {
		eb_account * ea = accounts->data;
		if (ea->list_item)
			remove_account_line(ea);
	}
	
	ec->group->contacts_shown--;
	gtk_container_remove(GTK_CONTAINER(ec->group->tree), ec->list_item);
	ec->list_item = NULL;
	ec->tree = NULL;
	ec->pix = NULL;
	ec->status = NULL;
	ec->label = NULL;
	if (ec->icon_handler != -1)
		eb_timeout_remove(ec->icon_handler);
	ec->icon_handler = -1;
}


/* hides an account on the buddy list (removes it from the tree) */
void remove_account_line( eb_account * ea)
{

	if (!ea || !ea->account_contact || !ea->list_item) {
		return;
	}

	gtk_container_remove(GTK_CONTAINER(ea->account_contact->tree), ea->list_item);
	ea->list_item = NULL;
	ea->pix = NULL;
	ea->status = NULL;
	if (ea->icon_handler != -1)
		eb_timeout_remove(ea->icon_handler);
	ea->icon_handler = -1;
}

/* timeout function called 10 seconds after a contact logs off
   (removes the logoff icon and hides the contact if necessary) */
static gint hide_contact(struct contact * ec)
{
	LList * l;
	
	if (ec->icon_handler == -1)
		return FALSE;
	ec->icon_handler = -1;
	
	if (status_show == 2)		
		remove_contact_line(ec);
	else
		for (l = ec->accounts; l; l = l->next) {
			eb_account * account = l->data;
			buddy_update_status(account);
		}
	
	return FALSE;
}

/* timeout function called 10 seconds after an account logs off
   (removes the logoff icon and hides the account if necessary)
   This timeout is only set when other accounts in the parent
   contact are remaining shown */
static gint hide_account(eb_account * ea)
{
	if (ea->icon_handler == -1)
		return FALSE;
	ea->icon_handler = -1;
			
	if (!ea->list_item)
		return FALSE;
			
	/* 
	 * if status_show == 1 then we are in the Contacts Tab
	 * If all account lines are removed from here, leaving
	 * an empty contact, then the user cannot expand the 
	 * contact to see the list of accounts.  While this isn't
	 * such a big deal, it causes ayttm to segfault when
	 * you switch back to the Online view.
	 * 	- Philip
	 */
	if ((status_show == 2) /*|| (status_show == 1)*/)
		remove_account_line(ea);
	else
		buddy_update_status(ea);
	return FALSE;
}

/* update the status info (pixmap and state) of a contact */
void contact_update_status(struct contact * ec)
{
	eb_account * ea = NULL;
	LList * l;
	int width, height;
	int width2, height2;
	int width3, height3;

	/* find the account who's status information should be reflected in
	   the contact line (preferably the default protocol account, but
	   if that one is not logged on, use another) */
	for (l = ec->accounts; l; l = l->next) {
		eb_account * account = l->data;
		if(account->online) {
			if(account->service_id == ec->default_chatb) {
				ea = account;
				break;
			} else if(ea == NULL || !ea->online) {
				ea = account;
			}
		} else if(ea == NULL) {
			if(account->service_id == ec->default_chatb) {
				ea = account;
			}
		}
	}

	if (!ea)
		return;

	set_status_label(ea, TRUE);
	
	gtk_label_set_text(GTK_LABEL(ec->label), ec->nick);

	/* set the icon if there isn't another timeout about to alter the icon */
	if (ec->icon_handler == -1) {
		gtkut_set_pixmap(ea->ela, 
			RUN_SERVICE(ea)->get_status_pixmap(ea), 
			&ec->pix);
	}

	width = contact_list->allocation.width;
	height = contact_list->allocation.height;

	if(GTK_WIDGET_VISIBLE(GTK_SCROLLED_WINDOW(contact_window)->vscrollbar)) {
		width3 = GTK_SCROLLED_WINDOW(contact_window)->vscrollbar->allocation.width;
		height3 = GTK_SCROLLED_WINDOW(contact_window)->vscrollbar->allocation.height;
	}
	else {
		width3 = 0;
		height3 = 0;
	}

	if ( iGetLocalPref("do_noautoresize") == 0 ) {
		width2 = contact_window->allocation.width;
		height2 = contact_window->allocation.height;

		if(width+width3> width2)
			gtk_widget_set_usize(contact_window,width+width3+2,height2);
	}
		
}

/* called by a service module when a buddy's status changes
 * this will call buddy_update_status.  The service should
 * call this so that the change is written to the log file
 */
void buddy_update_status_and_log(eb_account * ea)
{
	eb_log_status_changed(ea, RUN_SERVICE(ea)->get_status_string(ea));
	buddy_update_status(ea);

	if (!ea->account_contact->last_status)
		ea->account_contact->last_status = strdup("dummy");
}

/* update the status info (pixmap and state) of an account */
void buddy_update_status(eb_account * ea)
{
	char *c = NULL, *tmp = NULL;
	if (!ea || !ea->list_item)
		return;
	
	tmp = g_strndup(RUN_SERVICE(ea)->get_status_string(ea), 20);
	c = g_strdup_printf("%s%s%s",
			strlen(tmp)?"(":"",
			tmp,
			strlen(tmp)?")":"");
	
	set_status_label(ea, FALSE);
	eb_update_status(ea, c);
	
	
	g_free(c);
	g_free(tmp);
	
	/* update the icon if another timeout isn't about to change it */
	if (ea->icon_handler == -1) {
		if (RUN_SERVICE(ea) && RUN_SERVICE(ea)->get_status_pixmap && ea->pix)
			gtkut_set_pixmap(ea->ela, 
				RUN_SERVICE(ea)->get_status_pixmap(ea), 
				&ea->pix);
	}

	/* since the contact's status info  might be a copy of this
	   account's status info, we should refresh that also */	
	contact_update_status(ea->account_contact);	
}

/* the timeout function called after a contact logs in (to take the
   "open door" icon away) */
static gint set_contact_icon(struct contact * ec)
{
	/* abort if another timeout already took care of it */
	if (ec->icon_handler == -1)
		return FALSE;
	ec->icon_handler = -1;
		
	contact_update_status(ec);
	return FALSE;
}

/* the timeout function called after an account logs in (to take the
   "open door" icon away) */
static gint set_account_icon(eb_account * ea)
{
	/* abort if another timeout already took care of it */
	if (ea->icon_handler == -1)
		return FALSE;
	ea->icon_handler = -1;
	
	/* do it here for pounce (else it's too soon) */
	if (ea->account_contact->online == 1)
		do_trigger_online(ea->account_contact);

	buddy_update_status(ea);
	return FALSE;
}

static gint update_window_title(struct contact *ec) 
{
	gchar * title = NULL;
	if (window_title_handler != -1)
		eb_timeout_remove(window_title_handler);
	
	if (ec != NULL) {
		title = g_strdup_printf("(%c %s)", 
			ec->online ? '+':'-', ec->nick);
		window_title_handler = eb_timeout_add(5000, 
					(GtkFunction)update_window_title, 
					NULL);
	} else {
		title = _(PACKAGE_STRING"-"RELEASE);
	}
	
	gtk_window_set_title(GTK_WINDOW(statuswindow), title);

	if (ec!= NULL)
		g_free(title);
	
	return 0;
}
	
/* function called when a contact logs in */
static void contact_login(struct contact * ec)
{
	char buff[1024];
	ec->group->contacts_online++;

	/* display the "open door" icon */
	gtk_pixmap_set(GTK_PIXMAP(ec->pix), iconlogin_pm, iconlogin_bm);
	
	/* remove any other timeouts (if a user just logged out immediately before) */
	if (ec->icon_handler != -1)
		eb_timeout_remove(ec->icon_handler);
		
	/* timeout to set the contact icon in 10 seconds */
	ec->icon_handler = eb_timeout_add(10000, (GtkFunction)set_contact_icon,
		(gpointer) ec);

	if((time(NULL) - last_sound_played > 0) && iGetLocalPref("do_online_sound")) {
		/* If we have the do_no_sound_for_ignore flag set,
		   only play the sound if the contact is not ignored. */
		if (!iGetLocalPref("do_no_sound_for_ignore") ||
		(strcasecmp(ec->group->name, _("Ignore")) != 0)) {
			play_sound( SOUND_BUDDY_ARRIVE );
			last_sound_played = time(NULL);
		}
	}
	
	eb_chat_window_do_timestamp(ec, 1);
	g_snprintf(buff, 1024, _("%s is now online"), ec->nick);
	update_window_title(ec);
	update_status_message(buff);
}

/* function called when a contact logs off */
static void contact_logoff(struct contact * ec)
{
	char buff[1024];
	/* display the "closed door" icon */
	gtk_pixmap_set(GTK_PIXMAP(ec->pix), iconlogoff_pm, iconlogoff_bm);
	ec->group->contacts_online--;
	
	/* remove any other timeouts (if the user just logged in) */
	if (ec->icon_handler != -1)
		eb_timeout_remove(ec->icon_handler);
		
	/* timeout to remove the contact from the list */
	ec->icon_handler = eb_timeout_add(10000, (GtkFunction)hide_contact,
		(gpointer) ec);

	if((time(NULL) - last_sound_played > 0) && iGetLocalPref("do_online_sound")) {
		/* If we have the do_no_sound_for_ignore flag set,
		   only play the sound if the contact is not ignored. */
		if (!iGetLocalPref("do_no_sound_for_ignore") ||
		(strcasecmp(ec->group->name, _("Ignore")) != 0)) {
			play_sound( SOUND_BUDDY_LEAVE );
			last_sound_played = time(NULL);
		}
	}

	do_trigger_offline(ec);

	eb_chat_window_do_timestamp(ec, 0);
	g_snprintf(buff, 1024, _("%s is now offline"), ec->nick);
	update_window_title(ec);
	update_status_message(buff); 
}

/* timeout called every 30 seconds for each online account to update
   its status */
static gint refresh_buddy_status(eb_account * ea)
{
	/* remove the timeout if the account is no longer displayed */
	if (!ea->list_item || ea->status_handler == -1) {
		ea->status_handler = -1;
		return FALSE;
	}
	
	/* don't refresh if a "door" icon is being displayed */
	if (ea->icon_handler != -1)
		return TRUE;
		
	buddy_update_status(ea);
	return TRUE;
}

/* called to signify that a buddy has logged in, and updates the GUI
   accordingly */
void buddy_login(eb_account * ea)
{
	
	/* don't do anything if this has already been done */
	if (ea->online)
		return;
	
	ea->account_contact->online++;
	ea->online = TRUE;

	if (!ea->account_contact->last_status)
		ea->account_contact->last_status = strdup("dummy");
	
	if (iGetLocalPref("do_ignore_unknown") && !strcmp(_("Unknown"), ea->account_contact->group->name))
		return;
	
	add_account_line(ea);
	
	/* sets the "open door" icon */
	gtk_pixmap_set(GTK_PIXMAP(ea->pix), iconlogin_pm, iconlogin_bm);

	/* set the timeout to remove the "open door" icon */
	if (ea->icon_handler != -1)
		eb_timeout_remove(ea->icon_handler);
	ea->icon_handler = eb_timeout_add(10000, (GtkFunction)set_account_icon,
		(gpointer) ea);
	
	/* if there is only one account (this one) logged in under the
	   parent contact, we must login the contact also */
	if (ea->account_contact->online == 1)
		contact_login(ea->account_contact);
		
	buddy_update_status(ea);
	
	/* make sure the status gets updated often */
	ea->status_handler = eb_timeout_add(30000,
		(GtkFunction)refresh_buddy_status, (gpointer) ea);
}

/* called to signify that a buddy has logged off, and updates the GUI
   accordingly */
void buddy_logoff(eb_account * ea)
{
	
	/* don't do anything if this has already been done */
	if (!ea || !ea->online)
		return;
	
	ea->account_contact->online--;
	ea->online = FALSE;

	if (!ea->account_contact->last_status)
		ea->account_contact->last_status = strdup("dummy");

	if (iGetLocalPref("do_ignore_unknown") && !strcmp(_("Unknown"), ea->account_contact->group->name))
		return;

	/* sets the "closed door" icon */
	gtk_pixmap_set(GTK_PIXMAP(ea->pix), iconlogoff_pm, iconlogoff_bm);

	/* removes any previously set timeouts for the account */ 
	if (ea->icon_handler != -1)
		eb_timeout_remove(ea->icon_handler);
	ea->icon_handler = -1;
	if (ea->status_handler != -1)
		eb_timeout_remove(ea->status_handler);
	ea->status_handler = -1;
	
	/* if this is the last account of the parent contact to log off,
	   we must log off the contact also */
	if (ea->account_contact->online == 0)
		contact_logoff(ea->account_contact);

	/* timeout to remove the "close door" icon */
	ea->icon_handler = eb_timeout_add(10000, (GtkFunction)hide_account,
		(gpointer) ea);

}

void update_contact_window_length ()
{
  int h,w;
  h = iGetLocalPref("length_contact_window");
  w = iGetLocalPref("width_contact_window");
  if (h == 0) 
	  h = 256;
  if (w == 0)
	  w = 150;
  eb_debug(DBG_CORE, "statuswindow size: %dx%d\n",h,w);
  gtk_widget_set_usize(contact_window, w, h);
}

/* Generates the contact list tree (should only be called once) */
static GtkWidget* MakeContactList()
{
	LList * l1;
	
	contact_window = gtk_scrolled_window_new(NULL, NULL);
	contact_list = gtk_tree_new();

	for (l1 = groups; l1; l1=l1->next) {
		grouplist * grp = l1->data;
		add_group_line(grp);
	}
	
	gtk_widget_show(contact_list);
	gtk_scrolled_window_add_with_viewport
	  (GTK_SCROLLED_WINDOW(contact_window),
	   contact_list);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(contact_window),
		GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	update_contact_window_length ();
	return (contact_window);
}



static GtkWidget* MakeStatusMenu(eb_local_account * ela)
{
	  GtkWidget* status_menu_item;
	  GtkWidget* status_menu;
	  LList * status_label;
	  LList * temp_list;
	  GtkWidget* hbox, *label;
	  GtkStyle* style;
	  GSList * group = NULL;
	  LList * widgets = NULL;
	  int x;
	  gchar string[255];
	  status_menu = gtk_menu_new();
	  style = gtk_widget_get_style(status_menu);

	  assert(ela);
	  gtk_widget_realize(status_menu);
	  status_label = eb_services[ela->service_id].sc->get_states();

	  status_menu_item = gtk_tearoff_menu_item_new();
	  gtk_menu_append(GTK_MENU(status_menu), status_menu_item);
	  gtk_widget_show(status_menu_item);

	  for(temp_list = status_label, x = 0; temp_list; x++, temp_list=temp_list->next) {
		struct acctStatus * stats = g_new0(struct acctStatus, 1);
		
		stats->ela = ela;
		stats->status= x;
		
		status_menu_item = gtk_radio_menu_item_new(group);
		group = gtk_radio_menu_item_group(GTK_RADIO_MENU_ITEM(status_menu_item));
		widgets = l_list_append(widgets, status_menu_item);
		hbox = gtk_hbox_new(FALSE, 3);
		label = gtk_label_new((gchar*)temp_list->data);
		
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
		gtk_widget_show(label);
		gtk_widget_show(hbox);
		gtk_widget_show(status_menu_item);
		gtk_container_add(GTK_CONTAINER(status_menu_item), hbox);
		gtk_menu_append(GTK_MENU(status_menu), status_menu_item);
		gtk_signal_connect(GTK_OBJECT(status_menu_item), "activate", 
				eb_status, (gpointer) stats );
		gtk_signal_connect(GTK_OBJECT(status_menu_item), "remove", 
				eb_status_remove, (gpointer) stats );

	}

	ela->status_menu = widgets;

	g_snprintf(string, 255, "%s [%s]", ela->handle, get_service_name(ela->service_id));

	ela->status_button = gtk_menu_item_new_with_label(string);
  
	if (!widgets)
		return ela->status_button; 

	/* First deactivate zeroth status radio item */
	GTK_CHECK_MENU_ITEM(l_list_nth(widgets, 0)->data)->active = 0 ;
	/* Now, activate the desired status radio item */
	GTK_CHECK_MENU_ITEM(
		l_list_nth(widgets, eb_services[ela->service_id].sc->get_current_state(ela))->data)->active = 1 ;
  
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(ela->status_button), status_menu);
	l_list_free(status_label);

	return ela->status_button;
}

static void eb_smiley_function(GtkWidget *widget, gpointer data) {
	menu_item_data *mid = data;

	assert(data);
	fprintf(stderr, _("eb_smiley_function: calling callback\n"));
	mid->callback(mid->user_data);
}

static void eb_import_function(GtkWidget *widget, gpointer data) {
	menu_item_data *mid = data;

	assert(data);
	fprintf(stderr, _("eb_import_function: calling callback\n"));
	mid->callback(mid->user_data);
	update_contact_list ();
	write_contact_list();
}

static void eb_profile_function(GtkWidget *widget, gpointer data) {
	menu_item_data *mid=data;

	assert(data);
	fprintf(stderr, _("eb_profile_function: calling callback\n"));
	mid->callback(mid->user_data);
	update_contact_list ();
	write_contact_list();
}


void eb_profile_window(void * v_profile_submenuitem)
{
	GtkWidget *profile_submenuitem = v_profile_submenuitem;
	GtkWidget *label;
	LList *list=NULL;
	GtkWidget * profile_menu = gtk_menu_new();
	menu_data *md=NULL;
	menu_item_data *mid=NULL;
	gboolean added = FALSE;

	label = gtk_tearoff_menu_item_new();
	gtk_menu_append(GTK_MENU(profile_menu), label);
	gtk_widget_show(label);

	md = GetPref(EB_PROFILE_MENU);
	if(md) {
		for(list = md->menu_items; list; list  = list->next ) {
			mid=(menu_item_data *)list->data;
			label = gtk_menu_item_new_with_label(mid->label);
			gtk_menu_append(GTK_MENU(profile_menu), label);
			gtk_signal_connect(GTK_OBJECT(label), "activate",
          			eb_profile_function, mid);
			gtk_widget_show(label);  
			added = TRUE;
		}
	}
	gtk_widget_set_sensitive(profile_submenuitem, added);
	
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(profile_submenuitem), profile_menu);
	gtk_widget_show(profile_menu);
	gtk_widget_show(profile_submenuitem);
}

void eb_smiley_window(void *v_smiley_submenuitem)
{
	GtkWidget * smiley_submenuitem = v_smiley_submenuitem;
	GtkWidget *label;
	GSList *group = NULL;
	LList *list=NULL;
	GtkWidget * smiley_menu = gtk_menu_new();
	menu_data *md=NULL;

	label = gtk_tearoff_menu_item_new();
	gtk_menu_append(GTK_MENU(smiley_menu), label);
	gtk_widget_show(label);


	md = GetPref(EB_SMILEY_MENU);
	if(md) {
		struct _menu_items {
			menu_item_data *mid;
			GtkWidget *label;
		} *items=malloc(sizeof(struct _menu_items)*l_list_length(md->menu_items));
		int i;
		for(i=0,list = md->menu_items; list; i++, list = list->next ) {
			items[i].mid=(menu_item_data *)list->data;
			eb_debug(DBG_CORE, "adding smiley item: %s\n", items[i].mid->label);
			items[i].label = gtk_radio_menu_item_new_with_label(group, items[i].mid->label);
			group = gtk_radio_menu_item_group(GTK_RADIO_MENU_ITEM(items[i].label));
			if(md->active == items[i].mid)
				gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(items[i].label), TRUE);
			else
				gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(items[i].label), FALSE);
			gtk_menu_append(GTK_MENU(smiley_menu), items[i].label);
			gtk_widget_show(items[i].label);
		}
		for(--i; i>=0; i--)
			gtk_signal_connect(GTK_OBJECT(items[i].label), "activate",
					eb_smiley_function, items[i].mid);
		free(items);
	}
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(smiley_submenuitem), smiley_menu);
	gtk_widget_show(smiley_menu);
	gtk_widget_show(smiley_submenuitem);
}

void eb_import_window(void *v_import_submenuitem)
{
	GtkWidget * import_submenuitem = v_import_submenuitem;
	GtkWidget *label;
	LList *list=NULL;
	GtkWidget * import_menu = gtk_menu_new();
	menu_data *md=NULL;
	menu_item_data *mid=NULL;

	label = gtk_tearoff_menu_item_new();
	gtk_menu_append(GTK_MENU(import_menu), label);
	gtk_widget_show(label);

	md = GetPref(EB_IMPORT_MENU);
	if(md) {
		for(list = md->menu_items; list; list  = list->next ) {
			mid=(menu_item_data *)list->data;
			eb_debug(DBG_CORE, "adding import item: %s\n", mid->label);
			label = gtk_menu_item_new_with_label(mid->label);
			gtk_menu_append(GTK_MENU(import_menu), label);
			gtk_signal_connect(GTK_OBJECT(label), "activate",
					eb_import_function, mid);
			gtk_widget_show(label);  
		}
	}
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(import_submenuitem), import_menu);
	gtk_widget_show(import_menu);
	gtk_widget_show(import_submenuitem);
}

void eb_set_status_window(void *v_set_status_submenuitem)
{
	GtkWidget * set_status_submenuitem = v_set_status_submenuitem;
	GtkWidget *label;
	GtkWidget * account_menu = gtk_menu_new();
	LList *list=NULL;

	label = gtk_tearoff_menu_item_new();
	gtk_menu_append(GTK_MENU(account_menu), label); 
	gtk_widget_show(label);
	for(list = accounts; list; list  = list->next ) {
		eb_local_account *ela = (eb_local_account *)list->data;
		label = MakeStatusMenu(ela);
		gtk_menu_append(GTK_MENU(account_menu), label);
		if (!ela->status_menu)
			gtk_widget_set_sensitive(label, FALSE);
		else
			gtk_widget_set_sensitive(label, TRUE);
		gtk_widget_show(label);
	}
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(set_status_submenuitem), account_menu);
	gtk_widget_show(account_menu);
	gtk_widget_show(set_status_submenuitem);
}

static GtkItemFactoryEntry menu_items[] = {
	{ N_("/_Chat"),		NULL,       NULL, 0, "<Branch>" },
	{ N_("/Chat/Set _status"),	
				NULL, 	    NULL, 0, NULL },
	{ N_("/Chat/Sign o_n all"),	
				"<control>A", eb_sign_on_all, 0, NULL },
	{ N_("/Chat/Sign o_ff all"),	
				"<control>F", eb_sign_off_all, 0, NULL },
	{ N_("/Chat/---"),	NULL, NULL, 0, "<Separator>" },
	{ N_("/Chat/New _group chat..."),
  				NULL, launch_group_chat, 0, NULL },
	{ N_("/Chat/Set as _away..."),	
				NULL, show_away_choicewindow, 0, NULL },
	{ N_("/Chat/---"),	NULL, NULL, 0, "<Separator>" },
	{ N_("/Chat/_Smileys"),	NULL, 	    NULL, 0, NULL },
	{ N_("/Chat/---"),	NULL, NULL, 0, "<Separator>" },
	{ N_("/Chat/_Quit"),	"<control>Q", delete_event, 0, NULL },

	{ N_("/_Edit"),		NULL,       NULL, 0, "<Branch>" },
	{ N_("/Edit/_Preferences..."),
  				NULL, build_prefs_callback, 0, NULL },
	{ N_("/Edit/Add or _delete accounts..."),
  				NULL, eb_add_accounts, 0, NULL },
	{ N_("/Edit/Edit _accounts..."),
  				NULL, eb_edit_accounts, 0, NULL },
	{ N_("/Edit/---"),	NULL, NULL, 0, "<Separator>" },
	{ N_("/Edit/Add a _contact account..."),		
  				NULL, add_callback, 0, NULL },
	{ N_("/Edit/Add a _group..."),
  				NULL, add_group_callback, 0, NULL },
	{ N_("/Edit/---"),	NULL,         NULL, 0, "<Separator>" },
	{ N_("/Edit/_Import"),	NULL, 	    NULL, 0, NULL },
	{ N_("/Edit/Set profi_le"),	NULL, 	    NULL, 0, NULL },
	
	{ N_("/_Help"),		NULL, NULL, 0, "<Branch>" },
#ifndef __MINGW32__
	{ N_("/Help/_Web site..."),	NULL, show_website, 0, NULL },
	{ N_("/Help/_Manual..."),	NULL, show_manual, 0, NULL },
	{ N_("/Help/---"),		NULL, NULL, 0, "<Separator>" },
#endif
	{ N_("/Help/_About Ayttm..."),NULL, ay_show_about, 0, NULL },
	{ N_("/Help/---"),		NULL, NULL, 0, "<Separator>" },
	{ N_("/Help/Check for new _release"),NULL, ay_check_release, 0, GINT_TO_POINTER(0) }
#if ADD_DEBUG_TO_MENU
	,
	{ N_("/Help/---"),		NULL, NULL, 0, "<Separator>" },
	{ N_("/Help/Dump structures"),NULL, ay_dump_structures_cb, 0, NULL }
#endif	
};

static GtkItemFactory *main_menu_factory = NULL;

static void menu_set_sensitive(GtkItemFactory *ifactory, const gchar *path,
			gboolean sensitive)
{
	GtkWidget *widget;

	if (ifactory == NULL)
		return;

	widget = gtk_item_factory_get_item(ifactory, path);
	if(widget == NULL) {
		eb_debug(DBG_CORE, "unknown menu entry %s\n", path);
		return;
	}
	gtk_widget_set_sensitive(widget, sensitive);
}


void set_menu_sensitivity(void)
{
	int online = connected_local_accounts();
	
	menu_set_sensitive(main_menu_factory, N_("/Chat/Set status"), l_list_length(accounts));
	menu_set_sensitive(main_menu_factory, N_("/Chat/New group chat..."), online);
	menu_set_sensitive(main_menu_factory, N_("/Chat/Set as away..."), online);
	menu_set_sensitive(main_menu_factory, N_("/Chat/Sign off all"), online);
	menu_set_sensitive(main_menu_factory, N_("/Chat/Sign on all"),
				(online != l_list_length(accounts)));
	
}

static gchar *menu_translate(const gchar *path, gpointer data)
{
	gchar *retval;

	retval = (gchar *)gettext(path);

	return retval;
}

static void get_main_menu( GtkWidget  *window,
                    GtkWidget **menubar )
{
	GtkItemFactory *item_factory;
	GtkAccelGroup *accel_group;
	gint nmenu_items = sizeof (menu_items) / sizeof (menu_items[0]);

	accel_group = gtk_accel_group_new ();

	item_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<main>", 
                        	       accel_group);
	main_menu_factory = item_factory;
	gtk_item_factory_set_translate_func(item_factory, menu_translate,
					    NULL, NULL);

	gtk_item_factory_create_items (item_factory, nmenu_items, menu_items, NULL);

	gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);

	if (menubar)
		/* Finally, return the actual menu bar created by the item factory. */ 
		*menubar = gtk_item_factory_get_widget (item_factory, "<main>");
}

void ay_set_submenus(void)
{
	/* fill in branches */
	GtkWidget *submenuitem;

	if (!main_menu_factory)
		return; /* not a big problem, it's just too soon */

	submenuitem = gtk_item_factory_get_widget(main_menu_factory, "/Edit/Import");
	eb_import_window(submenuitem);
	SetPref("widget::import_submenuitem", submenuitem);

	submenuitem = gtk_item_factory_get_widget(main_menu_factory, "/Chat/Smileys");
	eb_smiley_window(submenuitem);
	SetPref("widget::smiley_submenuitem", submenuitem);

	submenuitem = gtk_item_factory_get_widget(main_menu_factory, "/Edit/Set profile");
	eb_profile_window(submenuitem);
	SetPref("widget::profile_submenuitem", submenuitem);

	submenuitem = gtk_item_factory_get_widget(main_menu_factory, "/Chat/Set status");
	eb_set_status_window(submenuitem);
	SetPref("widget::set_status_submenuitem", submenuitem);
}

void eb_status_window()
{
	GtkWidget *statusbox;
	GtkWidget *vbox;
	GtkWidget *label;
	GtkWidget *menubox;
	GtkWidget *menu;
	GtkWidget *hbox;
	GtkWidget *radioonline, *radiocontact, *radioaccount;
	GtkAccelGroup *accel = NULL;
	char * userrc = NULL;
	int win_x, win_y, win_w, win_h;
	int flags;

	statuswindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	accel = gtk_accel_group_new();
	gtk_window_add_accel_group( GTK_WINDOW(statuswindow),accel );
	/* The next line allows you to make the window smaller than the orig. size */
	gtk_window_set_policy(GTK_WINDOW(statuswindow), TRUE, TRUE, FALSE);
	gtk_window_set_wmclass(GTK_WINDOW(statuswindow), "main_window", "Ayttm");

	status_show = iGetLocalPref("status_show_level");

	statusbox = gtk_vbox_new(FALSE, 0);

	menubox = gtk_handle_box_new();
	gtk_handle_box_set_handle_position(GTK_HANDLE_BOX(menubox), GTK_POS_LEFT);
	gtk_handle_box_set_snap_edge(GTK_HANDLE_BOX(menubox), GTK_POS_LEFT);
	gtk_handle_box_set_shadow_type(GTK_HANDLE_BOX(menubox), GTK_SHADOW_NONE);

	userrc = g_strconcat(config_dir, G_DIR_SEPARATOR_S, "menurc", NULL);
	gtk_item_factory_parse_rc(userrc);
	g_free(userrc);

	get_main_menu(statuswindow, &menu);
	
	ay_set_submenus();

	gtk_container_add(GTK_CONTAINER(menubox), menu );
	gtk_widget_show(menu);
	gtk_box_pack_start(GTK_BOX(statusbox), menubox, FALSE, FALSE, 0 );
	gtk_widget_show(menubox);

	/*
	 * Do the main status window
	 */

	vbox = gtk_vbox_new(FALSE, 0);
	hbox = gtk_hbox_new(FALSE, 0);

	label = gtk_radio_button_new_with_label (NULL, _("Online"));
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 1);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(label),
			status_show==2);
	gtk_widget_show(label);
	radioonline = label;

	label = gtk_radio_button_new_with_label
	  (gtk_radio_button_group(GTK_RADIO_BUTTON(label)),
	   _("Contacts"));
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 1);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(label),
			status_show==1);
	gtk_widget_show(label);
	radiocontact = label;

	label = gtk_radio_button_new_with_label
	  (gtk_radio_button_group(GTK_RADIO_BUTTON(label)),
	   _("Accounts"));
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 1);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(label),
			status_show==0);
	gtk_widget_show(label);
	radioaccount = label;
	
	gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
	gtk_widget_show(hbox);

	eb_debug(DBG_CORE, "%d\n", l_list_length(accounts));
	MakeContactList();
	gtk_widget_show(contact_window);
	gtk_box_pack_start(GTK_BOX(vbox),contact_window,TRUE,TRUE,0);

	gtk_widget_show(vbox);

	gtk_box_pack_start(GTK_BOX(statusbox), vbox, TRUE, TRUE,0);

	/*
	 * Status Bar
	 */

	hbox = gtk_handle_box_new();
	gtk_handle_box_set_handle_position(GTK_HANDLE_BOX(hbox), GTK_POS_LEFT);
	gtk_handle_box_set_snap_edge(GTK_HANDLE_BOX(hbox), GTK_POS_LEFT);
	status_message = gtk_label_new(_("Welcome To Ayttm"));
	status_bar = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(status_bar), GTK_SHADOW_IN );
	gtk_widget_show(status_message);
	gtk_container_add(GTK_CONTAINER(status_bar), status_message);
	gtk_widget_show(status_bar);
	gtk_container_add(GTK_CONTAINER(hbox), status_bar);
	gtk_widget_show(hbox);
		
        gtk_box_pack_start(GTK_BOX(statusbox), hbox ,FALSE, FALSE,0);
        gtk_window_set_title(GTK_WINDOW(statuswindow), _(PACKAGE_STRING"-"RELEASE));
	
	gtk_widget_show(statusbox);

	gtk_container_add(GTK_CONTAINER(statuswindow), statusbox );

	gtk_signal_connect (GTK_OBJECT (statuswindow), "delete_event",
			    GTK_SIGNAL_FUNC (delete_event), NULL);

	gtk_widget_realize(statuswindow);
	
	gtkut_set_window_icon(statuswindow->window, NULL);
	iconlogin_pm = gdk_pixmap_create_from_xpm_d(statuswindow->window, &iconlogin_bm,
		NULL, (gchar **) login_icon_xpm);
	iconblank_pm = gdk_pixmap_create_from_xpm_d(statuswindow->window, &iconblank_bm,
		NULL, (gchar **) blank_icon_xpm);
	iconlogoff_pm = gdk_pixmap_create_from_xpm_d(statuswindow->window, &iconlogoff_bm,
		NULL, (gchar **) logoff_icon_xpm);

	/* handle geometry - ivey */
#ifndef __MINGW32__
	if (geometry[0] != 0) { 
		flags = XParseGeometry(geometry, &win_x, &win_y, &win_w, &win_h);
		gtk_window_set_position(GTK_WINDOW(statuswindow), GTK_WIN_POS_NONE); 
		gtk_widget_set_uposition(statuswindow, win_x, win_y);
		gtk_widget_set_usize(statuswindow, win_w, win_h);
	} else 
#endif
	{
		if (iGetLocalPref("x_contact_window") > 0
		&&  iGetLocalPref("y_contact_window") > 0)
			gtk_widget_set_uposition(statuswindow, 
					iGetLocalPref("x_contact_window"),
					iGetLocalPref("y_contact_window"));
	                gdk_window_move(statuswindow->window, 
					iGetLocalPref("x_contact_window"),
					iGetLocalPref("y_contact_window"));
	}	

	gtk_widget_show(statuswindow);
#ifndef __MINGW32__
	if (geometry[0] == 0)
#endif
	{
		/* Force a move */
		if (iGetLocalPref("x_contact_window") > 0
		&&  iGetLocalPref("y_contact_window") > 0) {
			/* Clear up any events which will map the window */
			/* Until the item is mapped you can not move
			   the window. */
			while(gtk_events_pending())
				gtk_main_iteration();
			/* Move only after cleanup */
	                gdk_window_move(statuswindow->window, 
					iGetLocalPref("x_contact_window"),
					iGetLocalPref("y_contact_window"));
		}
	}	
	
	update_contact_list ();

	gtk_signal_connect(GTK_OBJECT(radioonline), "clicked",
			   GTK_SIGNAL_FUNC(status_show_callback),
			   (gpointer) 2);
	gtk_signal_connect(GTK_OBJECT(radiocontact), "clicked",
			   GTK_SIGNAL_FUNC(status_show_callback),
			   (gpointer) 1);
	gtk_signal_connect(GTK_OBJECT(radioaccount), "clicked",
			   GTK_SIGNAL_FUNC(status_show_callback),
			   (gpointer) 0);
	gtk_signal_connect(GTK_OBJECT(contact_window), "size_allocate",
			eb_save_size,NULL);
	
	set_menu_sensitivity();

}


