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
 * trigger.c
 */
#include "intl.h"
#include <string.h>
#include <stdlib.h>

#include "chat_window.h"
#include "dialog.h"
#include "sound.h"
#include "util.h"
#include "globals.h"
#include "file_select.h"
#include "messages.h"
#include "gtk/gtkutils.h"

static gint window_open = 0;
static GtkWidget *edit_trigger_window;
static GtkWidget *trigger_list;
static GtkWidget *account_name;
static GtkWidget *parameter;
static GtkWidget *action_name;

static void quick_message(const char *contact, int online, const char *message)
{

	char buf[1024];
	snprintf(buf, 1024, _("%s just came %s.\n\n%s"), contact,
		online ? _("online") : _("offline"), message);
	ay_do_info(_("Online trigger"), buf);
}

static void pounce_contact(struct contact *con, char *str)
{
	gint pos = 0;

	ay_conversation_chat_with_contact(con);
	gtk_editable_insert_text(GTK_EDITABLE(con->conversation->window->entry), str,
		strlen(str), &pos);
	send_message(NULL, con->conversation);
}

void do_trigger_action(struct contact *con, int trigger_type)
{
	gchar param_string[2048];
	gchar *substr;
	gchar *basestr;

	strcpy(param_string, "");
	substr = NULL;

	if (con->trigger.action == NO_ACTION)
		return;
	if (strlen(con->trigger.param) >= (sizeof(param_string) / 2)) {
		eb_debug(DBG_CORE, "Trigger parameter too long - ignoring");
		return;
	}
	/* replace all occurrences of %t with "online" or "offline" */
	basestr = con->trigger.param;
	while ((substr = strstr(basestr, "%t")) != NULL) {
		if (substr[-1] == '%') {
			strncat(param_string, basestr,
				(size_t) (substr - basestr + 2));
			basestr = substr + 2;
			continue;
		} else {
			strncat(param_string, basestr,
				(size_t) (substr - basestr));
			strncat(param_string,
				((trigger_type ==
						USER_ONLINE) ? _("online") :
					_("offline")),
				(size_t) (sizeof(param_string) -
					strlen(param_string)));
			basestr = substr + 2;
		}
		if ((strlen(param_string) + strlen(basestr) + 8) >
			sizeof(param_string)) {
			eb_debug(DBG_CORE,
				"Result string may be too long, no substitution done\n");
			basestr = con->trigger.param;
			strcpy(param_string, "");
			break;
		}
	}
	/* copy remainder (or all if no subst done) */
	strncat(param_string, basestr,
		(size_t) (sizeof(param_string) - strlen(param_string)));

	if (con->trigger.action == PLAY_SOUND) {
		playsoundfile(param_string);
	} else if (con->trigger.action == EXECUTE) {
		system(param_string);
	} else if (con->trigger.action == DIALOG) {
		quick_message(con->nick, con->trigger.type != USER_OFFLINE,
			param_string);
	} else if (con->trigger.action == POUNCE
		&& con->trigger.type == USER_ONLINE) {
		pounce_contact(con, param_string);
	}

}

void do_trigger_online(struct contact *con)
{
	if ((con->trigger.type == USER_ONLINE)
		|| (con->trigger.type == USER_ON_OFF_LINE)) {
		do_trigger_action(con, USER_ONLINE);
	}
}

void do_trigger_offline(struct contact *con)
{
	if ((con->trigger.type == USER_OFFLINE)
		|| (con->trigger.type == USER_ON_OFF_LINE)) {
		do_trigger_action(con, USER_OFFLINE);
	}
}

static void browse_done(const char *filename, void *data)
{
	if (filename == NULL)
		return;

	gtk_entry_set_text(GTK_ENTRY(parameter), filename);
}

static void set_button_callback(GtkWidget *widget, struct contact *con)
{
	strncpy(con->trigger.param, gtk_entry_get_text(GTK_ENTRY(parameter)),
		sizeof(con->trigger.param));
	con->trigger.type =
		gtk_combo_box_get_active(GTK_COMBO_BOX(trigger_list));
	con->trigger.action =
		gtk_combo_box_get_active(GTK_COMBO_BOX(action_name));

	write_contact_list();

	destroy_window();
}

void destroy_window()
{
	window_open = 0;

	gtk_widget_destroy(edit_trigger_window);
}

