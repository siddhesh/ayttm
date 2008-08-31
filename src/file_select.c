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

//#include "ui_file_selection_dlg.h"
#include <gtk/gtk.h>
#include "file_select.h"

void ay_do_file_selection_open( const char *inDefaultFile, const char *inWindowTitle, 
			 t_file_selection_callback *inCallback, void *inData )
{
	const char *selected_file = NULL;
	GtkWidget *dialog = NULL;

	dialog = gtk_file_chooser_dialog_new( inWindowTitle, NULL,
			GTK_FILE_CHOOSER_ACTION_OPEN,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
			NULL);

	if(inDefaultFile)
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), inDefaultFile);

	if(gtk_dialog_run(GTK_DIALOG(dialog))  == GTK_RESPONSE_ACCEPT) 
		selected_file = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(dialog) );
	else 
		selected_file = NULL;

	(inCallback)( selected_file, inData );
	gtk_widget_destroy(dialog);
}


void ay_do_file_selection_save( const char *inDefaultFile, const char *inWindowTitle, 
			 t_file_selection_callback *inCallback, void *inData )
{
	const char *selected_file = NULL;
	GtkWidget *dialog = NULL;
	char url[255];
	char *curr_folder = NULL;

	dialog = gtk_file_chooser_dialog_new( inWindowTitle, NULL,
			GTK_FILE_CHOOSER_ACTION_SAVE,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
			NULL);

	if ( (curr_folder = gtk_file_chooser_get_current_folder ( GTK_FILE_CHOOSER(dialog) ) ) )
		snprintf(url, sizeof(url), "%s/%s", curr_folder, (inDefaultFile?inDefaultFile:""));
	else
		snprintf(url, sizeof(url), "/%s", (inDefaultFile?inDefaultFile:""));

	gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), url);

	if(gtk_dialog_run(GTK_DIALOG(dialog))  == GTK_RESPONSE_ACCEPT) 
		selected_file = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(dialog) );
	else 
		selected_file = NULL;

	(inCallback)( selected_file, inData );
	gtk_widget_destroy(dialog);
}


