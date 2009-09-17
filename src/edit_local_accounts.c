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
#include "add_contact_window.h"
#include "mem_util.h"

enum {
	CONNECT,
	USER_NAME,
	PASSWORD,
	SERVICE_TYPE,
	COL_COUNT
};

static GtkWidget *account_list;
static GtkListStore *account_list_store;
static GtkWidget *account_window = NULL;
static GtkWidget *username;
static GtkWidget *password;
static GtkWidget *service_type;
static GtkToolItem *mod_button;
static GtkToolItem *del_button;
static GtkWidget *connect_at_startup;
static GtkTreeIter selected_row;
static gboolean is_open = FALSE;
static gint num_accounts = 0;

static void rebuild_set_status_menu(void)
{
	ay_set_submenus();
}

static void destroy(GtkWidget *widget, gpointer data)
{
	is_open = FALSE;
	num_accounts = 0;
}

static void read_contacts()
{
	LList *node;
	GtkTreeIter insert;

	for (node = accounts; node; node = node->next) {
		eb_local_account *ela = node->data;

		LList *pairs = RUN_SERVICE(ela)->write_local_config(ela);

		gtk_list_store_append(account_list_store, &insert);
		gtk_list_store_set(account_list_store, &insert,
			SERVICE_TYPE, eb_services[ela->service_id].name,
			USER_NAME, value_pair_get_value(pairs, "SCREEN_NAME"),
			PASSWORD, value_pair_get_value(pairs, "PASSWORD"), -1);

		if (ela->connect_at_startup)
			gtk_list_store_set(account_list_store, &insert,
				CONNECT, TRUE, -1);
		else
			gtk_list_store_set(account_list_store, &insert,
				CONNECT, FALSE, -1);

		value_pair_free(pairs);
		num_accounts++;
	}
}

/* List Selection Callback */
static gboolean selection_made_callback(GtkTreeSelection *selection,
	gpointer data)
{
	gchar *entry_name;
	gchar *entry_pass;
	gchar *entry_service;
	gboolean autoconnect;
	int x, y;
	GtkTreeViewColumn *column = NULL;
	GtkTreeIter sel;
	GList *path_list;

	gint sel_count = gtk_tree_selection_count_selected_rows(selection);

	if (sel_count < 1 || sel_count > 1) {
		gtk_entry_set_text(GTK_ENTRY(username), "");
		gtk_entry_set_text(GTK_ENTRY(password), "");

		if (sel_count > 1) {
			gtk_widget_set_sensitive(GTK_WIDGET(mod_button), FALSE);
			gtk_widget_set_sensitive(GTK_WIDGET(del_button), TRUE);
			gtk_combo_box_set_active(GTK_COMBO_BOX(service_type),
				-1);
		} else {
			gtk_widget_set_sensitive(GTK_WIDGET(mod_button), FALSE);
			gtk_widget_set_sensitive(GTK_WIDGET(del_button), FALSE);
			gtk_combo_box_set_active(GTK_COMBO_BOX(service_type),
				-1);
		}

		return FALSE;
	}

	path_list = gtk_tree_selection_get_selected_rows(selection, NULL);

	gtk_tree_model_get_iter(GTK_TREE_MODEL(account_list_store), &sel,
		path_list->data);

	g_list_foreach(path_list, (GFunc) gtk_tree_path_free, NULL);
	g_list_free(path_list);

	selected_row = sel;

	gtk_widget_get_pointer(account_list, &x, &y);
	gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(account_list),
		x, y, NULL, &column, NULL, NULL);

	if (column && !strcmp(gtk_tree_view_column_get_title(column), _("C"))) {
		gboolean autoconnect;

		gtk_tree_model_get(GTK_TREE_MODEL(account_list_store),
			&selected_row, CONNECT, &autoconnect, -1);

		gtk_list_store_set(account_list_store, &selected_row,
			CONNECT, !autoconnect);
	}

	gtk_tree_model_get(GTK_TREE_MODEL(account_list_store), &sel,
		CONNECT, &autoconnect,
		USER_NAME, &entry_name,
		PASSWORD, &entry_pass, SERVICE_TYPE, &entry_service, -1);

	gtk_entry_set_text(GTK_ENTRY(username), entry_name);
	gtk_entry_set_text(GTK_ENTRY(password), entry_pass);
	{
		int i;
		LList *l, *list = get_service_list();
		for (l = list, i = 0; l; l = l_list_next(l), i++) {
			char *name = l->data;
			if (!strcmp(name, entry_service)) {
				gtk_combo_box_set_active(GTK_COMBO_BOX
					(service_type), i);
				break;
			}
		}
		l_list_free(list);

	}
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(connect_at_startup),
		autoconnect);

	gtk_widget_set_sensitive(GTK_WIDGET(mod_button), TRUE);
	gtk_widget_set_sensitive(GTK_WIDGET(del_button), TRUE);

	return FALSE;
}