static void browse_file(GtkWidget *widget, gpointer data)
{
	ay_do_file_selection(NULL, _("Select parameter"), browse_done, NULL);
}

trigger_type get_trigger_type_num(const char *text)
{
	if (!text)
		return -1;

	if (!strcmp(text, "USER_ONLINE")) {
		return USER_ONLINE;
	} else if (!strcmp(text, "USER_OFFLINE")) {
		return USER_OFFLINE;
	} else if (!strcmp(text, "USER_ON_OFF_LINE")) {
		return USER_ON_OFF_LINE;
	}

	return -1;
}

trigger_action get_trigger_action_num(const char *text)
{
	if (!text)
		return -1;

	if (!strcmp(text, "PLAY_SOUND")) {
		return PLAY_SOUND;
	} else if (!strcmp(text, "EXECUTE")) {
		return EXECUTE;
	} else if (!strcmp(text, "DIALOG")) {
		return DIALOG;
	} else if (!strcmp(text, "POUNCE")) {
		return POUNCE;
	}

	return -1;
}

gchar *get_trigger_type_text(trigger_type type)
{
	if (type == USER_ONLINE)
		return "USER_ONLINE";
	else if (type == USER_OFFLINE)
		return "USER_OFFLINE";
	else if (type == USER_ON_OFF_LINE)
		return "USER_ON_OFF_LINE";
	else
		return "UNKNOWN";

}

gchar *get_trigger_action_text(trigger_action action)
{
	if (action == PLAY_SOUND)
		return "PLAY_SOUND";
	else if (action == EXECUTE)
		return "EXECUTE";
	else if (action == DIALOG)
		return "DIALOG";
	else if (action == POUNCE)
		return "POUNCE";
	else
		return "UNKNOWN";
}

static void get_trigger_list(GtkComboBox *entrybox)
{
	gtk_combo_box_append_text(entrybox, _("None"));
	gtk_combo_box_append_text(entrybox, _("User goes online"));
	gtk_combo_box_append_text(entrybox, _("User goes offline"));
	gtk_combo_box_append_text(entrybox, _("User goes on or offline"));
}

static void get_action_list(GtkComboBox *actions)
{
	gtk_combo_box_append_text(actions, _("None"));
	gtk_combo_box_append_text(actions, _("Play sound"));
	gtk_combo_box_append_text(actions, _("Execute command"));
	gtk_combo_box_append_text(actions, _("Show alert dialog"));
	gtk_combo_box_append_text(actions, _("Send message"));
}

