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

#include "intl.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "service.h"
#include "gtk_globals.h"
#include "status.h"
#include "util.h"
#include "value_pair.h"
#include "prefs.h"
#include "messages.h"
#include "dialog.h"

#include "gtk/gtkutils.h"

#include "pixmaps/ok.xpm"
#include "pixmaps/cancel.xpm"
#include "pixmaps/tb_trash.xpm"
#include "pixmaps/tb_edit.xpm"
#include "pixmaps/tb_preferences.xpm"
#include "pixmaps/checkbox_on.xpm"
#include "pixmaps/checkbox_off.xpm"

enum {
	CONNECT,
	USER_NAME,
	PASSWORD,
	SERVICE_TYPE
};

typedef char *account_row[4];

static GtkWidget *account_list;
static GtkWidget *account_window = NULL;
static GtkWidget *username;
static GtkWidget *password;
static GtkWidget *service_type;
static GtkWidget *mod_button;
static GtkWidget *del_button;
static GtkWidget *connect_at_startup;
static gint selected_row = -1;
static gboolean is_open = FALSE;
static gint num_accounts = 0;

static GdkPixmap *checkboxonxpm = NULL;
static GdkPixmap *checkboxonxpmmask = NULL;
static GdkPixmap *checkboxoffxpm = NULL;
static GdkPixmap *checkboxoffxpmmask = NULL;

static void rebuild_set_status_menu(void)
{
	ay_set_submenus();
}

static void destroy(GtkWidget * widget, gpointer data)
{
	is_open = FALSE;
	num_accounts = 0;
	selected_row = -1;
}

static void read_contacts()
{
	account_row text;
	LList *node;

	if (checkboxonxpm == NULL)
		checkboxonxpm = gdk_pixmap_create_from_xpm_d(account_window->window,
						 &checkboxonxpmmask, NULL,
						 checkbox_on_xpm);
	if (checkboxoffxpm == NULL)
		checkboxoffxpm = gdk_pixmap_create_from_xpm_d(account_window->window,
						 &checkboxoffxpmmask, NULL,
						 checkbox_off_xpm);

	gtk_clist_set_column_auto_resize(GTK_CLIST(account_list), 0, TRUE);
	gtk_clist_set_column_auto_resize(GTK_CLIST(account_list), 1, TRUE);

	for (node = accounts; node; node = node->next) {
		eb_local_account *ela = node->data;
		int row = 0;
		LList *pairs = RUN_SERVICE(ela)->write_local_config(ela);

		text[CONNECT] = "";

		text[SERVICE_TYPE] = eb_services[ela->service_id].name;

		text[USER_NAME] = value_pair_get_value(pairs, "SCREEN_NAME");

		text[PASSWORD] = value_pair_get_value(pairs, "PASSWORD");

		/* gtk_clist_append copies our strings, so we don't need to */
		row = gtk_clist_append(GTK_CLIST(account_list), text);

		if (ela->connect_at_startup)
			gtk_clist_set_pixmap(GTK_CLIST(account_list), row,
					     CONNECT, checkboxonxpm,
					     checkboxonxpmmask);
		else
			gtk_clist_set_pixmap(GTK_CLIST(account_list), row,
					     CONNECT, checkboxoffxpm,
					     checkboxoffxpmmask);
		free(text[USER_NAME]);
		free(text[PASSWORD]);

		value_pair_free(pairs);
		num_accounts++;
	}
}

static void selection_unmade(GtkWidget * clist,
			     gint row,
			     gint column,
			     GdkEventButton * event, gpointer data)
{
	gtk_entry_set_text(GTK_ENTRY(username), "");
	gtk_entry_set_text(GTK_ENTRY(password), "");

}

static void selection_made(GtkWidget * clist,
			   gint row,
			   gint column,
			   GdkEventButton * event, gpointer data)
{

	gchar *entry_name;
	gchar *entry_pass;
	gchar *entry_service;
	GdkPixmap *pix = NULL;
	GdkBitmap *bmp = NULL;

	selected_row = row;

	/* Put data in selected row into the entry boxes for revision */

	gtk_clist_get_text(GTK_CLIST(clist), row, USER_NAME, &entry_name);
	gtk_clist_get_text(GTK_CLIST(clist), row, PASSWORD, &entry_pass);
	gtk_clist_get_text(GTK_CLIST(clist), row, SERVICE_TYPE,
			   &entry_service);
	gtk_clist_get_pixmap(GTK_CLIST(clist), row, CONNECT, &pix, &bmp);

	if (column == CONNECT) {
		if (pix == checkboxonxpm)
			gtk_clist_set_pixmap(GTK_CLIST(clist), row,
					     CONNECT, checkboxoffxpm,
					     checkboxoffxpmmask);
		else
			gtk_clist_set_pixmap(GTK_CLIST(clist), row,
					     CONNECT, checkboxonxpm,
					     checkboxonxpmmask);
		return;
	}


	gtk_entry_set_text(GTK_ENTRY(username), entry_name);
	gtk_entry_set_text(GTK_ENTRY(password), entry_pass);
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(service_type)->entry),
			   entry_service);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(connect_at_startup),
				     (pix == checkboxonxpm));

	gtk_widget_set_sensitive(mod_button, TRUE);
	gtk_widget_set_sensitive(del_button, TRUE);

	return;
}

