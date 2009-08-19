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

#include <string.h>
#include <stdlib.h>
#include <gdk/gdkkeysyms.h>

#include "dialog.h"
#include "gtk_globals.h"


/* List Dialogs */
void do_list_dialog( const char *message, const char *title, const char **list, 
			void (*action)(const char *text, void *data), void *data )
{
	const char **ptr=list;
	LList *tmp = NULL;

	while(*ptr) {
		char *t=strdup(*ptr);
		ptr++;
		tmp = l_list_append(tmp, t);
	}

	do_llist_dialog(message, title, tmp, action, data);

	while(tmp) {
		LList *t=tmp;
		free(tmp->data);
		tmp = l_list_remove_link(tmp, tmp);
		l_list_free_1(t);
	}
}


void do_llist_dialog( const char *message, const char *title, const LList *list, 
			void (*action)(const char *text, void *data), void *data )
{
	GtkWidget *dialog;
	GtkWidget *label;
	GtkWidget *list_view;
	GtkListStore *list_store;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;

	GtkWidget *scwin;
	GtkWidget *dialog_content_area;
	
	const LList *t_list = list;

	int result = 0;

	eb_debug(DBG_CORE, ">Entering\n");
	if(list==NULL) {
		eb_debug(DBG_CORE, ">Leaving as list[0]==NULL\n");
		return;
	}

	dialog = gtk_dialog_new_with_buttons(
						title,
						NULL,
						0,
						GTK_STOCK_OK,
						GTK_RESPONSE_ACCEPT,
						GTK_STOCK_CANCEL,
						GTK_RESPONSE_REJECT,
						NULL
					);

#ifdef HAVE_GTK_2_14
	dialog_content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
#else
	dialog_content_area = GTK_DIALOG(dialog)->vbox;
#endif

	label = gtk_label_new(message);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);


	list_store = gtk_list_store_new(1, G_TYPE_STRING);
	list_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));
	g_object_set(list_view, "headers-visible", FALSE, NULL);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(NULL, renderer,
			"text", 0,
			NULL);

	gtk_tree_view_append_column(GTK_TREE_VIEW(list_view), column);
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(list_view));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);

	g_signal_connect_swapped(list_view, "row-activated", G_CALLBACK(gtk_window_activate_default), dialog);

	scwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_add (GTK_CONTAINER(scwin),list_view);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request(scwin, -1, 350);

	while(t_list) {
		GtkTreeIter append;

		gtk_list_store_append(list_store, &append);
		gtk_list_store_set(list_store, &append, 0, t_list->data, -1);
		t_list = t_list->next;
	}

	gtk_box_pack_start(GTK_BOX(dialog_content_area), label, FALSE, FALSE, 10);
	gtk_box_pack_start(GTK_BOX(dialog_content_area), scwin, TRUE, TRUE, 5 );

	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

	gtk_widget_show_all(dialog);

	result = gtk_dialog_run(GTK_DIALOG(dialog));

	if(result == GTK_RESPONSE_ACCEPT) {
		char *chars;
		GtkTreeIter iter;
		GtkTreeModel *model = GTK_TREE_MODEL(list_store);

		if(!gtk_tree_selection_get_selected(selection, &model, &iter))
			return;
        
		gtk_tree_model_get(GTK_TREE_MODEL(list_store), &iter, 0, &chars, -1);
		action(chars, data);
	}

	gtk_widget_destroy(dialog);
}


/* Confirmation dialogs */
void eb_do_dialog(const char *message, const char *title, void (*action)(void *, int), void *data)
{
	int result = eb_do_confirm_dialog(message, title);

	action(data, result);
}


int eb_do_confirm_dialog(const char *message, const char *title)
{
	GtkWidget *dialog, *content_area, *label;
	int ret=0;

	GtkWidget *dialog_content_area = NULL;

	dialog = gtk_dialog_new_with_buttons(
						title,
						NULL,
						0,
						GTK_STOCK_YES,
						GTK_RESPONSE_ACCEPT,
						GTK_STOCK_NO,
						GTK_RESPONSE_REJECT,
						NULL
					);

	content_area = gtk_hbox_new(FALSE, 5);

#ifdef HAVE_GTK_2_14
	dialog_content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
#else
	dialog_content_area = GTK_DIALOG(dialog)->vbox;
#endif

	gtk_box_pack_start(GTK_BOX(dialog_content_area), content_area, TRUE, TRUE, 2);

	label = gtk_label_new("");

	gtk_label_set_markup(GTK_LABEL(label), message);

	gtk_box_pack_start(GTK_BOX(content_area), 
			gtk_image_new_from_stock(GTK_STOCK_DIALOG_QUESTION, GTK_ICON_SIZE_DIALOG), 
			FALSE, FALSE, 5);

	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT);

	gtk_box_pack_start(GTK_BOX(content_area), label, TRUE, TRUE, 5);

	gtk_widget_show_all(dialog);

	ret = gtk_dialog_run(GTK_DIALOG(dialog));

	if (ret == GTK_RESPONSE_ACCEPT)
		ret = 1;
	else
		ret = 0;

	gtk_widget_destroy(dialog);

	return ret;
}


