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
	void (*callback)(char *value, void *data);
	void *data;
} list_dialog_data;

typedef struct _text_input_window
{
	void (*callback)(char *value, void *data);
	void *data;
	GtkWidget *window;
	GtkWidget *text;
} text_input_window;


static void list_dialog_callback(GtkWidget *widget,
			gint row,
			gint column,
			GdkEventButton *event,
			gpointer data)
{
	gchar *text;
	list_dialog_data *ldd = data;

	gtk_clist_get_text(GTK_CLIST(widget), row, column, &text);
	ldd->callback(strdup(text), ldd->data);
	free(ldd);
}

void do_list_dialog( char * message, char * title, const char **list, void (*action)(char * text, gpointer data), gpointer data )
{
	const char **ptr=list;
	LList *tmp = NULL;
	while(*ptr) {
		char *t=strdup(*ptr);
		ptr++;
		tmp = l_list_append(tmp, t);
	}
	do_llist_dialog(message, title, tmp, action, data);
}

static int sig1 = 0, sig2 = 0;
static GtkWidget *s_scwin = NULL;
static gboolean list_dialog_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	char c;
	GtkWidget *clist = GTK_WIDGET(data);
	int i = 0;
	GdkModifierType	modifiers = event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_MOD4_MASK);

	if (modifiers == 0) {
		c = event->keyval;
		if (c >= GDK_A && c <= GDK_Z) {
			c += (GDK_a - GDK_A);
		}
		
		if (c >= GDK_a && c <= GDK_z) {
			char *text=NULL;
			while (gtk_clist_get_text(GTK_CLIST(clist), i, 0, &text)) {
				if (s_scwin && text && (text[0] == c
				|| (text[0]=='#' && text[1]==c) /*irc hack*/ )) {
					GtkAdjustment *ga;
					eb_debug(DBG_CORE, "found %s with %c\n", text, c);

					gtk_signal_handler_block(GTK_OBJECT(clist), sig1);
					gtk_signal_handler_block(GTK_OBJECT(clist), sig2);
					gtk_clist_select_row(GTK_CLIST(clist), i, 0);
					
					gtk_clist_moveto(GTK_CLIST(clist), i, 0, 0.5, 0.5);
					ga = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(s_scwin));
					gtk_adjustment_set_value(ga, (GTK_CLIST(clist)->row_height + 1) * i);
					gtk_scrolled_window_set_vadjustment(GTK_SCROLLED_WINDOW(s_scwin), ga);
								
					gtk_signal_handler_unblock(GTK_OBJECT(clist), sig1);
					gtk_signal_handler_unblock(GTK_OBJECT(clist), sig2);
					return TRUE;
				}
				i++;
			}
		}
	}
	return TRUE;
}

void do_llist_dialog( char * message, char * title, LList *list, void (*action)(char * text, gpointer data), gpointer data )
{
	GtkWidget * dialog_window;
	GtkWidget * label;
	GtkWidget * clist;
	GtkWidget * scwin;
	
	/*  UNUSED GtkWidget * button_box; */
	char *Row[2]={NULL, NULL};
	list_dialog_data *ldata;
	LList *t_list = list;

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

	/* Convert the **list to a LList */
	clist = gtk_clist_new(1);	/* Only 1 column */
	gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_SINGLE);
	gtk_clist_set_column_width (GTK_CLIST(clist), 0, 200);
	/* Array of pointers to elements, one per column */
	while(t_list) {
		Row[0]=strdup(t_list->data);
		gtk_clist_append(GTK_CLIST(clist), Row);
		free(Row[0]);
		t_list = t_list->next;
	}

	ldata=calloc(1, sizeof(list_dialog_data));
	ldata->callback=action;
	ldata->data=data;
	gtk_signal_connect(GTK_OBJECT(dialog_window),
			"key_press_event",
			GTK_SIGNAL_FUNC(list_dialog_key_press),
			clist);
	sig1 = gtk_signal_connect(GTK_OBJECT(clist),
		"select_row",
		GTK_SIGNAL_FUNC(list_dialog_callback),
		ldata);
	sig2 = gtk_signal_connect_object(GTK_OBJECT(clist),
		"select_row",
		GTK_SIGNAL_FUNC(gtk_widget_destroy),
		(gpointer)dialog_window);
	gtk_widget_show(clist);
	/* End list construction */

	scwin = gtk_scrolled_window_new(NULL, NULL);
	s_scwin = scwin;
	gtk_scrolled_window_add_with_viewport
		(GTK_SCROLLED_WINDOW(scwin),clist);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_usize(scwin, 250, 350);
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
			gtk_signal_emit_by_name(GTK_OBJECT(b->no), "clicked");
		} else if (((GdkEventKey *)event)->keyval == GDK_KP_Enter) {
			gtk_signal_emit_by_name(GTK_OBJECT(b->yes), "clicked");
		} else if (((GdkEventKey *)event)->keyval == GDK_Return) {
			gtk_signal_emit_by_name(GTK_OBJECT(b->yes), "clicked");
		}
			
	}
}

