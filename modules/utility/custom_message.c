/*
 * Ayttm - Custom Message plugin, read a custom message from a file
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
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef __MINGW32__
#define __IN_PLUGIN__
#endif
#include "plugin_api.h"
#include "prefs.h"
#include "platform_defs.h"
#include "globals.h"
#include "service.h"

/*******************************************************************************
 *                             Begin Module Code
 ******************************************************************************/
/*  Module defines */
#define plugin_info custmsg_LTX_plugin_info
#define plugin_init custmsg_LTX_plugin_init
#define plugin_finish custmsg_LTX_plugin_finish
#define module_version custmsg_LTX_module_version


/* Function Prototypes */
/* ebmCallbackData is a parent struct, the child of which will be an ebmContactData */
static int plugin_init();
static int plugin_finish();
static int eb_custom_msg_timeout_callback(void * data);

static int ref_count=0;

/*  Module Exports */
PLUGIN_INFO plugin_info = {
	PLUGIN_UTILITY, 
	"Custom Message", 
	"Pick a custom away message from a file", 
	"$Revision: 1.2 $",
	"$Date: 2008/08/11 04:50:46 $",
	&ref_count,
	plugin_init,
	plugin_finish,
	NULL
};
/* End Module Exports */

unsigned int module_version() {return CORE_VERSION;}

static char custom_away_msg[MAX_PREF_LEN];
static int enable_plugin=0;
static int custom_msg_tag=0;

static int plugin_init()
{
	input_list * il = calloc(1, sizeof(input_list));

	plugin_info.prefs = il;
	il->widget.entry.value = custom_away_msg;
	il->name = "custom_away_msg";
	il->label = _("Custom Away _File:");
	il->type = EB_INPUT_ENTRY;

	il->next = calloc(1, sizeof(input_list));
	il = il->next;
	il->widget.checkbox.value = &enable_plugin;
	il->name = "enable_plugin";
	il->label= _("_Automatically update status:");
	il->type = EB_INPUT_CHECKBOX;

	custom_msg_tag = eb_timeout_add(5 * 1000, 
			(void *) eb_custom_msg_timeout_callback, 0);

	return 0;
}

static int plugin_finish()
{
	if(custom_msg_tag) {
		eb_timeout_remove(custom_msg_tag);
		custom_msg_tag=0;
	}
	while(plugin_info.prefs) {
		input_list *il = plugin_info.prefs->next;
		free(plugin_info.prefs);
		plugin_info.prefs = il;
	}

	return 0;
}

/*******************************************************************************
 *                             End Module Code
 ******************************************************************************/

static void set_away(char *a_message)
{
	LList *list;

	for (list = accounts; list; list = list->next)
	{
		eb_local_account *ela = list->data;
		/* Only change state for those accounts which are connected */
		if(ela->connected)
			eb_services[ela->service_id].sc->set_away(ela, a_message, 0);
	}
}

static int eb_custom_msg_timeout_callback(void *data)
{
	static long file_time;
	struct stat s;
	int fd, n, i;
	char buf[1024];

   if(!enable_plugin)
      return 1;

	if( !custom_away_msg[0] )
		return 1;

	stat(custom_away_msg, &s);

	if(file_time >= s.st_mtime)
		return 1;

	file_time = s.st_mtime;

	fd = open(custom_away_msg, O_RDONLY);
	n=read(fd, buf, sizeof(buf)-1);
	buf[n]=0;
	for(i=n-1; buf[i]=='\n'; i--)
		buf[i]=0;

	close(fd);

	set_away(buf);

	return 1;
}


