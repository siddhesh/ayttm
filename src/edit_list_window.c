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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "edit_list_window.h"
#include "gtk_globals.h"
#include "service.h"
#include "messages.h"
#include "dialog.h"

#include "gtk/gtkutils.h"

#include "pixmaps/ok.xpm"
#include "pixmaps/cancel.xpm"

#ifndef MIN
#define MIN(x, y)	((x)<(y)?(x):(y))
#endif

/* types */
typedef struct _data {
        char title[2048];
        GString *message;
} data; 

/* globals */
static char mbuf[1024];

static GtkWidget *text_entry = NULL;
static GtkWidget *window = NULL;
static GtkWidget *title = NULL;
static GtkWidget *save = NULL;
static LList *datalist = NULL;
static GtkWidget *data_clist = NULL;

static gint data_open = 0;

static char datatype[1024];

static char myfile[2048];
char bentity[1024], eentity[1024];
char bvalue[1024], evalue[1024];
	 

static void (*mycallback)(char * msg, void *cbdata);
static void *mycbdata;

static void write_data();
static void select_data_cb(GtkCList *clist, gint row, gint column,
                   GdkEventButton *event, gpointer user_data);

static void build_data_clist();

static void destroy( GtkWidget *widget, gpointer data)
{
	data_open = 0;
}

static void save_data (void);

static void load_data(char *file)
{
	FILE * fp;
	char buff[2048];
	data * my_data = NULL;
	gboolean reading_title=FALSE;
	gboolean reading_message=FALSE;

	g_snprintf (myfile, 1024, "%s", file);

	if(!(fp = fopen(myfile, "r")))
		return;

	datalist = NULL;

	while ( fgets(buff, sizeof(buff), fp))
	{
		g_strchomp(buff);
		if (!g_strncasecmp(buff, bentity, strlen(bentity))) {
			my_data = g_new0(data, 1);
		} else if (!g_strncasecmp( buff, eentity, strlen(eentity))) {
			datalist = l_list_append(datalist, my_data);
		} else if (!g_strncasecmp(buff, "<TITLE>", strlen("<TITLE>"))) {
			reading_title=TRUE;
		} else if (!g_strncasecmp(buff, "</TITLE>", strlen("</TITLE>"))) {
			reading_title=FALSE;
		} else if (!g_strncasecmp(buff, bvalue, strlen(bvalue))) {
			reading_message=TRUE;
			my_data->message = g_string_new(NULL);
		} else if (!g_strncasecmp(buff, evalue, strlen(evalue)))  {
			reading_message=FALSE;
		} else if(reading_title) {
			strncpy(my_data->title, buff, MIN(strlen(buff), sizeof(my_data->title)));
		} else if(reading_message) {
			if(my_data->message && my_data->message->len)
				g_string_append_c(my_data->message, '\n');
			g_string_append(my_data->message, buff);
		}
	}

	fclose(fp);
}

static void delete_data_cb(GtkWidget * menuitem, gpointer d)
{
	data *my_data = (data *)d;
	int i=0;
	data *ldata = NULL;
	
	datalist = l_list_remove(datalist, my_data);
	write_data(myfile);
	while(NULL != (ldata = (data*)gtk_clist_get_row_data(
			GTK_CLIST(data_clist),i))) {
		if (ldata == my_data) {
			gtk_clist_remove(GTK_CLIST(data_clist), i);
			break;
		}
		i++;
	}
}

static void deselect_data_cb(GtkCList *clist, gint row, gint column,
                   GdkEventButton *event, gpointer user_data) 
{
	if (event && event->button == 3) {
		gtk_signal_emit_stop_by_name(GTK_OBJECT(clist),
				   "unselect-row");
		select_data_cb(clist, row, column,
				event, user_data);
	}
}

static void select_data_cb(GtkCList *clist, gint row, gint column,
                   GdkEventButton *event, gpointer user_data) 
{
	data *my_data;
	
	my_data = (data *)gtk_clist_get_row_data(GTK_CLIST(data_clist),
				row);
	gtk_signal_handler_block_by_func(GTK_OBJECT(clist),
			select_data_cb, NULL);
	gtk_signal_handler_block_by_func(GTK_OBJECT(clist),
			deselect_data_cb, NULL);
	gtk_clist_select_row(clist, row, column);
	gtk_signal_handler_unblock_by_func(GTK_OBJECT(clist),
			select_data_cb, NULL);
	gtk_signal_handler_unblock_by_func(GTK_OBJECT(clist),
			deselect_data_cb, NULL);
	if (!event)
		return;
	switch (event->button) {
	case 1:
		if (my_data) {
			int dummy;
			gtk_entry_set_text(GTK_ENTRY(title), my_data->title);
			gtk_editable_delete_text(GTK_EDITABLE(text_entry), 0, -1);
			gtk_editable_insert_text(GTK_EDITABLE(text_entry), 
						my_data->message->str,
						strlen(my_data->message->str), &dummy);
		}
		break;
	case 3: {
		GtkWidget *menu = gtk_menu_new();
		gtkut_create_menu_button (GTK_MENU(menu), _("Delete"),
			GTK_SIGNAL_FUNC(delete_data_cb), my_data);
		gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
				event->button, event->time );
		}
		break;
	}
}

