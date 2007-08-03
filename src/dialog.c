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

#include "gtk/gtkutils.h"

#include "pixmaps/tb_yes.xpm"
#include "pixmaps/tb_no.xpm"
#include "pixmaps/ok.xpm"
#include "pixmaps/cancel.xpm"
#include "pixmaps/question.xpm"


typedef struct _list_dialog_data {
	void (*callback)(const char *value, void *data);
	void *data;
} list_dialog_data;

typedef struct {
	void *data;
	void (*action)(void *, int);
} callback_data;

typedef struct _text_input_window
{
	void (*callback)(const char *value, void *data);
	void *data;
	GtkWidget *window;
	GtkWidget *text;
} text_input_window;


static void list_dialog_callback(GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, void *data)
{
	gchar *text;
	list_dialog_data *ldd = data;
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model(tree_view);
	gtk_tree_model_get_iter(model, &iter, path);

	gtk_tree_model_get(model, &iter, 0, &text, -1);
	ldd->callback(text, ldd->data);
	free(ldd);
}

void do_list_dialog( const char *message, const char *title, const char **list, void (*action)(const char *text, void *data), void *data )
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


void do_llist_dialog( const char *message, const char *title, const LList *list, void (*action)(const char *text, void *data), void *data )
{
	GtkWidget * dialog_window;
	GtkWidget * label;
	GtkWidget * clist;
	GtkListStore *clist_store;
	GtkWidget * scwin;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;
	
	/*  UNUSED GtkWidget * button_box; */
	char *Row[2]={NULL, NULL};
	list_dialog_data *ldata;
	const LList *t_list = list;

	eb_debug(DBG_CORE, ">Entering\n");
	if(list==NULL) {
		eb_debug(DBG_CORE, ">Leaving as list[0]==NULL\n");
		return;
	}
	dialog_window = gtk_dialog_new();

	gtk_widget_realize(dialog_window);
	gtk_window_set_title(GTK_WINDOW(dialog_window), title );

	label = gtk_label_new(message);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog_window)->vbox),
		label, FALSE, FALSE, 5);

	clist_store = gtk_list_store_new(1, G_TYPE_STRING);
	clist = gtk_tree_view_new_with_model(GTK_TREE_MODEL(clist_store));
	g_object_set(clist, "headers-visible", FALSE, NULL);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(NULL, renderer,
			"text", 0,
			NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(clist), column);
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(clist));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);

	while(t_list) {
		GtkTreeIter append;
		Row[0]=strdup(t_list->data);
		gtk_list_store_append(clist_store, &append);
		gtk_list_store_set(clist_store, &append, 0, Row[0], -1);
		free(Row[0]);
		t_list = t_list->next;
	}

	ldata=calloc(1, sizeof(list_dialog_data));
	ldata->callback=action;
	ldata->data=data;
	g_signal_connect(clist, "row-activated", G_CALLBACK(list_dialog_callback), ldata);
	g_signal_connect_swapped(clist, "row-activated", G_CALLBACK(gtk_widget_destroy), (gpointer)dialog_window);
	gtk_widget_show(clist);
	/* End list construction */

	scwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_add
		(GTK_CONTAINER(scwin),clist);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request(scwin, 250, 350);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog_window)->action_area), 
						scwin, FALSE, FALSE, 5 );

	gtk_widget_show_all(dialog_window);
	eb_debug(DBG_CORE, ">Leaving, all done\n");
}

typedef struct _dialog_buttons {
	GtkWidget *yes;
	GtkWidget *no;
} dialog_buttons;

static void dialog_close(GtkWidget *widget, GdkEventAny *event, gpointer data)
{
	dialog_buttons *b = (dialog_buttons *)data;
	if (event->type == GDK_KEY_PRESS) {
		if (((GdkEventKey *)event)->keyval == GDK_Escape) {
			g_signal_emit_by_name(b->no, "clicked", NULL);
		} else if (((GdkEventKey *)event)->keyval == GDK_KP_Enter) {
			g_signal_emit_by_name(b->yes, "clicked", NULL);
		} else if (((GdkEventKey *)event)->keyval == GDK_Return) {
			g_signal_emit_by_name(b->yes, "clicked", NULL);
		}
	}
}

static void eb_gtk_dialog_callback(GtkWidget *widget, gpointer data)
{
	callback_data *cd = (callback_data *)data;
	int result=0;

	result=(int)g_object_get_data(G_OBJECT(widget), "userdata");
	cd->action(cd->data, result);
	free(cd);
}