/* ForEach function. Removes each row in a single/multiple delete */
void remove_each(GtkTreeRowReference *reference, gpointer data)
{
	GtkTreeIter iter;
	GtkTreePath *path = gtk_tree_row_reference_get_path(reference);
	gtk_tree_model_get_iter(GTK_TREE_MODEL(account_list_store), &iter,
		path);
	gtk_list_store_remove(account_list_store, &iter);
	num_accounts--;
}

/* ForEach function. Adds corresponding GtkTreeRowReference to a GList for a GtkTreePath */
void path_to_row_reference(GtkTreePath *path, GList **reference)
{
	*reference =
		g_list_append(*reference,
		gtk_tree_row_reference_new(GTK_TREE_MODEL(account_list_store),
			path));
}

/* Callback for Delete button */
static void remove_callback(GtkWidget *widget, gpointer data)
{
	GList *reference_list = NULL;
	GtkTreeSelection *selection =
		gtk_tree_view_get_selection(GTK_TREE_VIEW(account_list));

	GList *path_list =
		gtk_tree_selection_get_selected_rows(selection, NULL);

	g_list_foreach(path_list, (GFunc) path_to_row_reference,
		&reference_list);
	g_list_foreach(reference_list, (GFunc) remove_each, NULL);

	g_list_foreach(reference_list, (GFunc) gtk_tree_row_reference_free,
		NULL);
	g_list_foreach(path_list, (GFunc) gtk_tree_path_free, NULL);
	g_list_free(path_list);
	g_list_free(reference_list);
}

static char *check_login_validity(const gchar *text[])
{
	LList *services = get_service_list();
	LList *l = services;

	if (text[USER_NAME] == NULL || strlen(text[USER_NAME]) == 0)
		return strdup(_("Please enter an account name."));

	if (text[SERVICE_TYPE] == NULL || strlen(text[SERVICE_TYPE]) == 0)
		return strdup(_("Please select an account type."));

	while (l) {
		if (!strcmp(l->data, text[SERVICE_TYPE]))
			return eb_services[get_service_id(l->data)].
				sc->check_login(text[USER_NAME],
				text[PASSWORD]);
		l = l->next;
	}

	return NULL;
}

/* Callback for Help Button */
static void help_callback(GtkWidget *widget, gpointer data)
{
	const char *msg =
		_("<b><big>How to create and register accounts:</big></b>\n\n"
		"<b>AIM, ICQ and Yahoo: </b>Use your screenname. "
		"You have to register your account via website.\n\n"
		"<b>MSN: </b>Use your complete login (user@host.com). "
		"You have to register your account via website.\n\n"
		"<b>IRC: </b>Use <i>login@server.com</i> in order to "
		"connect as <i>login</i> to <i>server.com</i>. You have "
		"to type in a password if the account is reserved on the "
		"server.\n\n"
		"<b>Jabber: </b>Use <i>login@server.com</i> in order to "
		"login as <i>login</i> to <i>server.com</i>. If the "
		"account does not exist, you'll be asked whether you want "
		"to register.\n\n"
		"<b>SMTP: </b>Use the email address you want as From address. "
		"Set the server to use from the <i>Preferences</i> menu. "
		"Passwords are not supported yet.");

	ay_do_info(_("Help"), msg);

/*		_("How to create and register accounts:\n"
			"- for AIM, ICQ and Yahoo: Use your screenname. "
			"You have to register your account via website.\n"
			"- for MSN: Use your complete login (user@host.com). "
			"You have to register your account via website.\n"
			"- for IRC: Use \"login@server.com\" in order to "
			"connect as login to server.com. You have to type "
			"in a password if the account is reserved on the "
			"server.\n"
			"- for Jabber: Use \"login@server.com\" in order to "
			"connect as login to server.com. If the account does "
			"not exist, you'll be asked whether you want to "
			"register.\n"
			"- for SMTP: Use the email address you want as From "
			"address. Set the server to use in prefs. Password "
			"isn't supported yet."));*/
}