static void remove_callback(GtkWidget * widget, gpointer data)
{
	if (selected_row != -1) {
		gtk_clist_remove(GTK_CLIST(account_list), selected_row);
		num_accounts--;
		selected_row = -1;
	}
}

static char *check_login_validity(char *text[])
{
	LList *services = get_service_list();
	LList *l = services;
	/* 
	 * okay, this should really be text[SERVICE_TYPE], text[USER_NAME] ...
	 * change it if you think this is confusing
	 */
	if (USER_NAME[text] == NULL || strlen(USER_NAME[text]) == 0)
		return strdup(_("You have to enter an account."));

	while (l) {
		if (!strcmp(l->data, SERVICE_TYPE[text]))
			return eb_services[get_service_id(l->data)].sc->
			    check_login(USER_NAME[text], PASSWORD[text]);
		l = l->next;
	}

	return NULL;
}

static void add_callback(GtkWidget * widget, gpointer data)
{
	char *text[4];
	char *error_message = NULL;
	int i, row;

	text[CONNECT] = "";
	text[USER_NAME] = gtk_entry_get_text(GTK_ENTRY(username));
	text[PASSWORD] = gtk_entry_get_text(GTK_ENTRY(password));
	text[SERVICE_TYPE] = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(service_type)->entry));

	error_message = check_login_validity(text);
	if (error_message) {
		char *buf = g_strdup_printf(_("This account is not a valid %s account: \n\n %s"), 
				    text[SERVICE_TYPE], error_message);
		g_free(error_message);
		ay_do_error( _("Invalid Account"), buf );
		g_free(buf);
		return;
	}

	if( !can_multiaccount( eb_services[ get_service_id( text[SERVICE_TYPE] ) ] ) ) {
		for (i = 0; i < num_accounts; i++) {
			char *service;
			gtk_clist_get_text(GTK_CLIST(account_list), i, SERVICE_TYPE, &service);
			if (!strcmp(service, text[SERVICE_TYPE])) {
				char *buf = g_strdup_printf(_("You already have an account for %s service.\n\n"
						    "Multiple accounts per service aren't supported yet."),
					 	   text[SERVICE_TYPE]);
				ay_do_error( _("Invalid Account"), buf );
				g_free(buf);
				return;
			}
		}
	}

	row = gtk_clist_append(GTK_CLIST(account_list), text);
	if (gtk_toggle_button_get_active
	    (GTK_TOGGLE_BUTTON(connect_at_startup)))
		gtk_clist_set_pixmap(GTK_CLIST(account_list), row, CONNECT,
				     checkboxonxpm, checkboxonxpmmask);
	else
		gtk_clist_set_pixmap(GTK_CLIST(account_list), row, CONNECT,
				     checkboxoffxpm, checkboxoffxpmmask);

	num_accounts++;
	eb_debug(DBG_CORE, "num_accounts %d\n", num_accounts);
	gtk_entry_set_text(GTK_ENTRY(username), "");
	gtk_entry_set_text(GTK_ENTRY(password), "");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(connect_at_startup),
				     FALSE);
}

