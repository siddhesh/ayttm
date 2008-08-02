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
#include <ctype.h>

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
#include "messages.h"

#include "ayttm_tray.h"

#include "gtk/gtkutils.h"
#include "gtk/html_text_buffer.h"
#include "gtk/gtk_tree_view_tooltip.h"

#include "pixmaps/login_icon.xpm"
#include "pixmaps/blank_icon.xpm"
#include "pixmaps/logoff_icon.xpm"
#include <gdk-pixbuf/gdk-pixbuf.h>

/* define to add debug stuff to 'Help' Menu */
/*
#define	ADD_DEBUG_TO_MENU	1
*/

#define MAIN_VIEW_EXPAND_ROW(row) \
	gtk_tree_view_expand_to_path(\
			GTK_TREE_VIEW(contact_list),\
			gtk_tree_model_get_path(\
				GTK_TREE_MODEL(contact_list_store),\
				(GtkTreeIter *)row))

#define MAIN_VIEW_COLLAPSE_ROW(row) \
	gtk_tree_view_collapse_row(\
			GTK_TREE_VIEW(contact_list),\
			gtk_tree_model_get_path(\
				GTK_TREE_MODEL(contact_list_store),\
				(GtkTreeIter *)row))

/*
 * Enumerate the Main window tree view
 */
enum {
	MAIN_VIEW_ICON,
	MAIN_VIEW_LABEL,
	MAIN_VIEW_STATUS_TIP,
	MAIN_VIEW_STATUS,
	MAIN_VIEW_ROW_DATA,
	MAIN_VIEW_ROW_TYPE,
	MAIN_VIEW_HAS_TIP,
	MAIN_VIEW_COL_COUNT
};

enum {
	TARGET_TYPE_ACCOUNT=1,
	TARGET_TYPE_CONTACT,
	TARGET_TYPE_GROUP
};

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
GtkUIManager *ui_manager = NULL;
GtkWidget *statuswindow = NULL;
GtkWidget *away_menu = NULL;

static int window_title_handler = -1;

static GtkTreeStore * contact_list_store = NULL;
static GtkWidget * contact_list = NULL;
static GtkWidget * contact_window = NULL;
static GtkWidget * status_bar = 0;
static GtkToolItem * status_message = NULL;
static GtkTreeViewTooltip *tooltip = NULL;

/* status_show is:
   0 => show all accounts,
   1 => show all contacts,
   2 => show online contacts
*/
static int status_show = -1;

static time_t last_sound_played = 0;
static void eb_save_size( GtkWidget * widget, gpointer data );

void do_events(void)
{
	gtk_main_iteration_do(FALSE);
}

void set_tooltips_active(int active)
{
	if(!tooltip)
		tooltip = gtk_tree_view_tooltip_new_for_treeview(GTK_TREE_VIEW(contact_list));

	if(active)
		gtk_tree_view_tooltip_set_tip_columns(tooltip, 
				MAIN_VIEW_HAS_TIP,
				MAIN_VIEW_ICON,
				MAIN_VIEW_LABEL,
				MAIN_VIEW_STATUS_TIP );
	else
		gtk_tree_view_tooltip_set_tip_columns(tooltip, 
				-1,
				-1,
				-1,
				-1 );
}

void focus_statuswindow (void) 
{
	gdk_window_raise(statuswindow->window);
}

gboolean delete_event( GtkWidget *widget, GdkEvent *event, gpointer data )
{
	hide_status_window();

	return TRUE;
}