void show_trigger_window(struct contact *con)
{
	GtkWidget *hbox;
	GtkWidget *hbox2;
	GtkWidget *vbox;
	GtkWidget *label;
	GtkWidget *button;
	GtkWidget *table;
	GtkWidget *frame;
	GtkWidget *separator;
	GtkWidget *hbox_param;
	GtkWidget *browse_button;

	if (window_open)
		return;
	window_open = 1;

	edit_trigger_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_position(GTK_WINDOW(edit_trigger_window),
		GTK_WIN_POS_MOUSE);
	gtk_widget_realize(edit_trigger_window);
	gtk_container_set_border_width(GTK_CONTAINER(edit_trigger_window), 5);

	table = gtk_table_new(4, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5);
	gtk_container_set_border_width(GTK_CONTAINER(table), 5);
	vbox = gtk_vbox_new(FALSE, 5);
	hbox = gtk_hbox_new(FALSE, 0);

	/*Section for letting the user know which trigger they are editing */

	label = gtk_label_new(_("User: "));
	gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 0, 1, GTK_FILL, GTK_FILL,
		0, 0);
	gtk_widget_show(hbox);

	account_name = gtk_label_new(con->nick);
	gtk_misc_set_alignment(GTK_MISC(account_name), 0, .5);
	gtk_table_attach(GTK_TABLE(table), account_name, 1, 2, 0, 1, GTK_FILL,
		GTK_FILL, 0, 0);
	gtk_widget_show(account_name);

	/*Section for declaring the trigger */
	hbox = gtk_hbox_new(FALSE, 0);

	label = gtk_label_new(_("Trigger: "));
	gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 1, 2, GTK_FILL, GTK_FILL,
		0, 0);
	gtk_widget_show(hbox);

	/*List of trigger types */

	trigger_list = gtk_combo_box_new_text();
	get_trigger_list(GTK_COMBO_BOX(trigger_list));
	gtk_combo_box_set_active(GTK_COMBO_BOX(trigger_list),
		con->trigger.type);

	gtk_table_attach(GTK_TABLE(table), trigger_list, 1, 2, 1, 2, GTK_FILL,
		GTK_FILL, 0, 0);
	gtk_widget_show(trigger_list);

	/*Section for action declaration */

	hbox = gtk_hbox_new(FALSE, 0);

	label = gtk_label_new(_("Action: "));
	gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 2, 3, GTK_FILL, GTK_FILL,
		0, 0);
	gtk_widget_show(hbox);

	/*List of available actions */
	action_name = gtk_combo_box_new_text();
	get_action_list(GTK_COMBO_BOX(action_name));

	gtk_combo_box_set_active(GTK_COMBO_BOX(action_name),
		con->trigger.action);

	gtk_table_attach(GTK_TABLE(table), action_name, 1, 2, 2, 3, GTK_FILL,
		GTK_FILL, 0, 0);
	gtk_widget_show(action_name);

	/*Section for Contact Name */
	hbox = gtk_hbox_new(FALSE, 0);

	label = gtk_label_new(_("Parameter: "));
	gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 3, 4, GTK_FILL, GTK_FILL,
		0, 0);
	gtk_widget_show(hbox);

	/*Entry for parameter */
	hbox_param = gtk_hbox_new(FALSE, 0);
	parameter = gtk_entry_new();

	gtk_entry_set_text(GTK_ENTRY(parameter), con->trigger.param);

	gtk_box_pack_start(GTK_BOX(hbox_param), parameter, FALSE, FALSE, 0);

	browse_button = gtk_button_new_with_label(_("Browse"));
	g_signal_connect(browse_button, "clicked", G_CALLBACK(browse_file),
		con);
	gtk_box_pack_start(GTK_BOX(hbox_param), browse_button, FALSE, FALSE, 0);

	gtk_table_attach(GTK_TABLE(table), hbox_param, 1, 2, 3, 4, GTK_FILL,
		GTK_FILL, 0, 0);
	gtk_widget_show(parameter);
	gtk_widget_show(browse_button);
	gtk_widget_show(hbox_param);

	/*Lets create a frame to put all of this in */
	frame = gtk_frame_new(NULL);
	gtk_frame_set_label(GTK_FRAME(frame), _("Edit Trigger"));

	gtk_container_add(GTK_CONTAINER(frame), table);
	gtk_widget_show(table);

	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);
	gtk_widget_show(frame);

	/*Lets try adding a separator */
	separator = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), separator, FALSE, FALSE, 5);
	gtk_widget_show(separator);

	hbox2 = gtk_hbox_new(TRUE, 5);

	/*Add Button */
	button = gtkut_stock_button_new_with_label(_("Update"),
		GTK_STOCK_PREFERENCES);
	g_signal_connect(button, "clicked", G_CALLBACK(set_button_callback),
		con);
	gtk_box_pack_start(GTK_BOX(hbox2), button, TRUE, TRUE, 0);

	/*Cancel Button */

	button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
	g_signal_connect_swapped(button, "clicked",
		G_CALLBACK(gtk_widget_destroy), edit_trigger_window);

	gtk_box_pack_start(GTK_BOX(hbox2), button, TRUE, TRUE, 0);

	/*Buttons End */

	hbox = gtk_hbox_new(FALSE, 0);
	table = gtk_table_new(1, 1, FALSE);

	gtk_box_pack_end(GTK_BOX(hbox), hbox2, FALSE, FALSE, 0);
	gtk_widget_show(hbox2);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	gtk_table_attach(GTK_TABLE(table), vbox, 0, 1, 0, 1, GTK_EXPAND,
		GTK_EXPAND, 0, 0);
	gtk_widget_show(vbox);

	gtk_container_add(GTK_CONTAINER(edit_trigger_window), table);

	gtk_widget_show(table);

	gtk_window_set_title(GTK_WINDOW(edit_trigger_window),
		_("Ayttm - Edit Trigger"));
	gtk_widget_show(edit_trigger_window);

	g_signal_connect(edit_trigger_window, "destroy",
		G_CALLBACK(destroy_window), NULL);

	window_open = 1;
}
