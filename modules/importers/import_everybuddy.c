/*
 * Ayttm 
 *
 * Copyright (C) 2003, the Ayttm team
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
#include <errno.h>

#include <gtk/gtk.h>

#ifdef __MINGW32__
#define __IN_PLUGIN__ 1
#endif
#include "util.h"
#include "globals.h"
#include "service.h"
#include "prefs.h"
#include "plugin_api.h"
#include "messages.h"


/*************************************************************************************
 *                             Begin Module Code
 ************************************************************************************/
/*  Module defines */
#ifndef USE_POSIX_DLOPEN
	#define plugin_info import_everybuddy_LTX_plugin_info
	#define plugin_init import_everybuddy_LTX_plugin_init
	#define plugin_finish import_everybuddy_LTX_plugin_finish
	#define module_version import_everybuddy_LTX_module_version
#endif


/* Function Prototypes */
void import_eb_accounts(ebmCallbackData *data);
int plugin_init();
int plugin_finish();

static int ref_count=0;

/*  Module Exports */
PLUGIN_INFO plugin_info = {
	PLUGIN_IMPORTER, 
	"Everybuddy Settings", 
	"Imports your Everybuddy settings into Ayttm", 
	"$Revision: 1.14 $",
	"$Date: 2009/08/26 16:25:56 $",
	&ref_count,
	plugin_init,
	plugin_finish
};
/* End Module Exports */

static void *buddy_list_tag=NULL;

unsigned int module_version() {return CORE_VERSION;}

int plugin_init()
{
	eb_debug(DBG_MOD,"EB Buddy List init\n");
	buddy_list_tag=eb_add_menu_item("Everybuddy Settings", EB_IMPORT_MENU, 
				import_eb_accounts, ebmIMPORTDATA, NULL);
	if(!buddy_list_tag)
		return(-1);
	return(0);
}

int plugin_finish()
{
	int result;

	result=eb_remove_menu_item(EB_IMPORT_MENU, buddy_list_tag);
	if(result) {
		g_warning("Unable to remove eb Buddy List menu item from  menu!");
		return(-1);
	}
	return(0);
}

/************************************************************************************
 *                             End Module Code
 ************************************************************************************/

static GtkWidget *window;
static GtkWidget *vbox;
static GtkWidget *hbox;
static GtkWidget *accountsbutton;
static GtkWidget *contactsbutton;
static GtkWidget *prefsbutton;
static GtkWidget *awaybutton;
static GtkWidget *okbutton;
static GtkWidget *cancelbutton;
static GtkWidget *label;
static int eb_imp_window_open = 0;

static void ok_callback(GtkWidget * widget, gpointer data) {
	
	char buff[1024];
	int a=0,c=0,p=0,m=0;
	
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(accountsbutton))) {
		snprintf(buff, 1024, "%s/.everybuddy/accounts", getenv("HOME"));
		if (!load_accounts_from_file(buff)) {
			ay_do_error(_("Import error"), _("Cannot import accounts.\n"
					"Check that ~/.everybuddy/accounts exists "
					"and is readable."));
		} else {
			a=1;
		}
	}
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(contactsbutton))) {
		snprintf(buff, 1024, "%s/.everybuddy/contacts", getenv("HOME"));
		if (!load_contacts_from_file(buff)) {
			ay_do_error(_("Import error"), _("Cannot import contacts.\n"
					"Check that ~/.everybuddy/contacts exists "
					"and is readable."));
		} else
			c=1;	
	}
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsbutton))) {
		/* prefs we want to save */
		char saved[7][MAX_PREF_LEN];
		FILE *in;
		
		strncpy(saved[0], cGetLocalPref("BuddyArriveFilename"), MAX_PREF_LEN);
		strncpy(saved[1], cGetLocalPref("BuddyAwayFilename"), MAX_PREF_LEN);
		strncpy(saved[2], cGetLocalPref("BuddyLeaveFilename"), MAX_PREF_LEN);
		strncpy(saved[3], cGetLocalPref("SendFilename"), MAX_PREF_LEN);
		strncpy(saved[4], cGetLocalPref("ReceiveFilename"), MAX_PREF_LEN);
		strncpy(saved[5], cGetLocalPref("FirstMsgFilename"), MAX_PREF_LEN);
		strncpy(saved[6], cGetLocalPref("modules_path"), MAX_PREF_LEN);

		snprintf(buff, 1024, "%s/.everybuddy/prefs", getenv("HOME"));
		in = fopen(buff, "r");
		if (in) {
			fclose (in);
			ayttm_prefs_read_file(buff);

			cSetLocalPref("BuddyArriveFilename", saved[0]);
			cSetLocalPref("BuddyAwayFilename", saved[1]);
			cSetLocalPref("BuddyLeaveFilename", saved[2]);
			cSetLocalPref("SendFilename", saved[3]);
			cSetLocalPref("ReceiveFilename", saved[4]);
			cSetLocalPref("FirstMsgFilename", saved[5]);
			cSetLocalPref("modules_path", saved[6]);

			ayttm_prefs_write();
			p=1;
		} else {
			ay_do_error(_("Import error"), 
					_("Cannot import preferences.\n"
					"Check that ~/.everybuddy/preferences "
					"exists and is readable."));			
		}
			
	}
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(awaybutton))) {
		FILE *in = NULL, *out = NULL;
		snprintf(buff, 1024, "%s/.everybuddy/away_messages", getenv("HOME"));
		in = fopen(buff, "r");
		if (in) {
			snprintf(buff, 1024, "%saway_messages", config_dir);
			out = fopen(buff, "a");
			if (!out) {
				ay_do_error(_("Import error"),
						 _("Cannot save away messages.\n"
						"Check that ~/.ayttm/away_messages "
						"is writable."));
			} else {
				while (fgets(buff, 1024, in) != NULL)
					fputs(buff, out);
				fclose(out);
				m=1;
			}
			fclose(in);
		} else {
			ay_do_error(_("Import error"), 
					_("Cannot import away messages.\n"
					"Check that ~/.everybuddy/away_messages "
					"exists and is readable."));			
		}
	}
	
	if (!a && !c && !p && !m) {
		return;
	} else {
		char message[1024];
		snprintf(message, 1024, "Successfully imported %s%s%s%s%s%s%s."
				"from Everybuddy.",
				a?"accounts":"",
				(a && (c||p||m))?", ":"",
				c?"contacts":"",
				(c && (p||m))?", ":"",
				p?"preferences":"",
				(p && m)?", ":"",
				m?"away messages":""
				);
		ay_do_info(_("Import success"), message);
	}
	gtk_widget_destroy(window);
	ay_set_submenus();
	set_menu_sensitivity();
}