void do_dialog( gchar * message, gchar * title, void (*action)(GtkWidget * widget, gpointer data), gpointer data )
{
	GtkWidget *dialog_window;
	GtkWidget *label;
	GtkWidget *hbox2, *hbox_xpm;
	GtkWidget *iconwid;
	GdkPixmap *icon;
	GdkBitmap *mask;
	GtkWidget *button;
	dialog_buttons *buttons = g_new0(dialog_buttons, 1);
	
	dialog_window = gtk_dialog_new();
	gtk_widget_realize(dialog_window);
	
	label = gtk_label_new(message);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_widget_show(label);
	gtk_widget_set_usize(label, 240, -1);

	gtk_misc_set_alignment (GTK_MISC (label), 0.1, 0.5);

	hbox_xpm = gtk_hbox_new(FALSE,5);
	
	icon = gdk_pixmap_create_from_xpm_d(statuswindow?statuswindow->window:NULL, &mask, NULL, question_xpm);
	iconwid = gtk_pixmap_new(icon, mask);
	
	gtk_box_pack_start(GTK_BOX(hbox_xpm), iconwid, TRUE, TRUE, 20);
	gtk_box_pack_start(GTK_BOX(hbox_xpm), label, TRUE, TRUE, 5);
	gtk_widget_show(iconwid);
	gtk_widget_show(label);

	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog_window)->vbox), hbox_xpm, TRUE, TRUE, 5);
	gtk_widget_show(hbox_xpm);
		
	button = gtkut_create_icon_button( _("No"), tb_no_xpm, dialog_window );
	
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
			 		   GTK_SIGNAL_FUNC(action), data );
	gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
			                  GTK_SIGNAL_FUNC(gtk_widget_destroy),
							  (gpointer)dialog_window);
	gtk_object_set_user_data(GTK_OBJECT(button), (gpointer)0);
	gtk_widget_show(button);
	
	buttons->no = button;
	
	hbox2 = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_end(GTK_BOX(hbox2), button, FALSE, FALSE, 5);

	button = gtkut_create_icon_button( _("Yes"), tb_yes_xpm, dialog_window );
	
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
			 		   GTK_SIGNAL_FUNC(action), data );
	gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
			                  GTK_SIGNAL_FUNC(gtk_widget_destroy),
							  (gpointer)dialog_window);
	gtk_object_set_user_data(GTK_OBJECT(button), (gpointer)1);
	gtk_widget_show(button);

	buttons->yes = button;
	
	gtk_signal_connect(GTK_OBJECT(dialog_window), "key_press_event",
			   GTK_SIGNAL_FUNC(dialog_close), buttons);
	
	gtk_box_pack_end(GTK_BOX(hbox2), button, FALSE, FALSE, 5);

	gtk_widget_show(hbox2);
	
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog_window)->action_area), 
						hbox2, TRUE, TRUE, 0);
	
	gtk_container_set_border_width(GTK_CONTAINER(dialog_window), 5);
	gtk_widget_realize(dialog_window);
	gtk_window_set_title(GTK_WINDOW(dialog_window), title );
	gtk_window_set_position(GTK_WINDOW(dialog_window), GTK_WIN_POS_MOUSE);
	gtk_widget_set_usize(dialog_window, 350, 150);
	gtk_widget_show(dialog_window);
	if (dialog_window->requisition.height > 150)
		gtk_widget_set_usize(dialog_window, 350, -1);
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
	text_input_window * window = (text_input_window*)data;
	char * text = gtk_editable_get_chars(GTK_EDITABLE(window->text), 0, -1);
	window->callback(text, window->data);
	g_free(text);
}

void do_text_input_window(gchar * title, gchar * value, 
		void (*action)(char * text, gpointer data), 
		gpointer data )
{
	do_text_input_window_multiline(title, value, 1, 0, action, data);
}

void do_password_input_window(gchar * title, gchar * value, 
		void (*action)(char * text, gpointer data), 
		gpointer data )
{
	do_text_input_window_multiline(title, value, 0, 1, action, data);
}

void do_text_input_window_multiline(gchar * title, gchar * value, 
		int ismulti, int ispassword, 
		void (*action)(char * text, gpointer data), 
		gpointer data )
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

	input_window->window = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_window_set_position(GTK_WINDOW(input_window->window), GTK_WIN_POS_MOUSE);
	gtk_widget_realize(input_window->window);

	gtk_container_set_border_width(GTK_CONTAINER(input_window->window), 5);
	label = gtk_label_new(title);

	if (ismulti)
		input_window->text = gtk_text_new(NULL, NULL);
	else
		input_window->text = gtk_entry_new();
	
	if (ispassword)
		gtk_entry_set_visibility(GTK_ENTRY(input_window->text), FALSE);
	
	gtk_widget_set_usize(input_window->text, 400, ismulti?200:-1);
	gtk_editable_insert_text(GTK_EDITABLE(input_window->text),
				 value, strlen(value), &dummy);
	gtk_editable_set_editable(GTK_EDITABLE(input_window->text),
			TRUE);

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

  	gtk_widget_set_usize(hbox2, 200,25);

	button = gtkut_create_icon_button( _("OK"), ok_xpm, input_window->window );
	     
	gtk_signal_connect(GTK_OBJECT(button), "clicked", input_window_ok,
			input_window);

	gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
				  GTK_SIGNAL_FUNC(gtk_widget_destroy),
				  GTK_OBJECT(input_window->window));
	     
	gtk_box_pack_start(GTK_BOX(hbox2), button, TRUE, TRUE, 0);
	gtk_widget_show(button);

	button = gtkut_create_icon_button( _("Cancel"), cancel_xpm, input_window->window );
	     
	gtk_signal_connect(GTK_OBJECT(button), "clicked", input_window_cancel,
			input_window);

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
	gtkut_set_window_icon(input_window->window->window, NULL);

	gtk_signal_connect(GTK_OBJECT(input_window->window), "destroy",
			GTK_SIGNAL_FUNC(input_window_destroy), input_window);

	gtk_widget_show(input_window->window);
}