/* ForEach function. Checks if the new account service has already been added */
gboolean service_exists = FALSE;
gboolean find_existing_account_mod(GtkTreeModel *model, GtkTreePath *path,
	GtkTreeIter *iter, gpointer data)
{
	gpointer serv;

	GtkTreeSelection *selection =
		gtk_tree_view_get_selection(GTK_TREE_VIEW(account_list));

	gtk_tree_model_get(model, iter, SERVICE_TYPE, &serv, -1);

	if (!gtk_tree_selection_path_is_selected(selection, path)
		&& !strcasecmp(serv, data)
		&& !can_multiaccount(eb_services[get_service_id(data)])) {
		return (service_exists = TRUE);
	}

	return (service_exists = FALSE);
}

/* ForEach function. Checks if the modified account service except the selected one already exists */
gboolean find_existing_account_add(GtkTreeModel *model, GtkTreePath *path,
	GtkTreeIter *iter, gpointer data)
{
	gpointer serv;
	gtk_tree_model_get(model, iter, SERVICE_TYPE, &serv, -1);

	if (!strcasecmp(serv, data)) {
		return (service_exists = TRUE);
	}

	return (service_exists = FALSE);
}

/* Callback for Add Button */
static void add_callback(GtkWidget *widget, gpointer data)
{
	const gchar *text[4];
	char *error_message = NULL;
	GtkTreeIter insert;

	text[CONNECT] = "";
	text[USER_NAME] = gtk_entry_get_text(GTK_ENTRY(username));
	text[PASSWORD] = gtk_entry_get_text(GTK_ENTRY(password));
	text[SERVICE_TYPE] =
		gtk_combo_box_get_active_text(GTK_COMBO_BOX(service_type));

	error_message = check_login_validity(text);
	if (error_message) {
		char *buf =
			g_strdup_printf(_
			("This account is not a valid %s account: \n\n %s"),
			(text[SERVICE_TYPE] ? text[SERVICE_TYPE] : ""),
			error_message);
		g_free(error_message);
		ay_do_error(_("Invalid Account"), buf);
		g_free(buf);
		return;
	}

	if (!can_multiaccount(eb_services[get_service_id(text[SERVICE_TYPE])])) {
		service_exists = FALSE;
		gtk_tree_model_foreach(GTK_TREE_MODEL(account_list_store),
			find_existing_account_add, (gchar *)text[SERVICE_TYPE]);

		if (service_exists) {
			char *buf =
				g_strdup_printf(_
				("You already have an account for %s service.\n\n"
					"Multiple accounts on this service aren't supported yet."),
				text[SERVICE_TYPE]);
			ay_do_error(_("Invalid Account"), buf);
			g_free(buf);
			return;
		}
	}

	gtk_list_store_append(account_list_store, &insert);
	gtk_list_store_set(account_list_store, &insert,
		CONNECT,
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON
			(connect_at_startup)), SERVICE_TYPE, text[SERVICE_TYPE],
		USER_NAME, text[USER_NAME], PASSWORD, text[PASSWORD], -1);

	num_accounts++;
	eb_debug(DBG_CORE, "num_accounts %d\n", num_accounts);
	gtk_entry_set_text(GTK_ENTRY(username), "");
	gtk_entry_set_text(GTK_ENTRY(password), "");
	gtk_combo_box_set_active(GTK_COMBO_BOX(service_type), -1);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(connect_at_startup),
		FALSE);
}