/* Input Windows */
static void do_input_window_internal(const char *title, const char *value,
				void (*action)(const char *text, void *data), int is_password,
				void *data)
{
	GtkWidget *label, *text;
	GtkWidget *dialog_content_area = NULL;
	int dummy = 0, result = 0;
	char *chars = NULL;

	GtkWidget *dialog = gtk_dialog_new_with_buttons(
							_("Ayttm"),
							NULL,	/* TODO get parent */
							0,
							GTK_STOCK_OK,
							GTK_RESPONSE_ACCEPT,
							GTK_STOCK_CANCEL,
							GTK_RESPONSE_REJECT,
							NULL
						);

#ifdef HAVE_GTK_2_14
	dialog_content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
#else
	dialog_content_area = GTK_DIALOG(dialog)->vbox;
#endif

	label = gtk_label_new("");
	text = gtk_entry_new();

	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_label_set_markup(GTK_LABEL(label), title);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);

	gtk_editable_insert_text (GTK_EDITABLE(text), value, strlen(value), &dummy);
	gtk_editable_set_editable(GTK_EDITABLE(text), TRUE);
	gtk_entry_set_activates_default(GTK_ENTRY(text), TRUE);

	if(is_password)
		gtk_entry_set_visibility(GTK_ENTRY(text), FALSE);

	gtk_box_pack_start(GTK_BOX(dialog_content_area), label, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(dialog_content_area), text, FALSE, TRUE, 5);

	gtk_widget_show_all(dialog);

	result = gtk_dialog_run(GTK_DIALOG(dialog));

	if(result == GTK_RESPONSE_ACCEPT) {
		chars = gtk_editable_get_chars(GTK_EDITABLE(text), 0, -1);
		action(chars, data);
	}

	gtk_widget_destroy(dialog);
}


void do_text_input_window(const char *title, const char *value, 
		void (*action)(const char *text, void *data), 
		void *data )
{
	do_input_window_internal(title, value, action, 0, data);
}

void do_password_input_window(const char *title, const char *value, 
		void (*action)(const char *text, void *data), 
		void *data )
{
	do_input_window_internal(title, value, action, 1, data);
}

void do_text_input_window_multiline(const char *title, const char *value, 
		void (*action)(const char *text, void *data), 
		void *data )
{
	GtkWidget * label; 
	GtkWidget *text;
	GtkWidget * dialog_content_area;
	int result = 0;

	GtkTextBuffer *buffer = NULL;
	
	GtkWidget *dialog = gtk_dialog_new_with_buttons(
							_("Ayttm"),
							NULL,	/* TODO get parent */
							0,
							GTK_STOCK_OK,
							GTK_RESPONSE_ACCEPT,
							GTK_STOCK_CANCEL,
							GTK_RESPONSE_REJECT,
							NULL
						);

#ifdef HAVE_GTK_2_14
	dialog_content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
#else
	dialog_content_area = GTK_DIALOG(dialog)->vbox;
#endif

	label = gtk_label_new("");


	text = gtk_text_view_new();
	
	gtk_widget_set_size_request(text, 400, 200);

	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
	gtk_text_buffer_insert_at_cursor(buffer, value, strlen(value));

	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_label_set_markup(GTK_LABEL(label), title);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);

	gtk_box_pack_start(GTK_BOX(dialog_content_area), label, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(dialog_content_area), text, FALSE, FALSE, 5);

	gtk_widget_show_all(dialog);

	result = gtk_dialog_run(GTK_DIALOG(dialog));

	if(result == GTK_RESPONSE_ACCEPT) {
		GtkTextIter start, end;
		char *chars;

		gtk_text_buffer_get_bounds(buffer, &start, &end);

		chars = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
		action(chars, data);
	}

	gtk_widget_destroy(dialog);
}