static void clicked_data_cb (GtkWidget *widget, GdkEventButton *event,
			    gpointer d)
{
	if (event->type == GDK_2BUTTON_PRESS && event->button == 1)
		save_data();
}

static void build_data_clist()
{
	LList * data_list = datalist;
	data * my_data = NULL;
	int i = 0;
	char *aw[2];
	aw[0] = _("Title");
	aw[1] = datatype; 
	data_clist = gtk_clist_new_with_titles(2, aw);
	
	gtk_clist_freeze(GTK_CLIST(data_clist));
			
	gtk_clist_set_column_auto_resize(GTK_CLIST(data_clist),
					0, TRUE);
	while (data_list) {
		my_data = (data *)data_list->data;
		
		aw[0] = g_strdup(my_data->title);
		aw[1] = g_strdup(my_data->message->str);
		
		if (strlen(aw[0]) > 30) {
			aw[0][27]=aw[1][28]=aw[1][29]='.';
			aw[0][30]='\0';
		}
		
		if (strlen(aw[1]) > 50) {
			aw[1][47]=aw[1][48]=aw[1][49]='.';
			aw[1][50]='\0';
		}
		
		if (strstr(aw[1], "\n")) {
			int p = (int)(strstr(aw[1], "\n") - aw[1]);
			aw[1][p]=aw[1][p+1]=aw[1][p+2]='.';
			aw[1][p+3]='\0';
			
		}
		
		gtk_clist_append(GTK_CLIST(data_clist), aw);
		
		gtk_clist_set_row_data(GTK_CLIST(data_clist), i, my_data);
		i++;
		data_list = data_list->next;
	}
	gtk_widget_show(data_clist);
	gtk_clist_set_button_actions(GTK_CLIST(data_clist), 1, 
			GTK_BUTTON_SELECTS);
	gtk_clist_set_button_actions(GTK_CLIST(data_clist), 2, 
			GTK_BUTTON_SELECTS);
	gtk_clist_set_button_actions(GTK_CLIST(data_clist), 3, 
			GTK_BUTTON_SELECTS);
	
	gtk_signal_connect(GTK_OBJECT(data_clist), "select-row",
			   GTK_SIGNAL_FUNC(select_data_cb), NULL);
	gtk_signal_connect(GTK_OBJECT(data_clist), "unselect-row",
			   GTK_SIGNAL_FUNC(deselect_data_cb), NULL);
	gtk_signal_connect(GTK_OBJECT(data_clist), "button_press_event",
			   GTK_SIGNAL_FUNC(clicked_data_cb), NULL);
	gtk_widget_set_usize(data_clist, -1, 100);
	gtk_clist_thaw(GTK_CLIST(data_clist));
}

static void write_data()
{
	LList * data_list = datalist;
	FILE *fp;
	char buff2[2048];
	data * my_data;

	if(!(fp = fdopen(creat(myfile, 0700),"w")))
		return;

	while (data_list) {
		my_data = (data *)data_list->data;

		fprintf(fp,bentity);
		fprintf(fp,"\n");
		fprintf(fp,"<TITLE>\n");
		strncpy(buff2, my_data->title, strlen(my_data->title) + 1);
		g_strchomp(buff2);
		fprintf(fp,"%s\n",buff2);
		fprintf(fp,"</TITLE>\n");
		fprintf(fp,bvalue);
		fprintf(fp,"\n");
		strncpy(buff2, my_data->message->str, strlen(my_data->message->str) + 1);
		g_strchomp(buff2);
		fprintf(fp,"%s\n",buff2);
		fprintf(fp,evalue);
		fprintf(fp,"\n");
		fprintf(fp,eentity);
		fprintf(fp,"\n");

		data_list = data_list->next;
	}	
	fclose(fp);
}

static LList * replace_data(LList *list, data *msg)
{
	LList *w = list;
	while (w) {
		data * omsg = (data *)w->data;
		if (!strcmp(omsg->title, msg->title)) {
			w->data = msg;
			return list;
		}
		w = w->next;
	}
	/* not found */
	return l_list_append(list, msg);
}