/* Callback for Modify button */
static void modify_callback(GtkWidget *widget, gpointer data)
{
	const gchar *text[4];
	char *error_message = NULL;

	text[CONNECT] = "";
	text[USER_NAME] = gtk_entry_get_text(GTK_ENTRY(username));
	text[PASSWORD] = gtk_entry_get_text(GTK_ENTRY(password));
	text[SERVICE_TYPE] =
		gtk_combo_box_get_active_text(GTK_COMBO_BOX(service_type));

	error_message = check_login_validity(text);
	if (error_message) {
		char *buf =
			g_strdup_printf(_
			("This account is not a valid %s account: \n\n %s"),
			text[SERVICE_TYPE], error_message);
		g_free(error_message);
		ay_do_error(_("Invalid Account"), buf);
		g_free(buf);
		return;
	}

	service_exists = FALSE;
	gtk_tree_model_foreach(GTK_TREE_MODEL(account_list_store),
		find_existing_account_mod, (gchar *)text[SERVICE_TYPE]);
	if (service_exists) {
		char *buf =
			g_strdup_printf(_
			("You already have an account for %s service.\n\n"
				"Multiple accounts on this service aren't supported yet."),
			text[SERVICE_TYPE]);
		ay_do_error(_("Invalid Account"), buf);
		g_free(buf);
		return;
	}

	/* update selected row in list */

	gtk_list_store_set(account_list_store, &selected_row,
		CONNECT,
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON
			(connect_at_startup)), SERVICE_TYPE, text[SERVICE_TYPE],
		USER_NAME, text[USER_NAME], PASSWORD, text[PASSWORD], -1);

	/* reset the entry fields */

	gtk_entry_set_text(GTK_ENTRY(username), "");
	gtk_entry_set_text(GTK_ENTRY(password), "");
	gtk_combo_box_set_active(GTK_COMBO_BOX(service_type), -1);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(connect_at_startup),
		FALSE);
}

/* Callback for Cancel button */
static void cancel_callback(GtkWidget *widget, gpointer data)
{
	gtk_widget_destroy(account_window);
	if (!statuswindow)
		gtk_main_quit();
}

/* ForEach Function. Saves details of an account to a file */
LList *pairs = NULL;
LList *existing_accounts = NULL, *new_accounts = NULL;
eb_local_account *ela = NULL;
static gboolean save_accounts(GtkTreeModel *model, GtkTreePath *path,
	GtkTreeIter *iter, gpointer data)
{
	FILE *fp = data;

	int tmp_connect = 0;
	char *service, *user, *pass;
	int id;

	gtk_tree_model_get(GTK_TREE_MODEL(account_list_store), iter,
		CONNECT, &tmp_connect,
		SERVICE_TYPE, &service, USER_NAME, &user, PASSWORD, &pass, -1);

	id = get_service_id(service);
	if (accounts && (ela = find_local_account_by_handle(user, id))) {
		LList *config = NULL;
		config = eb_services[id].sc->write_local_config(ela);
		config = value_pair_remove(config, "SCREEN_NAME");
		config = value_pair_add(config, "SCREEN_NAME", user);
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
			"Adding new account %s service %s\n", user, service);
		pairs = value_pair_add(NULL, "SCREEN_NAME", user);
		pairs = value_pair_add(pairs, "PASSWORD", pass);
		save_account_info(service, pairs);
		ela = eb_services[id].sc->read_local_account_config(pairs);
		//prevent segfault
		if (ela != NULL) {
			// Is this an account for which a module is not loaded?
			if (ela->service_id == -1)
				ela->service_id = id;
			new_accounts = l_list_append(new_accounts, ela);
			config = eb_services[id].sc->write_local_config(ela);
			config = value_pair_remove(config, "CONNECT");
			config = value_pair_add(config, "CONNECT",
				tmp_connect ? "1" : "0");

			fprintf(fp, "<ACCOUNT %s>\n", service);
			value_pair_print_values(config, fp, 1);
			fprintf(fp, "</ACCOUNT>\n");
		} else
			ay_do_error(_("Invalid Service"),
				_("Can't add account : unknown service"));
	}

	return FALSE;
}

