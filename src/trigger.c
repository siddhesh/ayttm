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
#include "globals.h"
#include "file_select.h"
#include "util.h"

#include "pixmaps/tb_preferences.xpm"
#include "pixmaps/cancel.xpm"

static gint window_open = 0;
static GtkWidget *edit_trigger_window;
static GtkWidget *trigger_list;
static GtkWidget *account_name;
static GtkWidget *parameter;
static GtkWidget *action_name;

static GList *triggers = NULL;
static GList *actions = NULL;

static void quick_message(gchar *message) {

  GtkWidget *dialog, *label, *okay_button;
   
  /* Create the widgets */
   
  dialog = gtk_dialog_new();
  label = gtk_label_new (message);
  okay_button = gtk_button_new_with_label(_("Okay"));
   
  /* Ensure that the dialog box is destroyed when the user clicks ok. */
   
  gtk_signal_connect_object (GTK_OBJECT (okay_button), "clicked",
			     GTK_SIGNAL_FUNC (gtk_widget_destroy), GTK_OBJECT(dialog));
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->action_area),
		     okay_button);

  /* Add the label, and show everything we've added to the dialog. */

  gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->vbox),
		     label);
  gtk_widget_show_all (dialog);
}

static void pounce_contact(struct contact *con, char *str) 
{
  gint pos = 0;

  eb_chat_window_display_contact(con);
  gtk_editable_insert_text(GTK_EDITABLE(con->chatwindow->entry), str, strlen(str), &pos);
  send_message(NULL, con->chatwindow);			   
}

void do_trigger_action(struct contact *con, int trigger_type)
{
    gchar param_string[2048];
    gchar *substr;
    gchar *basestr;
    
    strcpy(param_string, "");
    substr = NULL;
    
    if(con->trigger.action == NO_ACTION)
	return;
    if(strlen(con->trigger.param) >= (sizeof(param_string)/2))
    {
	eb_debug(DBG_CORE, "Trigger parameter too long - ignoring");
	return;
    }
    /* replace all occurrences of %t with "online" or "offline" */
    basestr = con->trigger.param;
    while ((substr = strstr(basestr, "%t")) != NULL)
    {
	if (substr[-1] == '%')
	{
	    strncat(param_string, basestr, (size_t)(substr-basestr + 2));
	    basestr=substr + 2;
	    continue;
	}
	else
	{
	    strncat(param_string, basestr, (size_t)(substr-basestr));
	    strncat(param_string, ((trigger_type == USER_ONLINE) ?
				    _("online") : _("offline")), 
			    	    (size_t)(sizeof(param_string)-strlen(param_string)));
	    basestr = substr + 2;
	    }
	    if((strlen(param_string) + strlen(basestr) + 8) >
		sizeof(param_string))
	    {
		eb_debug(DBG_CORE, "Result string may be too long, no substitution done\n");
		basestr = con->trigger.param;
		strcpy(param_string, "");
		break;
	    }
	}
	/* copy remainder (or all if no subst done) */
	strncat(param_string, basestr, (size_t)(sizeof(param_string)-strlen(param_string)));
	
	if(con->trigger.action == PLAY_SOUND)
	{
	    playsoundfile(param_string);
	} else if(con->trigger.action == EXECUTE)
	{
	    system(param_string);
	} else if(con->trigger.action == DIALOG)
	{
		quick_message(param_string);
	} else if(con->trigger.action == POUNCE && con->trigger.type == USER_ONLINE)
	{
		pounce_contact(con, param_string);
	}

}

void do_trigger_online(struct contact *con)
{
    if ((con->trigger.type == USER_ONLINE) || (con->trigger.type == USER_ON_OFF_LINE))
    {
	do_trigger_action(con, USER_ONLINE);
    }
}

void do_trigger_offline(struct contact *con)
{
    if((con->trigger.type == USER_OFFLINE) || (con->trigger.type == USER_ON_OFF_LINE))
    {
	do_trigger_action(con, USER_OFFLINE);
    }
}