static void modify_callback(GtkWidget * widget, gpointer data)
{
	char *text[4];
	char *error_message = NULL;
	int i;

	text[CONNECT] = "";
	text[USER_NAME] = gtk_entry_get_text(GTK_ENTRY(username));
	text[PASSWORD] = gtk_entry_get_text(GTK_ENTRY(password));
	text[SERVICE_TYPE] = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(service_type)->entry));

	error_message = check_login_validity(text);
	if (error_message) {
		char *buf = g_strdup_printf(_("This account is not a valid %s account: \n\n %s"), 
				    text[SERVICE_TYPE], error_message);
		g_free(error_message);
		ay_do_error( _("Invalid Account"), buf );
		g_free(buf);
		return;
	}

	for (i = 0; i < num_accounts; i++) {
		char *service;
		gtk_clist_get_text(GTK_CLIST(account_list), i, SERVICE_TYPE, &service);
		if (i != selected_row && !strcmp(service, text[SERVICE_TYPE])
		&& !can_multiaccount(eb_services[ get_service_id( text[SERVICE_TYPE] ) ] )) {
			char *buf = g_strdup_printf(_("You already have an account for %s service. \n\n"
						    "Multiple accounts per service aren't supported yet."), 
					    text[SERVICE_TYPE]);
			ay_do_error( _("Invalid Account"), buf );
			g_free(buf);
			return;
		}
	}

	/* update selected row in list */

	if (gtk_toggle_button_get_active
	    (GTK_TOGGLE_BUTTON(connect_at_startup)))
		gtk_clist_set_pixmap(GTK_CLIST(account_list), selected_row,
				     CONNECT, checkboxonxpm,
				     checkboxonxpmmask);
	else
		gtk_clist_set_pixmap(GTK_CLIST(account_list), selected_row,
				     CONNECT, checkboxoffxpm,
				     checkboxoffxpmmask);

	gtk_clist_set_text(GTK_CLIST(account_list),
			   selected_row, USER_NAME, text[USER_NAME]);

	gtk_clist_set_text(GTK_CLIST(account_list),
			   selected_row, PASSWORD, text[PASSWORD]);

	gtk_clist_set_text(GTK_CLIST(account_list),
			   selected_row, SERVICE_TYPE, text[SERVICE_TYPE]);

	/* reset the entry fields */

	gtk_entry_set_text(GTK_ENTRY(username), "");
	gtk_entry_set_text(GTK_ENTRY(password), "");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(connect_at_startup),
				     FALSE);
}

static void cancel_callback(GtkWidget * widget, gpointer data)
{
	gtk_widget_destroy(account_window);
	if (!statuswindow)
		gtk_main_quit();
}

static void ok_callback(GtkWidget * widget, gpointer data)
{
	FILE *fp;
	char buff[1024];
	char *service, *user, *pass;
	int i;
	int id;
	gboolean had_accounts = (accounts != NULL);
	LList *pairs = NULL;
	LList *existing_accounts = NULL, *new_accounts = NULL,
	    *acc_walk = NULL, *to_remove = NULL;
	eb_local_account *ela = NULL;

	if (gtk_entry_get_text(GTK_ENTRY(username)) != NULL 
			&& strlen(gtk_entry_get_text(GTK_ENTRY(username))) > 0 
			&& num_accounts == 0) {
		add_callback(widget, data);
	}

	g_snprintf(buff, 1024, "%saccounts", config_dir);

	fp = fdopen(creat(buff, 0700), "w");

	if (num_accounts == 0) {
		ay_do_error( _("Invalid Account"), _("You didn't define an account.") );
		return;
	}

	eb_sign_off_all();

	for (i = 0; i < num_accounts; i++) {
		int tmp_connect = 0;
		GdkPixmap *pix;
		GdkBitmap *bmp;

		gtk_clist_get_text(GTK_CLIST(account_list), i,
				   SERVICE_TYPE, &service);
		gtk_clist_get_text(GTK_CLIST(account_list), i, USER_NAME,
				   &user);
		gtk_clist_get_text(GTK_CLIST(account_list), i, PASSWORD,
				   &pass);

		gtk_clist_get_pixmap(GTK_CLIST(account_list), i, CONNECT,
				     &pix, &bmp);
		if (pix == checkboxonxpm)
			tmp_connect = 1;

		id = get_service_id(service);
		if (accounts
		    && (ela = find_local_account_by_handle(user, id))) {
			/* If the account exists, just 
			   update the password and logout
			 */
			LList *config = NULL;
			config = eb_services[id].sc->write_local_config(ela);
			config = value_pair_remove(config, "PASSWORD");
			config = value_pair_add(config, "PASSWORD", pass);
			config = value_pair_remove(config, "CONNECT");
			config = value_pair_add(config, "CONNECT",
					   tmp_connect ? "1" : "0");
			fprintf(fp, "<ACCOUNT %s>\n", service);
			value_pair_print_values(config, fp, 1);
			fprintf(fp, "</ACCOUNT>\n");
			existing_accounts = l_list_append(existing_accounts, ela);
		} else {
			LList *config = NULL;
			eb_debug(DBG_CORE,
				 "Adding new account %s service %s\n",
				 user, service);
			pairs = value_pair_add(NULL, "SCREEN_NAME", user);
			pairs = value_pair_add(pairs, "PASSWORD", pass);
			save_account_info(service, pairs);
			ela = eb_services[id].sc->
			    read_local_account_config(pairs);
			//prevent segfault
			if (ela != NULL) {
				// Is this an account for which a module is not loaded?
				if (ela->service_id == -1)
					ela->service_id = id;
				new_accounts = l_list_append(new_accounts, ela);
				config = eb_services[id].sc->
				    write_local_config(ela);
				config = value_pair_remove(config, "CONNECT");
				config = value_pair_add(config, "CONNECT",
						   tmp_connect ? "1" :
						   "0");

				fprintf(fp, "<ACCOUNT %s>\n", service);
				value_pair_print_values(config, fp, 1);
				fprintf(fp, "</ACCOUNT>\n");
			} else
				ay_do_error( _("Invalid Service"), _("Can't add account : unknown service") );
		}
	}


	fclose(fp);

	acc_walk = accounts;
	if (acc_walk) {
		while (acc_walk != NULL && acc_walk->data != NULL) {
			if (!l_list_find
			    (existing_accounts, acc_walk->data)) {
				eb_local_account *removed = (eb_local_account *) (acc_walk->data);
				/* removed account */
				if (removed && removed->connected
				    && RUN_SERVICE(removed)->logout != NULL)
					RUN_SERVICE(removed)->
					    logout(removed);
				to_remove = l_list_append(to_remove, acc_walk->data);
			}
			acc_walk = acc_walk->next;
		}
		for (acc_walk = to_remove; acc_walk && acc_walk->data;
		     acc_walk = acc_walk->next)
			accounts = l_list_remove(accounts, acc_walk->data);
		l_list_free(to_remove);
	}

	acc_walk = new_accounts;
	if (acc_walk) {
		while (acc_walk != NULL) {
			accounts = l_list_append(accounts, acc_walk->data);
			acc_walk = acc_walk->next;
		}
	}

	gtk_widget_destroy(account_window);

	/* if this was an initial launch, start up EB */
	if (!had_accounts) {
		load_accounts();
		load_contacts();
		eb_status_window();
	} else {
		load_accounts();
		rebuild_set_status_menu();
	}
}
		
