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

#ifndef __DIALOG__
#define __DIALOG__
#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

/*dialog.c*/
GtkWidget *eb_push_button(const char *text, GtkWidget *page);
GtkWidget *eb_button(const char *text, int *value, GtkWidget *page);

GSList * eb_radio (GSList * group, const char * text, int curr_val,
		   int set_val, GtkWidget *page, void * set_element);
/*
  Example usage of eb_radio:

int my_val;
void set_my_element (GtkWidget * w, int data)
  {
    my_val = data;
    // Do whatever other update stuffs.....
  }

int myfunc ()
{
  ....
  GtkWidget * hbox;
  GtkWidget * label;
  GList * group;

  // Setup initial Value
  my_val = 3;

  // Setup box to hold it
  hbox = gtk_hbox_new(FALSE, 5);

  // setup intro label
  label = gtk_label_new ("Option Group:");
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 5);

  // Setup group -- set to NULL to create new group
  group = NULL;

  // Create buttons
  group = eb_radio (group, "Opt 1", my_val, 1, hbox, set_my_element);
  group = eb_radio (group, "Opt 2", my_val, 2, hbox, set_my_element);
  group = eb_radio (group, "Opt 3", my_val, 3, hbox, set_my_element);
  group = eb_radio (group, "Opt 4", my_val, 4, hbox, set_my_element);

  // Don't forget to put hbox somewhere in a layout ;)
  gtk_widget_show (hbox);
  ....
}

*/

/* These need docs but I'm toooooo tired, atm.  Look in status.c :( */

GtkWidget * eb_menu_button (GtkMenu * menu, gchar * label,
			    GtkSignalFunc callback_func,
			    gpointer callback_arg);

GtkWidget * eb_menu_submenu (GtkMenu * menu, gchar * label,
			     GtkWidget *submenu, int nb);


/* TODO Check header in sound.c */
void do_message_dialog(char *message, char *title, int modal);
void do_error_dialog(char *message, char *title);
void do_dialog( gchar * message, gchar * title, void (*action)(GtkWidget * widget, gpointer data), gpointer data );
void do_list_dialog( gchar * message, gchar * title, char **list, void (*action)(char * text, gpointer data), gpointer data );
void do_text_input_window( gchar * title, gchar * value, void (*action)(char * text, gpointer data), gpointer data );
void do_text_input_window_multiline( gchar * title, gchar * value, int ismulti, void (*action)(char * text, gpointer data), gpointer data );
void eb_icon(GdkWindow *);
GtkWidget * do_icon_button (char *label, char **xpm, GtkWidget *w);

#ifdef __cplusplus
} /* extern "C" */
#endif

/*editcontacts.c*/
void eb_new_user();

#ifdef __cplusplus
extern "C" {
#endif

int progress_window_new( char * filename, unsigned long size );
void update_progress(int tag, unsigned long progress);
void progress_window_close(int tag);

/* defined in util.c */
gint gtk_notebook_get_number_pages(GtkNotebook *notebook);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