void ayttm_end_app_from_menu( GtkWidget *widget, GdkEvent *event, gpointer data )
{
	delete_event(widget, event, data);

	ayttm_end_app(widget, event, data);
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


static void remove_group_callback(void *d, int result)
{
	if(result != 0) {
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
		eb_do_dialog (buff, _("Delete Group"), remove_group_callback, d);
	}
}

static void remove_contact_callback(void *d, int result)
{
	if (result != 0) {
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
		eb_do_dialog (buff, _("Delete Contact"), remove_contact_callback, d);
	}
}

static void remove_account_callback(void *d, int result)
{
	if (result != 0) {
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
		eb_do_dialog (buff, _("Delete Account"), remove_account_callback, d);
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

static GtkWidget *status_message_window = NULL;
static GtkWidget *status_message_swindow = NULL;
static GtkWidget *status_message_window_text = NULL;

static void create_log_window(void)
{
	if (status_message_window != NULL)
		return;
	
	status_message_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	status_message_window_text = gtk_text_view_new();
	
	status_message_swindow = gtk_scrolled_window_new(NULL,NULL);
	
	gtk_window_set_title(GTK_WINDOW(status_message_window), _("Ayttm - history"));
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(status_message_swindow), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	
	html_text_view_init(GTK_TEXT_VIEW(status_message_window_text), HTML_IGNORE_FONT);
	
	gtk_widget_set_size_request(status_message_window_text, 450, 150);
	
	gtk_container_add(GTK_CONTAINER(status_message_swindow), status_message_window_text);
	gtk_container_add(GTK_CONTAINER(status_message_window), status_message_swindow);
	gtk_widget_show(status_message_window_text);
	gtk_widget_show(status_message_swindow);
	
	g_signal_connect(status_message_window, "delete-event", 
			G_CALLBACK(gtk_widget_hide_on_delete), NULL);
	
	gtk_widget_realize(status_message_window);
	gtk_widget_realize(status_message_swindow);
	gtk_widget_realize(status_message_window_text);
}

static char *last_inserted = NULL;

static void update_status_message(gchar * message )
{
	if (status_message_window == NULL)
		create_log_window();

	if(status_bar) {
		int i = 0;
		char *tstamp_mess = NULL;
		time_t t;
		struct tm * cur_time;
		
		for(i=strlen(message)-1; i>=0 && isspace(message[i]); i--)
			message[i]=' ';

		time(&t);
		cur_time = localtime(&t);
		tstamp_mess = g_strdup_printf("<font color=#888888><b>%d:%.2d:%.2d</b></font> %s<br>",
				cur_time->tm_hour,
				 cur_time->tm_min, 
				cur_time->tm_sec,
			 	message);
		
		for (i = 0; i < strlen(message); i++) {
        		if (isdigit(message[i])) {
                		return;
        		}
		}
		
		if (last_inserted) {
			if (!strcmp(last_inserted,message))
				return;
			else {
				free(last_inserted);
				last_inserted = strdup(message);
			}
		}
		else {
			last_inserted = strdup(message);
		}
		gtk_widget_hide(GTK_WIDGET(status_message));
		gtk_tool_button_set_label(GTK_TOOL_BUTTON(status_message), message);

		if (iGetLocalPref("do_noautoresize")) {
			gtk_widget_set_size_request(GTK_WIDGET(status_message), status_bar->allocation.width-10, -1);
		}
		gtk_widget_show(GTK_WIDGET(status_message));
		html_text_buffer_append( GTK_TEXT_VIEW(status_message_window_text),
				tstamp_mess, HTML_IGNORE_NONE );
		g_free(tstamp_mess);
	}
}

static void status_show_messages(GtkWidget *bar, void *data)
{
	gtk_widget_show(status_message_window);
	gtk_widget_show(status_message_window_text);
	gdk_window_raise(status_message_window->window);
}

static gboolean collapse_callback ( GtkTreeView * tree_view, GtkTreeIter *iter,
		GtkTreePath *path, gpointer data )
{
	gpointer d;
	struct contact *c;
	int type;


	GtkTreeModel *model = gtk_tree_view_get_model(tree_view);
	gtk_tree_model_get( model, iter,
			MAIN_VIEW_ROW_DATA, &d,
			MAIN_VIEW_ROW_TYPE, &type, -1);

	if(d && type == TARGET_TYPE_CONTACT) {
		c = (struct contact *)d;
		c->expanded = FALSE;
	}

	return FALSE;
}

static gboolean expand_callback ( GtkTreeView * tree_view, GtkTreeIter *iter,
		GtkTreePath *path, gpointer data )
{
	gpointer d;
	struct contact *c;
	int type;


	GtkTreeModel *model = gtk_tree_view_get_model(tree_view);
	
	gtk_tree_model_get( model, iter,
			MAIN_VIEW_ROW_DATA, &d,
			MAIN_VIEW_ROW_TYPE, &type, -1);


	if(d && type == TARGET_TYPE_CONTACT) {
		c = (struct contact *)d;
		c->expanded = TRUE;
	}

	return FALSE;
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
			g_signal_connect(button, "activate", G_CALLBACK(get_info),account);
			gtk_menu_shell_append(GTK_MENU_SHELL(InfoMenu), button);
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
	
	gtkut_create_menu_button (GTK_MENU(menu), _("Add contact to group"),
			G_CALLBACK(add_to_group_callback), d);

	gtkut_create_menu_button (GTK_MENU(menu), NULL, NULL, NULL);

	gtkut_create_menu_button (GTK_MENU(menu), _("Edit Group"),
			G_CALLBACK(edit_group_callback), d);

	gtkut_create_menu_button (GTK_MENU(menu), _("Delete Group"),
			G_CALLBACK(offer_remove_group_callback), d);
	
	gtkut_create_menu_button (GTK_MENU(menu), _("Sort"),
			G_CALLBACK(sort_group_callback), grp);
	
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
		if (conn) {
			ecd->contact=conn->nick;
			ecd->group=conn->group->name;
		} else if (acc) {
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
			gtk_menu_shell_append(GTK_MENU_SHELL(submenu), button);
			g_signal_connect(button, "activate",
					G_CALLBACK(eb_generic_menu_function), mid);
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
	
	gtkut_create_menu_button (GTK_MENU(menu), _("Add Account to Contact"),
			G_CALLBACK(add_account_to_contact_callback), d);

	gtkut_create_menu_button (GTK_MENU(menu), _("Edit Contact"),
			G_CALLBACK(edit_contact_callback), d);

	gtkut_create_menu_button (GTK_MENU(menu), _("Delete Contact"),
			G_CALLBACK(offer_remove_contact_callback), d);
	
	gtkut_create_menu_button (GTK_MENU(menu), NULL, NULL, NULL); /* sep */
	
	gtkut_create_menu_button (GTK_MENU(menu), _("Send File"),
			G_CALLBACK(send_file_with_contact_callback), d);
	
	gtkut_create_menu_button (GTK_MENU(menu), _("Edit Trigger"),
			G_CALLBACK(edit_trigger_callback), d);

	gtkut_create_menu_button (GTK_MENU(menu), NULL, NULL, NULL); /* sep */

	gtkut_create_menu_button (GTK_MENU(menu), _("View Log"),
			G_CALLBACK(view_log_callback), d);

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

	gtkut_create_menu_button (GTK_MENU(menu), _("Edit Account..."),
			G_CALLBACK(edit_account_callback), d);

	gtkut_create_menu_button (GTK_MENU(menu), _("Delete Account..."),
			G_CALLBACK(offer_remove_account_callback), d);

	gtkut_create_menu_button (GTK_MENU(menu), NULL, NULL, NULL); /* sep */

	if (CAN(acc, send_file))
		 gtkut_create_menu_button (GTK_MENU(menu), _("Send File..."),
			G_CALLBACK(send_file_callback), d);

	gtkut_create_menu_button (GTK_MENU(menu), _("Info..."),
			G_CALLBACK(get_info),d);

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

static gboolean right_click (GtkWidget *widget, GdkEventButton * event,
                         gpointer d)
{
	if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
		GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
		GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
		GtkTreeIter sel_iter;

		int target_type;
		gpointer data;

		GtkTreePath *path;

		if( gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget),
					event->x, event->y, &path,
					NULL, NULL, NULL) )
		{
			gtk_tree_selection_select_path(selection, path);
			gtk_tree_model_get_iter(model, &sel_iter, path);
			gtk_tree_path_free(path);
		}
		else
			return FALSE;

		gtk_tree_model_get(model, &sel_iter,
				MAIN_VIEW_ROW_DATA, &data,
				MAIN_VIEW_ROW_TYPE, &target_type,
				-1);

		if(target_type == TARGET_TYPE_CONTACT) {
			contact_menu (event, data);
		}
		if(target_type == TARGET_TYPE_ACCOUNT) {
			account_menu (event, data);
		}
		if(target_type == TARGET_TYPE_GROUP) {
			group_menu (event, data);
		}
	}

	return FALSE;
}


static void double_click (GtkTreeView *tree_view, GtkTreePath *path,
		GtkTreeViewColumn *col, gpointer data)
{
	int target_type;
	gpointer d;
	GtkTreeIter pos;

	GtkTreeModel *model = gtk_tree_view_get_model(tree_view);

	gtk_tree_model_get_iter(model, &pos, path);

	gtk_tree_model_get(model, &pos,
			MAIN_VIEW_ROW_DATA, &d,
			MAIN_VIEW_ROW_TYPE, &target_type,
			-1);

	if(target_type == TARGET_TYPE_ACCOUNT)
		eb_chat_window_display_account((eb_account *)d);
	else if(target_type == TARGET_TYPE_CONTACT)
    		eb_chat_window_display_contact((struct contact *)d);

}

/*
 * End Mouse button event handlers for elements of the group/contact/account
 * list
 */

static void add_callback(GtkWidget *widget)
{
	show_add_contact_window();
}

static void add_group_callback(GtkWidget *widget)
{
	show_add_group_window();
}

static void eb_add_accounts( GtkWidget * widget, gpointer stats )
{
	ay_edit_local_accounts();
}

static void eb_edit_accounts( GtkWidget * widget, gpointer stats )
{
	ayttm_prefs_show_window(4);
}

static void build_prefs_callback( GtkWidget * widget, gpointer stats )
{
	ayttm_prefs_show_window(0);
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
	
	if (version_cmp(version, PACKAGE_VERSION "-" RELEASE) > 0) {
		/* versions differ, should I warn ? */
		if (warn_again || !warned || strcmp(warned, version)) {
			char * buf = g_strdup_printf(_("A new release of ayttm is available.\n"
					"Latest version is %s, while you have %s.\n"), version, PACKAGE_VERSION "-" RELEASE);
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
	int is_auto = GPOINTER_TO_INT(userdata);
	char *version = ay_get_last_version();
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
		char buff[1024];
		char *sname = NULL;
		LList *l = eb_services[s->ela->service_id].sc->get_states();
		int i = 0;
		for (i = 0; i <= s->status; i++) {
			sname = (char *)l->data;
			l = l->next;
		}
		eb_debug(DBG_CORE, "Calling set_current_state: %i\n", s->status);
		eb_services[s->ela->service_id].sc->set_current_state(s->ela, s->status);
		new_state = eb_services[s->ela->service_id].sc->get_current_state(s->ela);
		snprintf(buff,1024, _("Setting %s (on %s) to %s"), s->ela->handle, get_service_name(s->ela->service_id), sname);
		update_status_message(buff);
		/* Did the state change work? */
		if(new_state != s->status)
			eb_set_active_menu_status(s->ela->status_menu, new_state);
	}
	eb_debug(DBG_CORE, "%s set to state %d.\n", eb_services[s->ela->service_id].name, s->status );
	/* in about 10 seconds try to flush contact mgmt queue */
	if (s->ela->mgmt_flush_tag == 0 
	&& (s->ela->connecting || s->ela->connected)) {
		s->ela->mgmt_flush_tag = 
			eb_timeout_add(1000 + (1000*s->ela->service_id),
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
		if (!ac->connected && (all || ac->connect_at_startup)) {
			char buff[1024];
			RUN_SERVICE(ac)->login(ac) ;
			snprintf(buff,1024,_("Setting %s (on %s) to Online"), ac->handle, get_service_name(ac->service_id));
			update_status_message(buff);
		}
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
		if (ac && ac->connected) {
			char buff[1024];

			RUN_SERVICE(ac)->logout(ac) ;
			snprintf(buff,1024,_("Setting %s (on %s) to Offline"), ac->handle, get_service_name(ac->service_id));
			update_status_message(buff);
		}
		if (ac && ac->mgmt_flush_tag) {
			eb_timeout_remove(ac->mgmt_flush_tag);
			ac->mgmt_flush_tag=0;
		}
		node = node->next ;
	}
	set_menu_sensitivity();

}

//static gint get_contact_position( struct contact * ec)
//{
//	gint i=0;
//	LList *l;
//	
//	for (l = ec->group->members; l && (l->data != ec); l=l->next)  {
//		struct contact * contact = l->data;
//		if (contact->list_item)
//			i++;
//	}
//	return i;
//}

//static gint get_account_position( eb_account * ea)
//{
//	gint i=0;
//	LList *l;
//	
//	for (l = ea->account_contact->accounts; l && (l->data != ea); l=l->next) {
//		eb_account * account = l->data;
//		if (account->list_item)
//			i++;
//	}
//	return i;
//}


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
					else if (status_show == 0) {
						MAIN_VIEW_EXPAND_ROW (con->list_item);
					}
					else
						MAIN_VIEW_COLLAPSE_ROW(con->list_item);
				} else {
					/* Close it up */
					if (ea->online) {
						buddy_update_status(ea);

						if (con->list_item != NULL)
							MAIN_VIEW_COLLAPSE_ROW(con->list_item);
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

static GdkPixbuf * iconlogin_pb = NULL;
static GdkPixbuf * iconblank_pb = NULL;
static GdkPixbuf * iconlogoff_pb = NULL;

static GtkTargetEntry drag_types[1] =
{
	{"text/plain", GTK_TARGET_SAME_WIDGET, 0}
};

/*
 * Add/Remove entries to/from box -- group, contact, account
 */

/* makes a group visible on the contact list */
void add_group_line(grouplist * eg)
{
	GtkTreeIter iter;

	/* Might call add_group() - which calls add_group_line() 
	   before contact_list exists - this is OK so just return */
	if (eg->list_item || !contact_list)
		return;
	
	gtk_tree_store_append( contact_list_store, &iter, NULL );
	gtk_tree_store_set(contact_list_store, &iter,
			MAIN_VIEW_LABEL, eg->name,
			MAIN_VIEW_ROW_DATA, eg,
			MAIN_VIEW_ROW_TYPE, TARGET_TYPE_GROUP,
			MAIN_VIEW_HAS_TIP, FALSE,
			-1);
	eg->list_item = gtk_tree_iter_copy(&iter);

	eg->contacts_online = 0;
	eg->contacts_shown = 0;
}

/* Refresh a group line after changing its label */
void update_group_line(grouplist * eg)
{
	if (!eg->list_item)
		return;

	gtk_tree_store_set(contact_list_store, eg->list_item,
			MAIN_VIEW_LABEL, eg->name,
			MAIN_VIEW_ROW_DATA, eg,
			-1);
}

static void set_status_label(eb_account *ea, int update_contact)
{
	char * c = NULL, *tmp = NULL;
	int need_tooltip_update = 0;
	
	c = g_strndup(RUN_SERVICE(ea)->get_status_string(ea), 40);

	while (strchr(c,'\n') != NULL) {
		char *t = strchr(c,'\n');
		*t = ' ';
	}

	if(strlen(c) == 40) {
		c[39] = c[38] = c[37] = '.';
	}

	tmp = g_strdup_printf("%s%s%s",
			strlen(c)?"(":"",
			c,
			strlen(c)?")":"");

	if (ea->status) {
		char *current = NULL;
		current = g_strdup(ea->status);
		eb_debug(DBG_CORE,"current %s c %s\n",current,tmp);
		if (current && strcmp(current, tmp)) {
			char buff[1024];
			g_snprintf(buff, 1024, _("%s is now %s"), 
					ea->account_contact->nick,
					strlen(c)?c:"Online");
			update_status_message(buff);
		}
	}
	
	ea->status = strdup(tmp);
	ea->tiptext = ea->status;
	
	if (update_contact) {
		struct tm *mytime;
		char buff[128];
		char *status;

		if(ea->account_contact->status)
			g_free(ea->account_contact->status);
		ea->account_contact->status = strdup(tmp);

		need_tooltip_update = (ea->account_contact->last_status && strcmp(c, ea->account_contact->last_status));
	
		status = RUN_SERVICE(ea)->get_status_string(ea);
		
		if(need_tooltip_update) {
			time(&ea->account_contact->last_status_change);
			if (ea->account_contact->last_status)
				free(ea->account_contact->last_status);
			ea->account_contact->last_status = strdup(tmp);
		}
			
		if (ea->account_contact->last_status_change != 0) {
			mytime = localtime(&ea->account_contact->last_status_change);
			strftime(buff, 128, "%H:%M (%b %d)", mytime);
			ea->tiptext = g_strdup_printf(
					_("%s\n<span size=\'small\'>Since %s</span>"),
					strlen(status)?status:"Online", buff);
		}
	}
	g_free(c);
	g_free(tmp);
	
	set_tooltips_active(iGetLocalPref("do_show_tooltips"));
}
	
/* makes an account visible on the buddy list, making the contact visible
   if necessary */
void add_account_line( eb_account * ea)
{
	char *label;
	GtkTreeIter iter;
	
	if (ea->list_item)
		return;
	
	add_contact_line(ea->account_contact);

	ea->pix = iconblank_pb;
	label = g_strdup(ea->handle);
	set_status_label(ea, TRUE);

	ea->icon_handler = -1;

	gtk_tree_store_append( contact_list_store, &iter,
			(GtkTreeIter *)(ea->account_contact->list_item) );
	gtk_tree_store_set(contact_list_store, &iter,
			MAIN_VIEW_ICON, ea->pix,
			MAIN_VIEW_LABEL, label,
			MAIN_VIEW_STATUS, ea->status,
			MAIN_VIEW_STATUS_TIP, ea->tiptext,
			MAIN_VIEW_ROW_DATA, ea,
			MAIN_VIEW_ROW_TYPE, TARGET_TYPE_ACCOUNT,
			MAIN_VIEW_HAS_TIP, FALSE,
			-1);
	ea->list_item = gtk_tree_iter_copy(&iter);
}

/* makes a contact visible on the buddy list */
void add_contact_line( struct contact * ec)
{
	GtkTreeIter *sibling = NULL;
	LList *c_iter;
	GtkTreeIter iter;

	if (ec->list_item)
		return;
	
	ec->pix = iconblank_pb;
	
	ec->label = g_strdup(ec->nick);
	ec->status = g_strdup("");

	ec->icon_handler = -1;

	for(c_iter = ec->group->members; c_iter; c_iter = l_list_next(c_iter)) {
		struct contact *c = c_iter->data;
		if( strcasecmp(ec->nick, c->nick) < 0 && c->list_item ) {
			sibling = c->list_item;
			break;
		}
	}

	gtk_tree_store_insert_before(contact_list_store, &iter, 
				     (GtkTreeIter *)ec->group->list_item,
				     sibling);

	gtk_tree_store_set(contact_list_store, &iter,
			MAIN_VIEW_ICON, ec->pix,
			MAIN_VIEW_LABEL, ec->label,
			MAIN_VIEW_STATUS, ec->status,
			MAIN_VIEW_ROW_DATA, ec,
			MAIN_VIEW_ROW_TYPE, TARGET_TYPE_CONTACT,
			MAIN_VIEW_HAS_TIP, TRUE,
			-1);
	ec->list_item = gtk_tree_iter_copy(&iter);
	
	if (!ec->group->contacts_shown) {
		if(ec->group->list_item &&
		   strcmp(_("Unknown"),ec->group->name) !=0 &&
		   strcmp(_("Ignore"),ec->group->name) !=0)
			MAIN_VIEW_EXPAND_ROW(ec->group->list_item);
	}
	
	ec->group->contacts_shown++;
	if(ec->expanded)
		MAIN_VIEW_EXPAND_ROW(ec->list_item);
	else
		MAIN_VIEW_COLLAPSE_ROW(ec->list_item);

}

/* Refresh a contact line after it's been modified */
void update_contact_line( struct contact * ec)
{
	if (!ec->list_item)
		return;
	
	gtk_tree_store_set(contact_list_store, ec->list_item,
			MAIN_VIEW_ICON, ec->pix,
			MAIN_VIEW_LABEL, ec->label,
			MAIN_VIEW_STATUS, ec->status,
			MAIN_VIEW_ROW_DATA, ec,
			MAIN_VIEW_ROW_TYPE, TARGET_TYPE_CONTACT,
			-1);
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
		
	gtk_tree_store_remove(contact_list_store, eg->list_item);
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
	gtk_tree_store_remove(GTK_TREE_STORE(contact_list_store), ec->list_item);
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

	if( ea->account_contact->list_item )
		gtk_tree_store_remove(GTK_TREE_STORE(contact_list_store),
				ea->list_item);
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

	GdkPixbuf *tmp = NULL;

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
	
	ec->label = ec->nick;

	/* set the icon if there isn't another timeout about to alter the icon */
	if (ec->icon_handler == -1) {
		tmp = GDK_PIXBUF(ec->pix);

		gtkut_set_pixbuf(ea->ela, 
			(const char **)RUN_SERVICE(ea)->get_status_pixmap(ea), 
			&tmp);

		ec->pix = tmp;

		gtk_tree_store_set(contact_list_store, ec->list_item,
				MAIN_VIEW_ICON, ec->pix,
				MAIN_VIEW_LABEL, ec->label,
				MAIN_VIEW_STATUS, ec->status,
				MAIN_VIEW_STATUS_TIP, ea->tiptext,
				-1);
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
			gtk_widget_set_size_request(contact_window,width+width3+5,height2);
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
	GdkPixbuf *tmpbuf = NULL;
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
		tmpbuf = GDK_PIXBUF(ea->pix);
		if (RUN_SERVICE(ea) && RUN_SERVICE(ea)->get_status_pixmap && ea->pix) {
			gtkut_set_pixbuf(ea->ela, 
				(const char **) RUN_SERVICE(ea)->get_status_pixmap(ea), 
				&tmpbuf);

			ea->pix = tmpbuf;

			gtk_tree_store_set(contact_list_store, ea->list_item,
					MAIN_VIEW_ICON, ea->pix,
					MAIN_VIEW_LABEL, ea->handle,
					MAIN_VIEW_STATUS, ea->status,
					MAIN_VIEW_STATUS_TIP, ea->tiptext,
					-1);
		}
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
	ec->pix = iconlogin_pb;

	/* remove any other timeouts (if a user just logged out immediately before) */
	if (ec->icon_handler != -1)
		eb_timeout_remove(ec->icon_handler);
		
	/* timeout to set the contact icon in 10 seconds */
	ec->icon_handler = eb_timeout_add(5000, (GtkFunction)set_contact_icon,
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
	g_snprintf(buff, 1024, _("%s is now Online"), ec->nick);
	update_window_title(ec);
	update_status_message(buff);
	
	gtk_tree_store_set(contact_list_store, ec->list_item,
			MAIN_VIEW_ICON, ec->pix,
			MAIN_VIEW_STATUS, ec->status,
			-1);
}

/* function called when a contact logs off */
static void contact_logoff(struct contact * ec)
{
	char buff[1024];
	/* display the "closed door" icon */
	ec->pix = iconlogoff_pb;
	ec->group->contacts_online--;
	
	/* remove any other timeouts (if the user just logged in) */
	if (ec->icon_handler != -1)
		eb_timeout_remove(ec->icon_handler);
		
	/* timeout to remove the contact from the list */
	ec->icon_handler = eb_timeout_add(5000, (GtkFunction)hide_contact,
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
	g_snprintf(buff, 1024, _("%s is now Offline"), ec->nick);
	update_window_title(ec);
	update_status_message(buff); 
	gtk_tree_store_set(contact_list_store, ec->list_item,
			MAIN_VIEW_ICON, ec->pix,
			MAIN_VIEW_STATUS, ec->status,
			-1);
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
	ea->pix = iconlogin_pb;

	/* set the timeout to remove the "open door" icon */
	if (ea->icon_handler != -1)
		eb_timeout_remove(ea->icon_handler);
	ea->icon_handler = eb_timeout_add(5000, (GtkFunction)set_account_icon,
		(gpointer) ea);
	
	/* if there is only one account (this one) logged in under the
	   parent contact, we must login the contact also */
	if (ea->account_contact->online == 1)
		contact_login(ea->account_contact);
		
	buddy_update_status(ea);

	gtk_tree_store_set(contact_list_store, ea->list_item,
			MAIN_VIEW_ICON, ea->pix,
			MAIN_VIEW_STATUS, ea->status,
			-1);
	
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
	ea->pix = iconlogoff_pb;

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
	
	gtk_tree_store_set(contact_list_store, ea->list_item,
			MAIN_VIEW_ICON, ea->pix,
			MAIN_VIEW_STATUS, ea->status,
			-1);

	/* timeout to remove the "close door" icon */
	ea->icon_handler = eb_timeout_add(5000, (GtkFunction)hide_account,
		(gpointer) ea);

}

void update_contact_window_length ()
{
  int h,w;
  h = iGetLocalPref("length_contact_window");
  w = iGetLocalPref("width_contact_window");
  if (h == 0) 
	  h = 300;
  if (w == 0)
	  w = 150;
  eb_debug(DBG_CORE, "statuswindow size: %dx%d\n",h,w);
  gtk_window_set_default_size(GTK_WINDOW(statuswindow), w, h);
}

/* DND Callbacks and supporting functions */

struct contact *drag_data = NULL;
int drag_data_type = -1;

int get_target_type(GtkTreeView *tree_view, GtkTreePath *path)
{
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(tree_view));
	GtkTreeIter iter;
	int target_type;

	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_model_get(model, &iter,
			MAIN_VIEW_ROW_TYPE, &target_type, -1);

	return target_type;
}


void drag_begin_callback
	(GtkWidget *widget, GdkDragContext *c, gpointer data)
{
	GtkTreePath *path;
	GdkPixmap *pix;
	GdkPixbuf *buf;
	GtkTreeIter iter;
	
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));

	if ( !gtk_tree_selection_get_selected(selection, &model, &iter) )
		return;

	path = gtk_tree_model_get_path(model, &iter);

	pix = gtk_tree_view_create_row_drag_icon(GTK_TREE_VIEW(widget), path);
	buf = gdk_pixbuf_get_from_drawable(NULL, pix, NULL, 0,0,0,0,-1,-1);
	gtk_drag_source_set_icon_pixbuf(widget, buf);

	gtk_tree_model_get(model, &iter,
			MAIN_VIEW_ROW_DATA, &drag_data,
			MAIN_VIEW_ROW_TYPE, &drag_data_type,
			-1);
}

void drag_motion_callback
	(GtkWidget *widget, GdkDragContext *c, guint x, guint y, guint time, gpointer data)
{
	/* TODO
	 * 1) Update the pixbuf as the drag is moved around the tree
	 */

	GtkTreePath *path;
	GdkRectangle rectangle;
	int wx, wy;
	GtkTreeSelection *selection;
	gboolean valid;

	gtk_tree_view_get_visible_rect(GTK_TREE_VIEW(widget), &rectangle);

	gtk_tree_view_widget_to_tree_coords(GTK_TREE_VIEW(widget), x, y, &wx, &wy);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
	valid = gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), x, y,
			&path, NULL, NULL, NULL);

	/*
	 * This is probably a bit crude. Any better approach to this?
	 */
	if( wy<rectangle.y+10 ) {
		gtk_tree_view_scroll_to_point(GTK_TREE_VIEW(widget), wx-10, wy-10);
	}
	else if( wy>rectangle.y+rectangle.height-10 ) {
		gtk_tree_view_scroll_to_point(GTK_TREE_VIEW(widget), wx+10, wy+10);
	}

	if( valid && get_target_type(GTK_TREE_VIEW(widget), path) == TARGET_TYPE_GROUP )
	{
		gtk_tree_selection_select_path(selection, path);
	}
	else {
		gtk_tree_selection_unselect_all(selection);
	}

	gtk_tree_path_free(path);
}


gboolean drag_drop_callback
	(GtkWidget *widget, GdkDragContext *c, guint x, guint y, guint time, gpointer data)
{
	GtkTreePath *path;
	GtkTreeIter iter;
	GtkTreeModel *model;

	grouplist *gl;
	struct contact *ec = drag_data;
	int target;

	if( !gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), x, y, &path, NULL, NULL, NULL) ) {
		gtk_drag_finish(c, FALSE, FALSE, time);
		return FALSE;
	}
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
	gtk_tree_model_get_iter(model, &iter, path);

	gtk_tree_model_get(model, &iter, 
			MAIN_VIEW_ROW_DATA, &gl,
			MAIN_VIEW_ROW_TYPE, &target,
			-1);
	
	if(gl == NULL || ec == NULL ||
			target != TARGET_TYPE_GROUP ||
			drag_data_type != TARGET_TYPE_CONTACT)
	{
		gtk_drag_finish(c, FALSE, FALSE, time);
		return FALSE;
	}
	move_contact(gl->name, ec);
	update_contact_list ();
	write_contact_list();

	drag_data = NULL;
	
	gtk_drag_finish(c, TRUE, TRUE, time);
	gtk_tree_path_free(path);
	return TRUE;
}

/* End DND Callbacks and supporting functions */

/* Generates the contact list tree (should only be called once) */
static GtkWidget* MakeContactList()
{
	LList * l1;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	
	contact_window = gtk_scrolled_window_new(NULL, NULL);
	contact_list_store = gtk_tree_store_new(
				MAIN_VIEW_COL_COUNT,
				GDK_TYPE_PIXBUF,
				G_TYPE_STRING,
				G_TYPE_STRING,
				G_TYPE_STRING,
				G_TYPE_POINTER,
				G_TYPE_INT,
				G_TYPE_BOOLEAN );

	contact_list = gtk_tree_view_new_with_model( GTK_TREE_MODEL(contact_list_store) );

	renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes(NULL, renderer,
			"pixbuf", MAIN_VIEW_ICON,
			NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(contact_list), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(NULL, renderer,
			"text", MAIN_VIEW_LABEL,
			NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(contact_list), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(NULL, renderer,
			"text", MAIN_VIEW_STATUS,
			NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(contact_list), column);

	gtk_drag_source_set(contact_list,
			GDK_BUTTON1_MASK | GDK_BUTTON2_MASK,
			drag_types, 1,
			GDK_ACTION_DEFAULT|GDK_ACTION_MOVE);
	gtk_drag_dest_set(contact_list, GTK_DEST_DEFAULT_ALL,
			  drag_types, 1,
			  GDK_ACTION_DEFAULT|GDK_ACTION_MOVE);

	g_signal_connect(contact_list, "row-activated", G_CALLBACK(double_click), NULL );
	g_signal_connect(contact_list, "button-press-event", G_CALLBACK(right_click), NULL );
	g_signal_connect(contact_list, "row-expanded", G_CALLBACK(expand_callback), NULL);
	g_signal_connect(contact_list, "row-collapsed", G_CALLBACK(collapse_callback), NULL );
	g_signal_connect(contact_list, "drag-begin", G_CALLBACK(drag_begin_callback), NULL);
	g_signal_connect(contact_list, "drag-drop", G_CALLBACK(drag_drop_callback), NULL);
	g_signal_connect(contact_list, "drag-motion", G_CALLBACK(drag_motion_callback), NULL);

	set_tooltips_active(iGetLocalPref("do_show_tooltips"));

	for (l1 = groups; l1; l1=l1->next) {
		grouplist * grp = l1->data;
		add_group_line(grp);
	}
	
	g_object_set(contact_list, "headers-visible", FALSE, NULL);
	gtk_widget_show(contact_list);
	gtk_container_add(GTK_CONTAINER(contact_window), contact_list);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(contact_window),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
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
	  gtk_menu_shell_append(GTK_MENU_SHELL(status_menu), status_menu_item);
	  gtk_widget_show(status_menu_item);

	  for(temp_list = status_label, x = 0; temp_list; x++, temp_list=temp_list->next) {
		struct acctStatus * stats = g_new0(struct acctStatus, 1);
		
		stats->ela = ela;
		stats->status= x;
		
		status_menu_item = gtk_radio_menu_item_new(group);
		group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(status_menu_item));
		widgets = l_list_append(widgets, status_menu_item);
		hbox = gtk_hbox_new(FALSE, 3);
		label = gtk_label_new((gchar*)temp_list->data);
		
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
		gtk_widget_show(label);
		gtk_widget_show(hbox);
		gtk_widget_show(status_menu_item);
		gtk_container_add(GTK_CONTAINER(status_menu_item), hbox);
		gtk_menu_shell_append(GTK_MENU_SHELL(status_menu), status_menu_item);
		g_signal_connect(status_menu_item, "activate",
				G_CALLBACK(eb_status), (gpointer) stats );
		g_signal_connect(status_menu_item, "remove",
				G_CALLBACK(eb_status_remove), (gpointer) stats );

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
	gtk_menu_shell_append(GTK_MENU_SHELL(profile_menu), label);
	gtk_widget_show(label);

	md = GetPref(EB_PROFILE_MENU);
	if(md) {
		for(list = md->menu_items; list; list  = list->next ) {
			mid=(menu_item_data *)list->data;
			label = gtk_menu_item_new_with_label(mid->label);
			gtk_menu_shell_append(GTK_MENU_SHELL(profile_menu), label);
			g_signal_connect(label, "activate",
					G_CALLBACK(eb_profile_function), mid);
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
	gtk_menu_shell_append(GTK_MENU_SHELL(smiley_menu), label);
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
			group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(items[i].label));
			if(md->active == items[i].mid)
				gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(items[i].label), TRUE);
			else
				gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(items[i].label), FALSE);
			gtk_menu_shell_append(GTK_MENU_SHELL(smiley_menu), items[i].label);
			gtk_widget_show(items[i].label);
		}
		for(--i; i>=0; i--)
			g_signal_connect(items[i].label, "activate",
					G_CALLBACK(eb_smiley_function), items[i].mid);
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
	gtk_menu_shell_append(GTK_MENU_SHELL(import_menu), label);
	gtk_widget_show(label);

	md = GetPref(EB_IMPORT_MENU);
	if(md) {
		for(list = md->menu_items; list; list  = list->next ) {
			mid=(menu_item_data *)list->data;
			eb_debug(DBG_CORE, "adding import item: %s\n", mid->label);
			label = gtk_menu_item_new_with_label(mid->label);
			gtk_menu_shell_append(GTK_MENU_SHELL(import_menu), label);
			g_signal_connect(label, "activate",
					G_CALLBACK(eb_import_function), mid);
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
	gtk_menu_shell_append(GTK_MENU_SHELL(account_menu), label); 
	gtk_widget_show(label);
	for(list = accounts; list; list  = list->next ) {
		eb_local_account *ela = (eb_local_account *)list->data;
		label = MakeStatusMenu(ela);
		gtk_menu_shell_append(GTK_MENU_SHELL(account_menu), label);
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


static char *main_menu_xml;

static GtkActionEntry action_items[] = {
	/* All the menus */
	{"Chat",	NULL,	"_Chat"		},
	{"Set status",	NULL,	"Se_t status"	},
	{"Smileys",	NULL,	"_Smileys"	},
	{"Edit",	NULL,	"_Edit"		},
	{"Import",	NULL,	"_Import"	},
	{"Set profile",	NULL,	"Set _profile"	},
	{"Help",	NULL,	"_Help"		},
	/* All the submenus */
	{"SignonAll", NULL, N_("Sign o_n all"), "<control>A", N_("Sign onto all accounts"),
		G_CALLBACK(eb_sign_on_all)},
	{"SignoffAll", NULL, N_("Sign o_ff all"), "<control>F", N_("Sign off all accounts"),
		G_CALLBACK(eb_sign_off_all)},
	{"GrpChat", NULL, N_("New group chat"), NULL, N_("Start a chat room for a group chat"),
		G_CALLBACK(launch_group_chat)},
	{"SetAway", NULL, N_("Set as _away"), NULL, N_("Set a Custom \"Away\" status"),
		G_CALLBACK(show_away_choicewindow)},
	{"Quit", NULL, N_("_Quit"),	"<control>Q", N_("Close Ayttm"),
		G_CALLBACK(ayttm_end_app_from_menu)},
	{"Prefs", NULL, N_("_Preferences"), NULL, N_("Customize your Ayttm"),
		G_CALLBACK(build_prefs_callback)},
	{"AddDelAccounts", NULL, N_("Add or _delete accounts"), NULL,
		N_("Add or Delete chat accounts"),
		G_CALLBACK(eb_add_accounts)},
	{"EditAccounts", NULL, N_("Edit _accounts"), NULL, N_("Edit your account details"),
		G_CALLBACK(eb_edit_accounts)},
	{"AddContactAccount", NULL, N_("Add a _contact account"), NULL,
		N_("Add an account for a contact"),
		G_CALLBACK(add_callback)},
	{"AddGroup", NULL, N_("Add a _group"), NULL, N_("Add a contact group"),
		G_CALLBACK(add_group_callback)},
#ifndef __MINGW32__
	{"Website", NULL, N_("_Web site"), NULL, NULL,
		G_CALLBACK(show_website)},
	{"Manual", NULL, N_("_Manual"), NULL, NULL,
		G_CALLBACK(show_manual)},
#endif
	{"About", NULL, N_("_About Ayttm"), NULL, NULL,
		G_CALLBACK(ay_show_about)},
	{"CheckRelease", NULL, N_("Check for new _release"), NULL, NULL,
		G_CALLBACK(ay_check_release)}
#if ADD_DEBUG_TO_MENU
	,
	{"DebugMenu", NULL, N_("Dump structures"), NULL, NULL,
		G_CALLBACK(ay_dump_structures_cb)}
#endif	
};

static void menu_set_sensitive(GtkUIManager *ui, const gchar *path, gboolean sensitive)
{
	GtkWidget *widget;

	if (ui == NULL)
		return;

	widget = gtk_ui_manager_get_widget(ui, path);
	if(widget == NULL) {
		eb_debug(DBG_CORE, "unknown menu entry %s\n", path);
		return;
	}
	gtk_widget_set_sensitive(widget, sensitive);
}


void set_menu_sensitivity(void)
{
	int online = connected_local_accounts();
	
	menu_set_sensitive(ui_manager, "ui/menubar/Chat/Set status", l_list_length(accounts));
	menu_set_sensitive(ui_manager, "ui/menubar/Chat/GrpChat", online);
	menu_set_sensitive(ui_manager, "ui/menubar/Chat/SetAway", online);
	menu_set_sensitive(ui_manager, "ui/menubar/Chat/SignoffAll", online);
	menu_set_sensitive(ui_manager, "ui/menubar/Chat/SignonAll",
			(online != l_list_length(accounts)));
	
	set_tray_menu_sensitive(online, l_list_length(accounts));
}

static gchar *menu_translate(const gchar *path, gpointer data)
{
	gchar *retval;

	retval = (gchar *)gettext(path);

	return retval;
}

static void menu_add_widget (GtkUIManager *ui, GtkWidget *widget, GtkContainer *container)
{
	gtk_container_add(GTK_CONTAINER(container), widget );
	gtk_widget_show(widget);
}

static void get_main_menu( GtkWidget  *window,
                    GtkWidget **menubar, GtkWidget *menubox )
{
	GError *error = NULL;
	guint n_action_items;
	GtkActionGroup *action_group;
	GtkAccelGroup *accel_group;

	main_menu_xml=g_strdup("");
	main_menu_xml = g_strconcat(
		"<ui>\
			<menubar>\
				<menu name=\"Chat\" action=\"Chat\">\
					<menu name=\"Set status\" action=\"Set status\"/>\
					<menuitem action=\"SignonAll\"/>\
					<menuitem action=\"SignoffAll\"/>\
					<separator/>\
					<menuitem action=\"GrpChat\"/>\
					<menuitem action=\"SetAway\"/>\
					<separator/>\
					<menu name=\"Smileys\" action=\"Smileys\"/>\
					<separator/>\
					<menuitem action=\"Quit\"/>\
				</menu>\
				<menu name=\"Edit\" action=\"Edit\">\
					<menuitem action=\"Prefs\"/>\
					<menuitem action=\"AddDelAccounts\"/>\
					<menuitem action=\"EditAccounts\"/>\
					<separator/>\
					<menuitem action=\"AddContactAccount\"/>\
					<menuitem action=\"AddGroup\"/>\
					<separator/>\
					<menu name=\"Import\" action=\"Import\"/>\
					<menu name=\"Set profile\" action=\"Set profile\"/>\
				</menu>\
				<menu name=\"Help\" action=\"Help\">",
#ifndef __MINGW32__
					"<menuitem action=\"Website\"/>\
					<menuitem action=\"Manual\"/>\
					<separator/>",
#endif
					"<menuitem name=\"About\" action=\"About\"/>\
					<separator/>\
					<menuitem action=\"CheckRelease\"/>",
#if ADD_DEBUG_TO_MENU
					"<separator/>\
					<menuitem action=\"DebugMenu\"/>",
#endif
				"</menu>\
			</menubar>\
		</ui>",
		NULL);

	n_action_items = G_N_ELEMENTS (action_items);

	action_group = gtk_action_group_new("MainMenuActions");
	gtk_action_group_add_actions(action_group, action_items, n_action_items, NULL );
	gtk_action_group_set_translate_func(action_group, menu_translate, NULL, NULL);
	ui_manager = gtk_ui_manager_new();
	gtk_ui_manager_insert_action_group(ui_manager, action_group, 0);
	gtk_ui_manager_add_ui_from_string(ui_manager, main_menu_xml, -1, &error);
	if(error) {
		eb_debug(DBG_CORE, "Failed to build main menu: %s\n", error->message);
		exit(1);
	}

	g_signal_connect(ui_manager, "add-widget", G_CALLBACK(menu_add_widget), menubox);
	*menubar = gtk_ui_manager_get_widget(ui_manager, "ui/menubar");
	if( (accel_group=gtk_ui_manager_get_accel_group(ui_manager)) != NULL )
		gtk_window_add_accel_group(GTK_WINDOW(window), accel_group);
	else
		eb_debug(DBG_CORE, "WARNING: No Accel Group returned by UIManager\n");
}

void ay_set_submenus(void)
{
	/* fill in branches */
	GtkWidget *submenuitem;

	if (!ui_manager)
		return; /* not a big problem, it's just too soon */

	submenuitem = gtk_ui_manager_get_widget(ui_manager, "ui/menubar/Edit/Import");

	eb_import_window(submenuitem);
	SetPref("widget::import_submenuitem", submenuitem);

	submenuitem = gtk_ui_manager_get_widget(ui_manager, "ui/menubar/Chat/Smileys");
	eb_smiley_window(submenuitem);
	SetPref("widget::smiley_submenuitem", submenuitem);

	submenuitem = gtk_ui_manager_get_widget(ui_manager, "ui/menubar/Edit/Set profile");
	eb_profile_window(submenuitem);
	SetPref("widget::profile_submenuitem", submenuitem);

	submenuitem = gtk_ui_manager_get_widget(ui_manager, "ui/menubar/Chat/Set status");
	eb_set_status_window(submenuitem);
	SetPref("widget::set_status_submenuitem", submenuitem);
}


void hide_status_window()
{
	eb_save_size(statuswindow, NULL);
	gtk_widget_hide(statuswindow);
}


void show_status_window()
{
	int win_x, win_y;
	unsigned int win_w, win_h;
	int flags;

	/* handle geometry - ivey */
#ifndef __MINGW32__
	if (geometry[0] != 0) { 
		flags = XParseGeometry(geometry, &win_x, &win_y, &win_w, &win_h);
		gtk_window_set_position(GTK_WINDOW(statuswindow), GTK_WIN_POS_NONE); 
		gtk_window_move(GTK_WINDOW(statuswindow), win_x, win_y);
		gtk_window_set_default_size(GTK_WINDOW(statuswindow), win_w, win_h);
	} else 
#endif
	{
		if (iGetLocalPref("x_contact_window") > 0
		&&  iGetLocalPref("y_contact_window") > 0)
			gtk_window_move(GTK_WINDOW(statuswindow), 
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
	                gtk_window_move(GTK_WINDOW(statuswindow), 
					iGetLocalPref("x_contact_window"),
					iGetLocalPref("y_contact_window"));
		}
	}	
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
//	char * userrc = NULL;

	statuswindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	/* The next line allows you to make the window smaller than the orig. size */
	gtk_window_set_resizable(GTK_WINDOW(statuswindow), TRUE);
	gtk_window_set_role(GTK_WINDOW(statuswindow), "ayttm_main_window");

	status_show = iGetLocalPref("status_show_level");

	statusbox = gtk_vbox_new(FALSE, 0);

	menubox = gtk_handle_box_new();
	gtk_handle_box_set_handle_position(GTK_HANDLE_BOX(menubox), GTK_POS_LEFT);
	gtk_handle_box_set_snap_edge(GTK_HANDLE_BOX(menubox), GTK_POS_LEFT);
	gtk_handle_box_set_shadow_type(GTK_HANDLE_BOX(menubox), GTK_SHADOW_NONE);

//	userrc = g_strconcat(config_dir, G_DIR_SEPARATOR_S, "menurc", NULL);
//	gtk_item_factory_parse_rc(userrc);
//	g_free(userrc);

	get_main_menu(statuswindow, &menu, menubox);
	
	ay_set_submenus();

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
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(label),
			status_show==2);
	gtk_widget_show(label);
	radioonline = label;

	label = gtk_radio_button_new_with_label_from_widget
		(GTK_RADIO_BUTTON(label), _("Contacts"));
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 1);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(label),
			status_show==1);
	gtk_widget_show(label);
	radiocontact = label;

	label = gtk_radio_button_new_with_label_from_widget
		(GTK_RADIO_BUTTON(label), _("Accounts"));
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 1);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(label),
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

	status_message = gtk_tool_button_new(NULL, _("Welcome To Ayttm"));
	
	status_bar = gtk_toolbar_new ();
	gtk_toolbar_set_orientation(GTK_TOOLBAR(status_bar), GTK_ORIENTATION_HORIZONTAL);
	gtk_toolbar_set_style(GTK_TOOLBAR(status_bar), GTK_TOOLBAR_TEXT);
	gtk_toolbar_set_show_arrow(GTK_TOOLBAR(status_bar), FALSE);
	gtk_widget_show(status_bar);
	gtk_widget_show(GTK_WIDGET(status_message));
	gtk_container_add(GTK_CONTAINER(hbox), status_bar);
	gtk_widget_show(hbox);

	gtk_widget_set_size_request(GTK_WIDGET(status_message), 
			((status_bar->allocation.width - 10>= -1)?status_bar->allocation.width - 10:-1),
			-1);
	
	create_log_window();
	
	gtk_toolbar_insert(GTK_TOOLBAR(status_bar),status_message, -1);
	g_signal_connect (status_message, "clicked", G_CALLBACK (status_show_messages), NULL);
 	
        gtk_box_pack_start(GTK_BOX(statusbox), hbox ,FALSE, FALSE,0);
        gtk_window_set_title(GTK_WINDOW(statuswindow), _(PACKAGE_STRING"-"RELEASE));
	
	gtk_widget_show(statusbox);

	gtk_container_add(GTK_CONTAINER(statuswindow), statusbox );

	g_signal_connect (statuswindow, "delete-event", G_CALLBACK (delete_event), NULL);

	gtk_widget_realize(statuswindow);
	
	iconlogin_pb = gdk_pixbuf_new_from_xpm_data( (const char **) login_icon_xpm );
	iconlogoff_pb = gdk_pixbuf_new_from_xpm_data( (const char **) logoff_icon_xpm );
	iconblank_pb = gdk_pixbuf_new_from_xpm_data( (const char **) blank_icon_xpm );

	show_status_window();
	
	update_contact_list ();

	g_signal_connect(radioonline, "clicked", G_CALLBACK(status_show_callback),
			   GINT_TO_POINTER(2));
	g_signal_connect(radiocontact, "clicked", G_CALLBACK(status_show_callback),
			   GINT_TO_POINTER(1));
	g_signal_connect(radioaccount, "clicked", G_CALLBACK(status_show_callback),
			   GINT_TO_POINTER(0));
	g_signal_connect(statuswindow, "size-allocate", G_CALLBACK(eb_save_size),
			   NULL);
	
	set_menu_sensitivity();

}