static void do_dialog( const char *message, const char *title, void (*action)(GtkWidget *widget, gpointer data), gpointer data )
{
	GtkWidget *dialog_window;
	GtkWidget *label;
	GtkWidget *hbox2, *hbox_xpm;
	GtkWidget *iconwid;
	GdkPixbuf *icon;
	GtkWidget *button;
	dialog_buttons *buttons = g_new0(dialog_buttons, 1);
	
	dialog_window = gtk_dialog_new();
	gtk_widget_realize(dialog_window);
	
	label = gtk_label_new(message);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_widget_show(label);
	gtk_widget_set_size_request(label, 240, -1);

	gtk_misc_set_alignment (GTK_MISC (label), 0.1, 0.5);

	hbox_xpm = gtk_hbox_new(FALSE,5);
	
	icon = gdk_pixbuf_new_from_xpm_data( (const char **) question_xpm);
	iconwid = gtk_image_new_from_pixbuf(icon);
	
	gtk_box_pack_start(GTK_BOX(hbox_xpm), iconwid, TRUE, TRUE, 20);
	gtk_box_pack_start(GTK_BOX(hbox_xpm), label, TRUE, TRUE, 5);
	gtk_widget_show(iconwid);
	gtk_widget_show(label);

	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog_window)->vbox), hbox_xpm, TRUE, TRUE, 5);
	gtk_widget_show(hbox_xpm);
		
	button = gtkut_create_icon_button( _("No"), tb_no_xpm, dialog_window );
	
	g_signal_connect(button, "clicked", G_CALLBACK(action), data );
	g_signal_connect_swapped(button, "clicked",
			G_CALLBACK(gtk_widget_destroy), (gpointer)dialog_window);
	g_object_set_data(G_OBJECT(button), "userdata", (gpointer)0);
	gtk_widget_show(button);
	
	buttons->no = button;
	
	hbox2 = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_end(GTK_BOX(hbox2), button, FALSE, FALSE, 5);

	button = gtkut_create_icon_button( _("Yes"), tb_yes_xpm, dialog_window );
	
	g_signal_connect(button, "clicked", G_CALLBACK(action), data );
	g_signal_connect_swapped(button, "clicked",
			G_CALLBACK(gtk_widget_destroy), (gpointer)dialog_window);
	g_object_set_data(G_OBJECT(button), "userdata", (gpointer)1);
	gtk_widget_show(button);

	buttons->yes = button;
	
	g_signal_connect(dialog_window, "key-press-event", G_CALLBACK(dialog_close), buttons);
	
	gtk_box_pack_end(GTK_BOX(hbox2), button, FALSE, FALSE, 5);

	gtk_widget_show(hbox2);
	
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog_window)->action_area), 
						hbox2, TRUE, TRUE, 0);
	
	gtk_container_set_border_width(GTK_CONTAINER(dialog_window), 5);
	gtk_widget_realize(dialog_window);
	gtk_window_set_title(GTK_WINDOW(dialog_window), title );
	gtk_window_set_position(GTK_WINDOW(dialog_window), GTK_WIN_POS_MOUSE);
	gtk_widget_set_size_request(dialog_window, 350, 150);
	gtk_widget_show(dialog_window);
	if (dialog_window->requisition.height > 150)
		gtk_widget_set_size_request(dialog_window, 350, -1);
}

void eb_do_dialog(const char *message, const char *title, void (*action)(void *, int), void *data)
{
	callback_data *cd=calloc(1, sizeof(callback_data));
	cd->action=action;
	cd->data=data;
	do_dialog(message, title, eb_gtk_dialog_callback, cd);
}

static void self_update_value(void *data, int ans)
{
	int *value = (int *)data;
	*value = ans;
	
}

void eb_do_no_callback_dialog( const char *message, const char *title, int *value)
{
	*value = -1;
	eb_do_dialog(message, title, self_update_value, value);
	while (*value == -1) {
		gtk_main_iteration();
	}	
}

/*
 * The following methods are for the text input window
 */

static void input_window_destroy(GtkWidget * widget, gpointer data)
{
	g_free(data);
}

static void input_window_cancel(GtkWidget * widget, gpointer data)
{
	text_input_window * window = (text_input_window*)data;
	gtk_widget_destroy(window->window);
}

static void input_window_ok(GtkWidget * widget, gpointer data)
{
	char *text;
	text_input_window * window = (text_input_window*)data;

	if(GTK_IS_TEXT_VIEW(window->text)) {
		GtkTextIter start, end;
		GtkTextBuffer *buffer=gtk_text_view_get_buffer(GTK_TEXT_VIEW(window->text));
		gtk_text_buffer_get_bounds(buffer, &start, &end);

		text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
	}
	else {
		text = gtk_editable_get_chars(GTK_EDITABLE(window->text), 0, -1);
	}
	window->callback(text, window->data);
	g_free(text);
}

