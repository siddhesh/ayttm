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
#include <gtk/gtk.h>
#include <string.h>

#include "progress_window.h"

typedef struct {
	int tag;
	GtkWidget * progress_meter;
	GtkWidget * progress_window;
	void (*close_cb)(int);
	unsigned long size;
	int activity;
	int update_timeout;
} progress_window_data;

static GList *bars = NULL;
static int last = 0;

static void destroy(GtkWidget * widget, gpointer data)
{
	progress_window_data * pwd = data;
	GList * l;

	if (pwd->close_cb)
		pwd->close_cb(pwd->tag);
	
	for(l = bars; l; l = l->next)
	{
		if(pwd == l->data) {
			bars = g_list_remove_link(bars, l);
			g_free(pwd);

			/* small hack - reset last if there are no more bars */
			if(bars == NULL)
				last = 0;

			break;
		}
	}
}

static int activity_update_timeout(void * data)
{
	progress_window_data *pwd = data;
	int new_val = gtk_progress_get_value(GTK_PROGRESS(pwd->progress_meter)) + 3;
	GtkAdjustment *adj = GTK_PROGRESS(pwd->progress_meter)->adjustment;

	if (new_val > adj->upper)
		new_val = adj->lower;

	gtk_progress_set_value(GTK_PROGRESS(pwd->progress_meter), new_val);

	return 1;
}

static int _progress_window_new( char * title, unsigned long size, int activity )
{
	progress_window_data *pwd = g_new0(progress_window_data, 1);
	
	GtkWidget * vbox = gtk_vbox_new( FALSE, 5);
	GtkWidget * label;

	pwd->size = size;
	pwd->tag = ++last;
	pwd->activity = activity;

	label = gtk_label_new(title);

	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 5);
	gtk_widget_show(label);

	pwd->progress_meter = gtk_progress_bar_new();
	gtk_progress_bar_set_orientation(GTK_PROGRESS_BAR(pwd->progress_meter), 
			GTK_PROGRESS_LEFT_TO_RIGHT );

	if(activity) {
		gtk_progress_set_activity_mode(GTK_PROGRESS(pwd->progress_meter), TRUE);
	}

	gtk_box_pack_start(GTK_BOX(vbox),pwd->progress_meter, TRUE, TRUE, 5);
	gtk_widget_show(pwd->progress_meter);

	pwd->progress_window = gtk_window_new(GTK_WINDOW_DIALOG);
/*	gtk_window_set_position(GTK_WINDOW(pwd->progress_window), GTK_WIN_POS_MOUSE); */
	gtk_container_add(GTK_CONTAINER(pwd->progress_window), vbox);
    	gtk_signal_connect( GTK_OBJECT(pwd->progress_window), "destroy",
	                        GTK_SIGNAL_FUNC(destroy), pwd );

	gtk_widget_show(vbox);
	gtk_widget_show(pwd->progress_window);

	bars = g_list_append(bars, pwd);

	if(activity) {
		pwd->update_timeout = gtk_timeout_add(50, (void *)activity_update_timeout, pwd);
	}
	return pwd->tag;
}

int activity_window_new( char * title )
{
	return _progress_window_new( title, 150, 1 );
}

int progress_window_new( char * filename, unsigned long size )
{
	gchar buff[2048];
	g_snprintf( buff, sizeof(buff), _("Transfering %s"), filename);
	return _progress_window_new( buff, size, 0 );
}
		
void update_progress(int tag, unsigned long progress)
{
	GList * l;
	for(l = bars; l; l=l->next)
	{
		progress_window_data * pwd = l->data;
		if(pwd->tag == tag) {
			gtk_progress_set_percentage(GTK_PROGRESS(pwd->progress_meter), ((float)progress)/((float)pwd->size));
			break;
		}
	}
}

void progress_window_close(int tag)
{
	GList * l;
	for(l = bars; l; l=l->next)
	{
		progress_window_data * pwd = l->data;
		if(pwd->tag == tag) {
			/* reset close_cb as the plugin is
			probably aware of closing the window */
			if(pwd->activity)
				gtk_timeout_remove(pwd->update_timeout);
			pwd->close_cb = NULL;
			gtk_widget_destroy(pwd->progress_window);
			break;
		}
	}
}

void progress_window_set_close_cb(int tag, void (*close_cb)(int)) 
{
	GList * l;
	for(l = bars; l; l=l->next)
	{
		progress_window_data * pwd = l->data;
		if(pwd->tag == tag) {
			pwd->close_cb = close_cb;
			break;
		}
	}

}