static void browse_done(char * filename, gpointer data)
{
	if(!filename)
		return;
  gtk_entry_set_text(GTK_ENTRY(parameter), filename);
}

static void set_button_callback(GtkWidget* widget, struct contact * con)
{
  strncpy( con->trigger.param, gtk_entry_get_text(GTK_ENTRY(parameter)),
		sizeof(con->trigger.param));
  con -> trigger.type = g_list_index(GTK_LIST(GTK_COMBO(trigger_list)->list)->children, 
				     GTK_LIST(GTK_COMBO(trigger_list)->list)->selection->data);
  con -> trigger.action = g_list_index(GTK_LIST(GTK_COMBO(action_name)->list)->children, 
				       GTK_LIST(GTK_COMBO(action_name)->list)->selection->data);

  write_contact_list();
	
  destroy_window();
}

void destroy_window()
{
  window_open = 0;
	
  gtk_widget_destroy(edit_trigger_window);
}

static void browse_file(GtkWidget* widget, gpointer data)
{
	eb_do_file_selector(NULL, _("Select parameter"), browse_done, NULL);
}


trigger_type get_trigger_type_num(gchar *text)
{
  if(!text) return -1;

  if(!strcmp(text, "USER_ONLINE"))
    {
      return USER_ONLINE;
    }	
  else if(!strcmp(text, "USER_OFFLINE"))
    {
      return USER_OFFLINE;
    }
    else if(!strcmp(text, "USER_ON_OFF_LINE"))
    {
	return USER_ON_OFF_LINE;
    }
	
  return -1;
}

trigger_action get_trigger_action_num(gchar *text)
{
  if(!text) return -1;
	
  if(!strcmp(text, "PLAY_SOUND"))
    {
      return PLAY_SOUND;
    }
  else if(!strcmp(text, "EXECUTE"))
    {
      return EXECUTE;
    } 
  else if(!strcmp(text, "DIALOG"))
    {
      return DIALOG;
    }
  else if(!strcmp(text, "POUNCE"))
    {
      return POUNCE;
    }
	
  return -1;
}

gchar* get_trigger_type_text(trigger_type type)
{
  if(type == USER_ONLINE)
    return "USER_ONLINE";
  else if(type == USER_OFFLINE)
    return "USER_OFFLINE";
    else if(type == USER_ON_OFF_LINE)
    return "USER_ON_OFF_LINE";
    else
    return "UNKNOWN";
		
}

gchar* get_trigger_action_text(trigger_action action)
{
  if(action == PLAY_SOUND)
    return "PLAY_SOUND";
  else if(action == EXECUTE)
    return "EXECUTE";
  else if(action == DIALOG)
    return "DIALOG";
  else if(action == POUNCE) 
    return "POUNCE";
  else
    return "UNKNOWN";
}

static GList *get_trigger_list()
{
  triggers = NULL;

  triggers = g_list_append(triggers, _("None"));
  triggers = g_list_append(triggers, _("User goes online"));
  triggers = g_list_append(triggers, _("User goes offline"));
  triggers = g_list_append(triggers, _("User goes on or offline"));
	
  return triggers;
}

static GList *get_action_list()
{
  actions = NULL;

  actions = g_list_append(actions, _("None"));
  actions = g_list_append(actions, _("Play sound"));
  actions = g_list_append(actions, _("Execute command"));
  actions = g_list_append(actions, _("Show alert dialog"));
  actions = g_list_append(actions, _("Send message"));
  return actions;
}


