/*
 * EveryBuddy 
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

unsigned int module_version() {return CORE_VERSION;}

#include "intl.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

#ifdef __MINGW32__
#define __IN_PLUGIN__
#endif
#include "plugin_api.h"
#include "prefs.h"
#include "platform_defs.h"

#ifndef NAME_MAX
#define NAME_MAX 512
#endif


static char notes_dir[NAME_MAX];
#ifdef __MINGW32__
static char notes_editor[MAX_PREF_LEN]="notepad";
#else
static char notes_editor[MAX_PREF_LEN]="xedit";
#endif

/*******************************************************************************
 *                             Begin Module Code
 ******************************************************************************/
/*  Module defines */
#define plugin_info notes_LTX_plugin_info
#define plugin_init notes_LTX_plugin_init
#define plugin_finish notes_LTX_plugin_finish
#define module_version notes_LTX_module_version


/* Function Prototypes */
/* ebmCallbackData is a parent struct, the child of which will be an ebmContactData */
static void notes_feature(ebmCallbackData *data);
static int plugin_init();
static int plugin_finish();

static int ref_count=0;

/*  Module Exports */
PLUGIN_INFO plugin_info = {
	PLUGIN_UTILITY, 
	"Keep notes on contacts", 
	"Keep notes about your contacts and buddies", 
	"$Revision: 1.5 $",
	"$Date: 2003/04/29 08:32:02 $",
	&ref_count,
	plugin_init,
	plugin_finish,
	NULL
};
/* End Module Exports */

static void *notes_tag1=NULL;
static void *notes_tag2=NULL;

static int plugin_init()
{
	input_list * il = calloc(1, sizeof(input_list));
	int result=0;

	eb_debug(DBG_MOD, "notes init\n");
	notes_tag1=eb_add_menu_item("Notes", EB_CHAT_WINDOW_MENU, notes_feature, ebmCONTACTDATA, NULL);
	if(!notes_tag1) {
		eb_debug(DBG_MOD, "Error!  Unable to add Notes menu to chat window menu\n");
		return(-1);
	}
	notes_tag2=eb_add_menu_item("Notes", EB_CONTACT_MENU, notes_feature, ebmCONTACTDATA, NULL);
	if(!notes_tag2) {
		result=eb_remove_menu_item(EB_CHAT_WINDOW_MENU, notes_tag1);
		eb_debug(DBG_MOD, "Error!  Unable to add Notes menu to contact menu\n");
		return(-1);
	}
	snprintf(notes_dir, NAME_MAX, "%s/notes", eb_config_dir());
	mkdir(notes_dir, 0700);
	eb_debug(DBG_MOD, "Notes Dir: %s\n", notes_dir);
	plugin_info.prefs = il;
	il->widget.entry.value = notes_editor;
	il->widget.entry.name = "notes_editor";
	il->widget.entry.label = _("Notes Editor");
	il->type = EB_INPUT_ENTRY;
	return(0);
}

static int plugin_finish()
{
	int result=0;
	
	while(plugin_info.prefs) {
		input_list *il = plugin_info.prefs->next;
		free(plugin_info.prefs);
		plugin_info.prefs = il;
	}

	result=eb_remove_menu_item(EB_CHAT_WINDOW_MENU, notes_tag1);
	if(result) {
		eb_debug(DBG_MOD, "Unable to remove Notes menu item from chat window menu!\n");
		return(-1);
	}
	result=eb_remove_menu_item(EB_CONTACT_MENU, notes_tag2);
	if(result) {
		eb_debug(DBG_MOD, "Unable to remove Notes menu item from chat window menu!\n");
		return(-1);
	}
	return(0);
}

/*******************************************************************************
 *                             End Module Code
 ******************************************************************************/

/* We're really going to get back an ebmContactData object */
static void notes_feature(ebmCallbackData *data)
{
	ebmContactData *ecd=NULL;
	char buff[256];
	char cmd_buff[1024];
#ifndef __MINGW32__
	pid_t pid;
#endif

	eb_debug(DBG_MOD, ">\n");
	if(IS_ebmContactData(data))
		ecd=(ebmContactData *)data;
	else {	/* This should never happen, unless something is horribly wrong */
		eb_debug(DBG_MOD, "*** Warning *** Unexpected ebmCallbackData type returned!\n");
		return;
	}
	eb_debug(DBG_MOD, "contact: %s remote_account: %s\n", ecd->contact, ecd->remote_account);
	snprintf(buff, 255, "Notes on %s", ecd->contact);
	snprintf(cmd_buff, 1023, "%s/%s", notes_dir, ecd->contact);

#ifndef __MINGW32__
	pid = fork();
	if (pid == 0) {
		char *args[3];
		int e;

		args[0] = strdup(plugin_info.prefs->widget.entry.value);
		args[1] = strdup(cmd_buff);
		args[2] = NULL;
		e = execvp(args[0], args);
		free(args[0]);
		free(args[1]);
		_exit(0);
	}
#endif
	eb_debug(DBG_MOD, "<\n");
}