/* Callback for OK button */
static void ok_callback(GtkWidget *widget, gpointer data)
{
	FILE *fp;
	char buff[1024];
	LList *saved_acc_info = NULL, *acc_walk = NULL, *to_remove = NULL;

	if (gtk_entry_get_text(GTK_ENTRY(username)) != NULL
		&& strlen(gtk_entry_get_text(GTK_ENTRY(username))) > 0
		&& num_accounts == 0) {
		add_callback(widget, data);
	}

	g_snprintf(buff, 1024, "%saccounts", config_dir);

	fp = fdopen(creat(buff, 0700), "w");

	if (num_accounts == 0) {
		ay_do_error(_("Invalid Account"),
			_("You didn't define an account."));
		return;
	}

	eb_sign_off_all();

	gtk_tree_model_foreach(GTK_TREE_MODEL(account_list_store),
		save_accounts, fp);

	fclose(fp);

	saved_acc_info = ay_save_account_information(-1);

	acc_walk = accounts;
	if (acc_walk) {
		while (acc_walk != NULL && acc_walk->data != NULL) {
			if (!l_list_find(existing_accounts, acc_walk->data)) {
				eb_local_account *removed =
					(eb_local_account *)(acc_walk->data);
				/* removed account */
				if (removed && removed->connected
					&& RUN_SERVICE(removed)->logout != NULL)
					RUN_SERVICE(removed)->logout(removed);
				to_remove =
					l_list_append(to_remove,
					acc_walk->data);
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

	load_accounts();
	rebuild_set_status_menu();
	set_menu_sensitivity();

	ay_restore_account_information(saved_acc_info);
	l_list_free(saved_acc_info);
}

void ay_edit_local_accounts(void)
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
	guint label_key;
	GtkWidget *toolbar;
	GtkToolItem *toolitem;
	GtkToolItem *tool_sep;
	GtkWidget *separator;
	LList *list;
	LList *l;

	GtkAccelGroup *accel_group;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;

	if (is_open)
		return;

	is_open = 1;

	accel_group = gtk_accel_group_new();

	account_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_position(GTK_WINDOW(account_window), GTK_WIN_POS_MOUSE);
	gtk_widget_realize(account_window);

	account_list_store = gtk_list_store_new(COL_COUNT,
		G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

	account_list =
		gtk_tree_view_new_with_model(GTK_TREE_MODEL
		(account_list_store));

	renderer = gtk_cell_renderer_toggle_new();
	column = gtk_tree_view_column_new_with_attributes(text[CONNECT],
		renderer, "active", CONNECT, NULL);

	gtk_tree_view_append_column(GTK_TREE_VIEW(account_list), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(text[USER_NAME],
		renderer, "text", USER_NAME, NULL);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(account_list), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(text[SERVICE_TYPE],
		renderer, "text", SERVICE_TYPE, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(account_list), column);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(account_list));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);

	gtk_container_set_border_width(GTK_CONTAINER(account_window), 5);
	g_signal_connect(selection, "changed",
		G_CALLBACK(selection_made_callback), NULL);

	box = gtk_vbox_new(FALSE, 0);
	window_box = gtk_vbox_new(FALSE, 5);
	hbox = gtk_hbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);

	/*Screen Name Section */

	label = gtk_label_new_with_mnemonic(_("Screen _Name:"));
	label_key = gtk_label_get_mnemonic_keyval(GTK_LABEL(label));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 5);
	gtk_widget_show(label);
	username = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(box), username, FALSE, FALSE, 2);
	gtk_widget_show(username);
	gtk_widget_add_accelerator(username, "grab_focus", accel_group,
		label_key, GDK_MOD1_MASK, (GtkAccelFlags) 0);

	/*Password Section */

	label = gtk_label_new_with_mnemonic(_("_Password:"));
	label_key = gtk_label_get_mnemonic_keyval(GTK_LABEL(label));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 5);
	gtk_widget_show(label);
	password = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(password), FALSE);
	gtk_box_pack_start(GTK_BOX(box), password, FALSE, FALSE, 2);
	gtk_widget_show(password);
	gtk_widget_add_accelerator(password, "grab_focus", accel_group,
		label_key, GDK_MOD1_MASK, (GtkAccelFlags) 0);

	/*Service Type Section */

	label = gtk_label_new_with_mnemonic(_("Service _Type:"));
	label_key = gtk_label_get_mnemonic_keyval(GTK_LABEL(label));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 5);
	gtk_widget_show(label);

	service_type = gtk_combo_box_new_text();

	list = get_service_list();
	for (l = list; l; l = l_list_next(l)) {
		char *label = l->data;
		gtk_combo_box_append_text(GTK_COMBO_BOX(service_type), label);
	}
	l_list_free(list);

	gtk_widget_show(service_type);

	gtk_box_pack_start(GTK_BOX(box), service_type, FALSE, FALSE, 2);
	gtk_widget_add_accelerator(service_type, "grab_focus", accel_group,
		label_key, GDK_MOD1_MASK, (GtkAccelFlags) 0);

	/*Connect at startup Section */

	connect_at_startup =
		gtk_check_button_new_with_mnemonic(_("_Connect at startup"));
	label_key =
		gtk_label_get_mnemonic_keyval(GTK_LABEL(GTK_BIN
			(connect_at_startup)->child));
	gtk_widget_show(connect_at_startup);
	gtk_widget_add_accelerator(connect_at_startup, "clicked", accel_group,
		label_key, GDK_MOD1_MASK, (GtkAccelFlags) 0);
	gtk_box_pack_start(GTK_BOX(box), connect_at_startup, FALSE, FALSE, 5);

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

	toolbar = gtk_toolbar_new();
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_BOTH);
	gtk_toolbar_set_show_arrow(GTK_TOOLBAR(toolbar), FALSE);
	gtk_toolbar_set_orientation(GTK_TOOLBAR(toolbar),
		GTK_ORIENTATION_HORIZONTAL);
	gtk_container_set_border_width(GTK_CONTAINER(toolbar), 0);

	/*Add Button */