static gboolean	enter_pressed( GtkWidget *widget, GdkEventKey *event, gpointer data )
{
	if (event->keyval == GDK_Return) {
		input_window_ok(NULL, data);
		gtk_widget_destroy(((text_input_window*)data)->window);
		return TRUE;
	}
	return FALSE;
}

void do_text_input_window(const char *title, const char *value, 
		void (*action)(const char *text, void *data), 
		void *data )
{
	do_text_input_window_multiline(title, value, 1, 0, action, data);
}

void do_password_input_window(const char *title, const char *value, 
		void (*action)(const char *text, void *data), 
		void *data )
{
	do_text_input_window_multiline(title, value, 0, 1, action, data);
}

void do_text_input_window_multiline(const char *title, const char *value, 
		int ismulti, int ispassword, 
		void (*action)(const char *text, void *data), 
		void *data )
{
	GtkWidget * vbox = gtk_vbox_new(FALSE, 5);
	GtkWidget * label; 
	GtkWidget * hbox;
	GtkWidget * hbox2;
	GtkWidget * separator;
	GtkWidget * button;
	int dummy;
	char *window_title;
	text_input_window * input_window = g_new0(text_input_window, 1);
	
	if (ispassword) ismulti=FALSE;
	
	input_window->callback = action;
	input_window->data = data;

	input_window->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_position(GTK_WINDOW(input_window->window), GTK_WIN_POS_MOUSE);
	gtk_widget_realize(input_window->window);

	gtk_container_set_border_width(GTK_CONTAINER(input_window->window), 5);
	label = gtk_label_new(title);

	if (ismulti)
		input_window->text = gtk_text_view_new();
	else {
		input_window->text = gtk_entry_new();
		gtk_editable_set_editable(GTK_EDITABLE(input_window->text), TRUE);
		g_signal_connect(input_window->text, "key-press-event",
				G_CALLBACK(enter_pressed), input_window);
	}
	if (ispassword)
		gtk_entry_set_visibility(GTK_ENTRY(input_window->text), FALSE);
	
	gtk_widget_set_size_request(input_window->text, 400, ismulti?200:-1);

	if(!ismulti) {
		gtk_editable_insert_text(GTK_EDITABLE(input_window->text),
					 value, strlen(value), &dummy);
		gtk_editable_set_editable(GTK_EDITABLE(input_window->text),
				TRUE);
	}
	else {
		GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(input_window->text));
		gtk_text_buffer_insert_at_cursor(buffer, value, strlen(value));
	}

	gtk_widget_show(label);
	gtk_widget_show(input_window->text);

	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), input_window->text, TRUE, TRUE, 0);


	separator = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), separator, FALSE, FALSE, 5);
	gtk_widget_show(separator);

	hbox2 = gtk_hbox_new(TRUE, 5);

  	gtk_widget_set_size_request(hbox2, 200,25);

	button = gtkut_create_icon_button( _("OK"), ok_xpm, input_window->window );
		     
	g_signal_connect(button, "clicked", G_CALLBACK(input_window_ok), input_window);

	g_signal_connect_swapped(button, "clicked",
			G_CALLBACK(gtk_widget_destroy),input_window->window);
	     
	gtk_box_pack_start(GTK_BOX(hbox2), button, TRUE, TRUE, 0);
	gtk_widget_show(button);

	button = gtkut_create_icon_button( _("Cancel"), cancel_xpm, input_window->window );
	     
	g_signal_connect(button, "clicked", G_CALLBACK(input_window_cancel), input_window);

	gtk_box_pack_start(GTK_BOX(hbox2), button, TRUE, TRUE, 0);
	gtk_widget_show(button);

	hbox = gtk_hbox_new(FALSE, 0);
		
	gtk_box_pack_end(GTK_BOX(hbox),hbox2, FALSE, FALSE, 0);
	gtk_widget_show(hbox2);      
      
  	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	gtk_container_add(GTK_CONTAINER(input_window->window), vbox);
	gtk_widget_show(vbox);

	if (strstr(title,"\n")) 
		window_title = g_strndup(title, (int)(strstr(title,"\n") - title));
	else
		window_title = g_strdup(title);
    	gtk_window_set_title(GTK_WINDOW(input_window->window), window_title);
	g_free(window_title);

	g_signal_connect(input_window->window, "destroy",
			G_CALLBACK(input_window_destroy), input_window);

	gtk_widget_grab_focus(input_window->text);
	gtk_widget_show(input_window->window);
}