void	ay_edit_local_accounts( void )
{
	char *text[] = { _("C"),
		_("Screen Name"),
		_("Password"),
		_("Service")
	};
	GtkWidget *box;
	GtkWidget *window_box;
	GtkWidget *hbox;
	GtkWidget *button_box;
	GtkWidget *label;
	GtkWidget *iconwid;
	GtkWidget *toolbar;
	GtkWidget *toolitem;
	GtkWidget *separator;
	GList *list;
	GdkPixmap *icon;
	GdkBitmap *mask;

	if (is_open)
		return;

	is_open = 1;

	account_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_position(GTK_WINDOW(account_window),
				GTK_WIN_POS_MOUSE);
	gtk_widget_realize(account_window);
	account_list = gtk_clist_new_with_titles(4, text);
	gtk_clist_set_column_visibility(GTK_CLIST(account_list), PASSWORD,
					FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(account_window), 5);
	gtk_signal_connect(GTK_OBJECT(account_list), "select_row",
			   GTK_SIGNAL_FUNC(selection_made), NULL);
	gtk_signal_connect(GTK_OBJECT(account_list), "unselect_row",
			   GTK_SIGNAL_FUNC(selection_unmade), NULL);

	box = gtk_vbox_new(FALSE, 0);
	window_box = gtk_vbox_new(FALSE, 5);
	hbox = gtk_hbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);

	/*Screen Name Section */

	label = gtk_label_new(_("Screen Name:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 5);
	gtk_widget_show(label);
	username = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(box), username, FALSE, FALSE, 2);
	gtk_widget_show(username);

	/*Password Section */

	label = gtk_label_new(_("Password:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 5);
	gtk_widget_show(label);
	password = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(password), FALSE);
	gtk_box_pack_start(GTK_BOX(box), password, FALSE, FALSE, 2);
	gtk_widget_show(password);

	/*Service Type Section */

	label = gtk_label_new(_("Service Type:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 5);
	gtk_widget_show(label);
	service_type = gtk_combo_new();
	list = llist_to_glist(get_service_list(), 1);
	gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(service_type)->entry),
			       FALSE);
	gtk_combo_set_popdown_strings(GTK_COMBO(service_type), list);
	g_list_free(list);
	gtk_widget_show(service_type);
	gtk_box_pack_start(GTK_BOX(box), service_type, FALSE, FALSE, 2);

	/*Connect at startup Section */

	connect_at_startup = gtk_check_button_new_with_label(_("Connect at startup"));
	gtk_widget_show(connect_at_startup);
	gtk_box_pack_start(GTK_BOX(box), connect_at_startup, FALSE, FALSE,
			   5);

	gtk_box_pack_start(GTK_BOX(hbox), box, FALSE, FALSE, 2);
	gtk_widget_show(box);

	box = gtk_vbox_new(FALSE, 0);

	read_contacts();

	gtk_box_pack_start(GTK_BOX(box), account_list, TRUE, TRUE, 0);
	gtk_widget_show(account_list);

	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 2);
	gtk_widget_show(box);

	gtk_box_pack_start(GTK_BOX(window_box), hbox, TRUE, TRUE, 0);
	gtk_widget_show(hbox);

	separator = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(window_box), separator, TRUE, TRUE, 0);
	gtk_widget_show(separator);

	/*Initialize Toolbar */

	toolbar = gtk_toolbar_new(GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_BOTH);
	gtk_toolbar_set_button_relief(GTK_TOOLBAR(toolbar),
				      GTK_RELIEF_NONE);
	gtk_container_set_border_width(GTK_CONTAINER(toolbar), 0);
	gtk_toolbar_set_space_size(GTK_TOOLBAR(toolbar), 5);

	/*Add Button */

	icon = gdk_pixmap_create_from_xpm_d(account_window->window, &mask,
					 NULL, tb_preferences_xpm);
	iconwid = gtk_pixmap_new(icon, mask);
	gtk_widget_show(iconwid);
	toolitem = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					   _("Add"),
					   _("Add Account"),
					   _("Add"),
					   iconwid,
					   GTK_SIGNAL_FUNC(add_callback),
					   NULL);
	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));

	/*Delete Button */

	icon = gdk_pixmap_create_from_xpm_d(account_window->window, &mask,
					 NULL, tb_trash_xpm);
	iconwid = gtk_pixmap_new(icon, mask);
	gtk_widget_show(iconwid);
	del_button = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					     _("Delete"),
					     _("Delete Account"),
					     _("Delete"),
					     iconwid,
					     GTK_SIGNAL_FUNC
					     (remove_callback), NULL);
	gtk_widget_set_sensitive(del_button, FALSE);
	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));

	/* Modify Button */

	icon = gdk_pixmap_create_from_xpm_d(account_window->window, &mask,
					 NULL, tb_edit_xpm);
	iconwid = gtk_pixmap_new(icon, mask);
	gtk_widget_show(iconwid);
	mod_button = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					     _("Modify"),
					     _("Modify Account"),
					     _("Modify"),
					     iconwid,
					     GTK_SIGNAL_FUNC
					     (modify_callback), NULL);
	gtk_widget_set_sensitive(mod_button, FALSE);
	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));

	separator = gtk_vseparator_new();
	gtk_widget_set_usize(GTK_WIDGET(separator), 0, 20);
	gtk_toolbar_append_widget(GTK_TOOLBAR(toolbar), separator, NULL,
				  NULL);
	gtk_widget_show(separator);
	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));

	/*Okay Button */

	icon = gdk_pixmap_create_from_xpm_d(account_window->window, &mask,
					 NULL, ok_xpm);
	iconwid = gtk_pixmap_new(icon, mask);
	gtk_widget_show(iconwid);
	toolitem = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					   _("Ok"),
					   _("Ok"),
					   _("Ok"),
					   iconwid,
					   GTK_SIGNAL_FUNC(ok_callback),
					   NULL);
	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));

	/*Cancel Button */

	icon = gdk_pixmap_create_from_xpm_d(account_window->window, &mask,
					 NULL, cancel_xpm);
	iconwid = gtk_pixmap_new(icon, mask);
	gtk_widget_show(iconwid);
	toolitem = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					   _("Cancel"),
					   _("Cancel"),
					   _("Cancel"),
					   iconwid,
					   GTK_SIGNAL_FUNC
					   (cancel_callback), NULL);

	/*Buttons End */

	button_box = gtk_hbox_new(FALSE, 0);

	gtk_box_pack_end(GTK_BOX(button_box), toolbar, FALSE, FALSE, 0);
	gtk_widget_show(toolbar);

	gtk_box_pack_start(GTK_BOX(window_box), button_box, FALSE, FALSE,
			   0);
	gtk_widget_show(button_box);

	gtk_widget_show(window_box);

	gtk_container_add(GTK_CONTAINER(account_window), window_box);
   
	gtk_window_set_title(GTK_WINDOW(account_window), _("Ayttm Account Editor"));
	gtkut_set_window_icon(account_window->window, NULL);

	gtk_signal_connect(GTK_OBJECT(account_window), "destroy",
			   GTK_SIGNAL_FUNC(destroy), NULL);
	gtk_widget_show(account_window);
	gtk_widget_grab_focus(username);
}