static void cancel_callback(GtkWidget * widget, gpointer data) {
	gtk_widget_destroy(window);
}

static void destroy_callback(GtkWidget * widget, gpointer data) {
	eb_imp_window_open = 0;
}


void import_eb_accounts(ebmCallbackData *data)
{
	if (!eb_imp_window_open) {
		eb_imp_window_open = 1;
		window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_MOUSE);
		gtk_window_set_title(GTK_WINDOW(window), _("Import parameters"));
		gtk_widget_realize(window);
		gtk_container_set_border_width(GTK_CONTAINER(window), 5);
		
		vbox = gtk_vbox_new(FALSE, 5);
		
		label = gtk_label_new(_("Select which parts of your everybuddy "
				"configuration to import.\n"));
		
		accountsbutton  = gtk_check_button_new_with_label(
					_("Import local accounts"));
		contactsbutton  = gtk_check_button_new_with_label(
					_("Import contacts"));
		prefsbutton     = gtk_check_button_new_with_label(
					_("Import preferences"));
		awaybutton      = gtk_check_button_new_with_label(
					_("Import away messages"));
		
		okbutton = gtk_button_new_from_stock(GTK_STOCK_OK);
		cancelbutton = gtk_button_new_from_stock(GTK_STOCK_CANCEL);

		hbox = gtk_hbox_new(FALSE, 5);
		gtk_box_pack_start(GTK_BOX(hbox), okbutton, FALSE, FALSE, 2);
		gtk_box_pack_start(GTK_BOX(hbox), cancelbutton, FALSE, FALSE, 2);

		gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 2);
		gtk_box_pack_start(GTK_BOX(vbox), accountsbutton, FALSE, FALSE, 2);
		gtk_box_pack_start(GTK_BOX(vbox), contactsbutton, FALSE, FALSE, 2);
		gtk_box_pack_start(GTK_BOX(vbox), prefsbutton, FALSE, FALSE, 2);
		gtk_box_pack_start(GTK_BOX(vbox), awaybutton, FALSE, FALSE, 2);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
		
		gtk_container_add (GTK_CONTAINER(window), vbox);
		g_signal_connect(okbutton, "clicked", G_CALLBACK(ok_callback), NULL);
		g_signal_connect(cancelbutton, "clicked", G_CALLBACK(cancel_callback), NULL);
		g_signal_connect(window, "destroy", G_CALLBACK(destroy_callback), NULL);
		
		gtk_widget_show(vbox);
		gtk_widget_show(hbox);
		gtk_widget_show(accountsbutton);
		gtk_widget_show(contactsbutton);
		gtk_widget_show(prefsbutton);
		gtk_widget_show(awaybutton);
		gtk_widget_show(okbutton);
		gtk_widget_show(cancelbutton);
		gtk_widget_show(label);

		gtk_widget_show(window);
	}
}