static void check_title( GtkWidget * widget, gpointer d)
{
	LList *w = datalist;
	char *txt = gtk_entry_get_text(GTK_ENTRY(title));
	int replace = FALSE;
	GList *ch = gtk_container_children(GTK_CONTAINER(save));
	while (w) {
		data * omsg = (data *)w->data;
		if (!strcmp(omsg->title, txt)) {
			replace = TRUE;
			break;
		}
		w = w->next;
	}
	
	while(ch) {
		if (GTK_IS_LABEL(ch->data)) {
			char *lab = NULL;
			gtk_label_get(GTK_LABEL(ch->data), &lab);
			if(replace && !strcmp(lab,_("Save"))) 
				gtk_label_set_text(GTK_LABEL(ch->data), 
						_("Replace saved"));
			if(!replace && !strcmp(lab,_("Replace saved"))) 
				gtk_label_set_text(GTK_LABEL(ch->data), 
						_("Save"));
			break;
		}
		ch = ch->next;
	}
}

static void ok_callback( GtkWidget * widget, gpointer data)
{
	save_data(); 
}

static void save_data (void)
{
	GString * a_title = g_string_sized_new(1024);
	GString * a_message = g_string_sized_new(1024);
	gint dosave = GTK_TOGGLE_BUTTON(save)->active;
	data * my_data;

	gchar *buff = NULL;

	buff = strdup(gtk_entry_get_text(GTK_ENTRY(title)));

	if (!buff || strlen(buff) == 0) {
		buff= gtk_editable_get_chars(GTK_EDITABLE(text_entry),0,-1);
		if (strchr(buff,'\n') != NULL) {
			char *t = strchr(buff,'\n');
			*t = 0;
		}
	}
	if (!buff || strlen(buff) == 0) {
		snprintf(mbuf, 1024, _("You have to fill in \"%s\"."), datatype);
		ay_do_error( _("Input missing"), mbuf );
		return;
	}
	g_string_append(a_title, buff); 

	free(buff);
	
	buff = gtk_editable_get_chars(GTK_EDITABLE(text_entry),0,-1);
	
	if (!buff || strlen(buff) == 0) /* in this case title can't be empty */
		buff = strdup(a_title->str);
	
	g_string_append(a_message, buff);
	
	free (buff);
	
	if (dosave == 1) {
		my_data = g_new0(data, 1);
		strncpy(my_data->title, a_title->str, strlen(a_title->str));
		my_data->message = g_string_new(NULL);
		g_string_append(my_data->message, a_message->str);
		datalist = replace_data(datalist, my_data);
		write_data();
		build_data_clist();
	}
	
	if (mycallback)
		mycallback(a_message->str, mycbdata);

	g_string_free(a_title, TRUE);
	g_string_free(a_message, TRUE);

	gtk_widget_destroy(window);
}

static void cancel_callback( GtkWidget *widget, gpointer data)
{
	gtk_widget_destroy(window);
}