void show_trigger_window(struct contact * con)
{
  GtkWidget *hbox;
  GtkWidget *hbox2;
  GtkWidget *vbox;
  GtkWidget *label;
  GtkWidget *button;
  GtkWidget *iconwid;
  GtkWidget *table;
  GtkWidget *frame;
  GtkWidget *separator;
  GList *list;
  GdkPixmap *icon;
  GdkBitmap *mask;
  GtkWidget *hbox_param;
  GtkWidget *browse_button;
     
  if(window_open) return;
  window_open = 1;

  edit_trigger_window = gtk_window_new(GTK_WINDOW_DIALOG);
  gtk_window_set_position(GTK_WINDOW(edit_trigger_window), GTK_WIN_POS_MOUSE);
  gtk_widget_realize(edit_trigger_window);
  gtk_container_set_border_width(GTK_CONTAINER(edit_trigger_window), 5);      
     
  table = gtk_table_new(4, 2, FALSE);
  gtk_table_set_row_spacings(GTK_TABLE(table), 5);
  gtk_table_set_row_spacings(GTK_TABLE(table), 5);
  gtk_container_set_border_width(GTK_CONTAINER(table), 5);
  vbox = gtk_vbox_new(FALSE, 5);
  hbox = gtk_hbox_new(FALSE, 0);
     
  /*Section for letting the user know which trigger they are editing*/
     
  label = gtk_label_new(_("User: "));
  gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 5);
  gtk_widget_show(label);
  gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(hbox);
     
  account_name = gtk_label_new(con -> nick);
  gtk_misc_set_alignment(GTK_MISC(account_name), 0, .5);
  gtk_table_attach(GTK_TABLE(table), account_name, 1, 2, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(account_name);
     
  /*Section for declaring the trigger*/
  hbox = gtk_hbox_new(FALSE, 0);
     
  label = gtk_label_new(_("Trigger: "));
  gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 5);
  gtk_widget_show(label);
  gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(hbox);
     
  /*List of trigger types*/
     
  trigger_list = gtk_combo_new();
  list = get_trigger_list();
  gtk_combo_set_popdown_strings(GTK_COMBO(trigger_list), list );
  gtk_list_select_item(GTK_LIST(GTK_COMBO(trigger_list)->list), con->trigger.type);
	
  gtk_editable_set_editable(GTK_EDITABLE(GTK_COMBO(trigger_list)->entry), 0);
  g_list_free(list);
  gtk_table_attach(GTK_TABLE(table), trigger_list, 1, 2, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(trigger_list);
     
  /*Section for action declaration*/
     
  hbox = gtk_hbox_new(FALSE, 0);
     
  label = gtk_label_new(_("Action: "));
  gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 5);
  gtk_widget_show(label);
  gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 2, 3, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(hbox);
     
  /*List of available actions*/
  action_name = gtk_combo_new();
  list = get_action_list();
  gtk_combo_set_popdown_strings(GTK_COMBO(action_name), list );

  gtk_list_select_item(GTK_LIST(GTK_COMBO(action_name)->list), con->trigger.action);

  g_list_free(list);
  gtk_table_attach(GTK_TABLE(table), action_name, 1, 2, 2, 3, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(action_name);
      
  /*Section for Contact Name*/
  hbox = gtk_hbox_new(FALSE, 0);
      
  label = gtk_label_new(_("Parameter: "));
  gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 5);
  gtk_widget_show(label);
  gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 3, 4, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(hbox);
     
  /*Entry for parameter*/
  hbox_param = gtk_hbox_new(FALSE, 0);
  parameter = gtk_entry_new();
	
  gtk_entry_set_text(GTK_ENTRY(parameter), con->trigger.param);
	
  gtk_box_pack_start(GTK_BOX(hbox_param), parameter, FALSE, FALSE, 0);
	
  browse_button = gtk_button_new_with_label(_("Browse"));
  gtk_signal_connect(GTK_OBJECT(browse_button), "clicked",
		     GTK_SIGNAL_FUNC(browse_file),
		     con);
  gtk_box_pack_start(GTK_BOX(hbox_param), browse_button, FALSE, FALSE, 0);
	
  gtk_table_attach(GTK_TABLE(table), hbox_param, 1, 2, 3, 4, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(parameter);
  gtk_widget_show(browse_button);
  gtk_widget_show(hbox_param);
	

  /*Lets create a frame to put all of this in*/
  frame = gtk_frame_new(NULL);
  gtk_frame_set_label(GTK_FRAME(frame), _("Edit Trigger"));

  gtk_container_add(GTK_CONTAINER(frame), table);
  gtk_widget_show(table);
      
  gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);
  gtk_widget_show(frame);
     
  /*Lets try adding a separator*/
  separator = gtk_hseparator_new();
  gtk_box_pack_start(GTK_BOX(vbox), separator, FALSE, FALSE, 5);
  gtk_widget_show(separator);
		
  hbox = gtk_hbox_new(FALSE, 5);
  hbox2 = gtk_hbox_new(TRUE, 5);
     
  /*Add Button*/
  gtk_widget_set_usize(hbox2, 200,25);
      
  icon = gdk_pixmap_create_from_xpm_d(edit_trigger_window->window, &mask, NULL, tb_preferences_xpm);
  iconwid = gtk_pixmap_new(icon, mask);
  label = gtk_label_new(_("Update"));
      
  gtk_box_pack_start(GTK_BOX(hbox), iconwid, FALSE, FALSE, 2);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
      
  gtk_widget_show(iconwid);
  gtk_widget_show(label);
      
  button = gtk_button_new();
      
  gtk_signal_connect(GTK_OBJECT(button), "clicked",
		     GTK_SIGNAL_FUNC(set_button_callback),
		     con);
  gtk_widget_show(hbox);     
     
  gtk_container_add(GTK_CONTAINER(button), hbox);		
     
  gtk_box_pack_start(GTK_BOX(hbox2), button, TRUE, TRUE, 0);
  gtk_widget_show(button);
      
  /*Cancel Button*/
      
  hbox = gtk_hbox_new(FALSE, 5);
  icon = gdk_pixmap_create_from_xpm_d(edit_trigger_window->window, &mask, NULL, cancel_xpm);
  iconwid = gtk_pixmap_new(icon, mask);
  label = gtk_label_new(_("Cancel"));
     
  gtk_box_pack_start(GTK_BOX(hbox), iconwid, FALSE, FALSE, 2);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
     
  gtk_widget_show(iconwid);
  gtk_widget_show(label);
     
  button = gtk_button_new();
     
  gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
			    GTK_SIGNAL_FUNC(gtk_widget_destroy),
			    GTK_OBJECT(edit_trigger_window));
  gtk_widget_show(hbox);     
     
  gtk_container_add(GTK_CONTAINER (button), hbox);		
     
  gtk_box_pack_start(GTK_BOX(hbox2), button, TRUE, TRUE, 0);
  gtk_widget_show(button);
      
  /*Buttons End*/
      
  hbox = gtk_hbox_new(FALSE, 0);
  table = gtk_table_new(1, 1, FALSE);
      
	
  gtk_box_pack_end(GTK_BOX(hbox),hbox2, FALSE, FALSE, 0);
  gtk_widget_show(hbox2);      
      
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show(hbox);
      

  gtk_table_attach(GTK_TABLE(table), vbox, 0, 1, 0, 1, GTK_EXPAND, GTK_EXPAND, 0, 0);
  gtk_widget_show(vbox);
     
  gtk_container_add(GTK_CONTAINER(edit_trigger_window), table);
	
  gtk_widget_show(table);
      

  gtk_window_set_title(GTK_WINDOW(edit_trigger_window), _("Ayttm - Edit Trigger"));
  eb_icon(edit_trigger_window->window); 
  gtk_widget_show(edit_trigger_window);
     
  gtk_signal_connect(GTK_OBJECT(edit_trigger_window), "destroy",
		     GTK_SIGNAL_FUNC(destroy_window), NULL);
      
  window_open = 1;
}