#define TOOLBAR_APPEND(titem,stock,tip,callback,cb_data) { \
	titem = gtk_tool_button_new_from_stock(stock); \
	gtk_tool_item_set_tooltip_text(titem, tip); \
	g_signal_connect(titem, "clicked", G_CALLBACK(callback), cb_data); \
	gtk_widget_show(GTK_WIDGET(titem)); \
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), titem, -1); \
}

/* line will tell whether to draw the separator line or not */
#define TOOLBAR_APPEND_SEPARATOR(line) { \
	tool_sep = gtk_separator_tool_item_new(); \
	gtk_separator_tool_item_set_draw(GTK_SEPARATOR_TOOL_ITEM(tool_sep), line); \
	gtk_widget_show(GTK_WIDGET(tool_sep)); \
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), tool_sep, -1); \
}

	TOOLBAR_APPEND(toolitem, GTK_STOCK_HELP, _("Help"), help_callback,
		NULL);

	TOOLBAR_APPEND_SEPARATOR(TRUE);

	TOOLBAR_APPEND(toolitem, GTK_STOCK_ADD, _("Add Account"), add_callback,
		NULL);

	TOOLBAR_APPEND_SEPARATOR(FALSE);

	/*Delete Button */

	TOOLBAR_APPEND(del_button, GTK_STOCK_DELETE, _("Delete Account"),
		remove_callback, NULL);

	gtk_widget_set_sensitive(GTK_WIDGET(del_button), FALSE);
	TOOLBAR_APPEND_SEPARATOR(FALSE);

	/* Modify Button */

	TOOLBAR_APPEND(mod_button, GTK_STOCK_EDIT, _("Modify Account"),
		modify_callback, NULL);

	gtk_widget_set_sensitive(GTK_WIDGET(mod_button), FALSE);

	TOOLBAR_APPEND_SEPARATOR(TRUE);

	/*Okay Button */

	TOOLBAR_APPEND(toolitem, GTK_STOCK_OK, _("Ok"), ok_callback, NULL);

	TOOLBAR_APPEND_SEPARATOR(FALSE);

	/*Cancel Button */

	TOOLBAR_APPEND(toolitem, GTK_STOCK_CANCEL, _("Cancel"), cancel_callback,
		NULL);

#undef TOOLBAR_APPEND_SEPARATOR
#undef TOOLBAR_APPEND
	/*Buttons End */

	button_box = gtk_hbox_new(FALSE, 0);

	gtk_box_pack_end(GTK_BOX(button_box), toolbar, FALSE, FALSE, 0);
	gtk_widget_show(toolbar);

	gtk_box_pack_start(GTK_BOX(window_box), button_box, FALSE, FALSE, 0);
	gtk_widget_show(button_box);

	gtk_widget_show(window_box);

	gtk_container_add(GTK_CONTAINER(account_window), window_box);

	gtk_window_set_title(GTK_WINDOW(account_window),
		_("Ayttm Account Editor"));

	g_signal_connect(account_window, "destroy", G_CALLBACK(destroy), NULL);

	gtk_window_add_accel_group(GTK_WINDOW(account_window), accel_group);

	gtk_widget_show(account_window);
	gtk_widget_grab_focus(username);
}