void show_data_choicewindow(
		char *file,
		char *dtype,
		char *ok_button_label,
		char *help,
		char *entityname,
		char *valuename,
		void (*cb)(char *msg, void *data),
		void *cbdata
		)
{
	if ( !data_open ) {
		GtkWidget * vbox;
		GtkWidget * vbox2;
		GtkWidget * hbox;
		GtkWidget * hbox2;
		GtkWidget * label;
		GtkWidget * frame;
		GtkWidget * table;
		GtkWidget * separator;
		GtkWidget * button;
		GtkWidget * scrollwindow;
		
		snprintf(datatype, 1024, "%s", dtype);
		mycallback = cb;
		mycbdata = cbdata;
		
		snprintf(bentity, 1024, "<%s>", entityname);
		snprintf(eentity, 1024, "</%s>", entityname);
		snprintf(bvalue, 1024, "<%s>", valuename);
		snprintf(evalue, 1024, "</%s>", valuename);
		
		window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_window_set_policy(GTK_WINDOW(window), TRUE, TRUE, TRUE);
		gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_MOUSE);
		gtk_widget_realize(window);

		vbox = gtk_vbox_new(FALSE, 5);
	
		table = gtk_table_new(2, 3, FALSE);

		hbox = gtk_hbox_new(FALSE, 5);
		label = gtk_label_new(_("Title: "));
		gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 5);
		gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 0, 1,
				 GTK_FILL, GTK_FILL, 0, 0);
		gtk_widget_show(label);
		gtk_widget_show(hbox);

		title = gtk_entry_new();
		gtk_table_attach(GTK_TABLE(table), title, 1, 2, 0, 1,
				 GTK_FILL, GTK_FILL, 0, 0);
		gtk_widget_show(title);
		
		gtk_signal_connect(GTK_OBJECT(title), "changed",
				GTK_SIGNAL_FUNC(check_title), NULL);
		
		hbox = gtk_hbox_new(FALSE, 5);
		label = gtk_label_new(_("Value:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0);
		gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 5);
		gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 1, 2,
				 GTK_FILL, GTK_FILL, 0, 0);
		gtk_widget_show(label);
		gtk_widget_show(hbox);
		
		vbox2 = gtk_vbox_new(FALSE, 0);
		gtk_widget_show(vbox2);
		
		if (help) {
			label = gtk_label_new(help);
			gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
			gtk_widget_set_usize(text_entry, 300, -1);
			gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
			gtk_widget_show(label);
			gtk_box_pack_start(GTK_BOX(vbox2), label, TRUE, TRUE, 0);
		}
		text_entry = gtk_text_new(NULL,NULL);
		gtk_text_set_editable(GTK_TEXT(text_entry), TRUE);
		gtk_widget_set_usize(text_entry, 300, 60);
		gtk_box_pack_start(GTK_BOX(vbox2), text_entry, TRUE, TRUE, 0);
		gtk_table_attach_defaults(GTK_TABLE(table), vbox2, 1, 2, 1, 2);
		gtk_widget_show(text_entry);

		save = gtk_check_button_new_with_label(_("Save"));
		gtk_table_attach(GTK_TABLE(table), save, 1, 2, 2, 3,
				 GTK_FILL, GTK_FILL, 0, 0);
		gtk_widget_show(save);

		snprintf(mbuf, 1024, _("New %s"), datatype);
		frame = gtk_frame_new(mbuf);
		gtk_container_add(GTK_CONTAINER(frame), table);
		gtk_widget_show(table);

		gtk_table_set_row_spacings (GTK_TABLE(table), 5);
		gtk_table_set_col_spacings (GTK_TABLE(table), 5);

		gtk_container_set_border_width(GTK_CONTAINER(window), 5);
		gtk_container_set_border_width(GTK_CONTAINER(table), 5);

		gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 5);
		gtk_widget_show(frame);

		load_data(file);
		build_data_clist();
		scrollwindow = gtk_scrolled_window_new(NULL, NULL);
		gtk_scrolled_window_set_policy(
			   GTK_SCROLLED_WINDOW (scrollwindow), 
			   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
		gtk_container_add(GTK_CONTAINER(scrollwindow), data_clist);
		
		snprintf(mbuf, 1024, _("Existing %ss"), datatype);
		frame = gtk_frame_new(mbuf);
		gtk_container_add(GTK_CONTAINER(frame), scrollwindow);
		gtk_container_set_border_width(GTK_CONTAINER(frame), 5);

		gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 5);
		gtk_widget_show(scrollwindow);
		gtk_widget_show(frame);
		
		/* seperator goes here */

		separator = gtk_hseparator_new();
		gtk_box_pack_start(GTK_BOX(vbox), separator, FALSE, FALSE, 5);
		gtk_widget_show(separator);
		
		/* 'OK' and 'Cancel' buttons go here */

		hbox2 = gtk_hbox_new(TRUE, 5);

		gtk_widget_set_usize(hbox2, 200, 25);

		/* put in the pixmap and label for the 'OK' button */

		button = do_icon_button(ok_button_label, ok_xpm, window); 

		gtk_signal_connect(GTK_OBJECT(button), "clicked",
				GTK_SIGNAL_FUNC(ok_callback), NULL);

		gtk_box_pack_start(GTK_BOX(hbox2), button, TRUE, TRUE, 0);
		gtk_widget_show(button);

		/* now start on the cancel button */

		button = do_icon_button(_("Cancel"), cancel_xpm, window);

		gtk_signal_connect(GTK_OBJECT(button), "clicked",
				GTK_SIGNAL_FUNC(cancel_callback), NULL);

		gtk_box_pack_start(GTK_BOX(hbox2), button, TRUE, TRUE, 0);
		gtk_widget_show(button);

		hbox = gtk_hbox_new(FALSE, 0);
		
		gtk_box_pack_end(GTK_BOX(hbox), hbox2, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

		gtk_widget_show(hbox);
		gtk_widget_show(hbox2);
		gtk_widget_show(vbox);
		
		gtk_container_add(GTK_CONTAINER(window), vbox);
				
		gtk_widget_show(window);
		gtk_widget_grab_focus(title);

	}
	snprintf(mbuf, 1024, _("Edit %ss"), datatype);
	gtk_window_set_title(GTK_WINDOW(window), mbuf);
	gtkut_set_window_icon(window->window, NULL);
	gtk_signal_connect(GTK_OBJECT(window), "destroy",
			GTK_SIGNAL_FUNC(destroy), NULL);
	data_open = 1; 
}
