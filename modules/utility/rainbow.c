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

#include "intl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __MINGW32__
#define __IN_PLUGIN__
#endif
#include "externs.h"
#include "plugin_api.h"
#include "prefs.h"
#include "platform_defs.h"


/*******************************************************************************
 *                             Begin Module Code
 ******************************************************************************/
/*  Module defines */
#define plugin_info rainbow_LTX_plugin_info
#define plugin_init rainbow_LTX_plugin_init
#define plugin_finish rainbow_LTX_plugin_finish
#define module_version rainbow_LTX_module_version


/* Function Prototypes */
/* ebmCallbackData is a parent struct, the child of which will be an ebmContactData */
static char *dorainbow(const eb_local_account * local,
		       const eb_account * remote,
		       const struct contact *contact, const char *s);
static int rainbow_init();
static int rainbow_finish();

static int doRainbow = 0;

static char sstart_r[MAX_PREF_LEN] = "255";
static char sstart_g[MAX_PREF_LEN] = "0";
static char sstart_b[MAX_PREF_LEN] = "0";

static char send_r[MAX_PREF_LEN] = "0";
static char send_g[MAX_PREF_LEN] = "0";
static char send_b[MAX_PREF_LEN] = "255";

static char *html_tags[] =
    { "html", "body", "font", "p", "i", "b", "u", "img", "a", "br", "hr",
"head" };
static int num_tags = 12;

static int ref_count = 0;

/*  Module Exports */
PLUGIN_INFO plugin_info = {
	PLUGIN_FILTER,
	"Rainbow",
	"Turns all outgoing messages into rainbow colours using HTML",
	"$Revision: 1.7 $",
	"$Date: 2003/05/06 17:04:50 $",
	&ref_count,
	rainbow_init,
	rainbow_finish,
	NULL
};
/* End Module Exports */

unsigned int module_version() {return CORE_VERSION;}

static int rainbow_init()
{
	input_list *il = calloc(1, sizeof(input_list));
	plugin_info.prefs = il;

	il->widget.checkbox.value = &doRainbow;
	il->widget.checkbox.name = "doRainbow";
	il->widget.checkbox.label = strdup(_("Enable rainbow conversion"));
	il->type = EB_INPUT_CHECKBOX;

	il->next = calloc(1, sizeof(input_list));
	il = il->next;
	il->widget.entry.value = sstart_r;
	il->widget.entry.name = "sstart_r";
	il->widget.entry.label = strdup(_("Starting R value:"));
	il->type = EB_INPUT_ENTRY;
	il->next = calloc(1, sizeof(input_list));
	il = il->next;
	il->widget.entry.value = sstart_g;
	il->widget.entry.name = "sstart_g";
	il->widget.entry.label = strdup(_("Starting G value:"));
	il->type = EB_INPUT_ENTRY;
	il->next = calloc(1, sizeof(input_list));
	il = il->next;
	il->widget.entry.value = sstart_b;
	il->widget.entry.name = "sstart_b";
	il->widget.entry.label = strdup(_("Starting B value:"));
	il->type = EB_INPUT_ENTRY;

	il->next = calloc(1, sizeof(input_list));
	il = il->next;
	il->widget.entry.value = send_r;
	il->widget.entry.name = "send_r";
	il->widget.entry.label = strdup(_("Ending R value:"));
	il->type = EB_INPUT_ENTRY;
	il->next = calloc(1, sizeof(input_list));
	il = il->next;
	il->widget.entry.value = send_g;
	il->widget.entry.name = "send_g";
	il->widget.entry.label = strdup(_("Ending G value:"));
	il->type = EB_INPUT_ENTRY;
	il->next = calloc(1, sizeof(input_list));
	il = il->next;
	il->widget.entry.value = send_b;
	il->widget.entry.name = "send_b";
	il->widget.entry.label = strdup(_("Ending B value:"));
	il->type = EB_INPUT_ENTRY;

	eb_debug(DBG_MOD, "Rainbow initialised\n");

	outgoing_message_filters =
	    l_list_append(outgoing_message_filters, &dorainbow);

	return 0;
}

static int rainbow_finish()
{
	eb_debug(DBG_MOD, "Rainbow shutting down\n");
	outgoing_message_filters =
	    l_list_remove(outgoing_message_filters, &dorainbow);
	
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
static char *dorainbow(const eb_local_account * local,
		       const eb_account * remote,
		       const struct contact *contact, const char *s)
{
	char *retval;
	char *wptr;
	int pos = 0;
	int start_r = atoi(sstart_r);
	int start_g = atoi(sstart_g);
	int start_b = atoi(sstart_b);
	int end_r = atoi(send_r);
	int end_g = atoi(send_g);
	int end_b = atoi(send_b);
	int len = strlen(s);

	if (!doRainbow) {
		return strdup(s);
	}

	if (start_r > 255 || start_r < 0) {
		start_r = 0;
	}
	if (start_g > 255 || start_g < 0) {
		start_g = 0;
	}
	if (start_b > 255 || start_b < 0) {
		start_b = 0;
	}
	if (end_r > 255 || end_r < 0) {
		end_r = 0;
	}
	if (end_g > 255 || end_g < 0) {
		end_g = 0;
	}
	if (end_b > 255 || end_b < 0) {
		end_b = 0;
	}

	wptr = retval = malloc(23 * len * sizeof(char));

	while (s[pos] != '\0') {
		if (s[pos] == '<') {
			int p2 = pos + 1;
			int a, found = 0;

			while (s[p2] == ' ' || s[p2] == '/') {
				p2++;
			}	// strip space before HTML tag
			for (a = 0; a < num_tags; a++) {
				if (!strncasecmp
				    (s + p2, html_tags[a],
				     strlen(html_tags[a]))) {
					found = 1;
					break;
				}
			}

			if (found) {
				while (1) {
					*(wptr++) = s[pos++];
					if (s[pos - 1] == '\0'
					    || s[pos - 1] == '>') {
						break;
					}
				}
				*wptr = '\0';	// in case this is the last char

				continue;
			}
		}

		wptr +=
		    snprintf(wptr, 22 * (len - pos),
			       "<font color=#%02x%02x%02x>%c",
			       (pos * end_r + (len - pos) * start_r) / len,
			       (pos * end_g + (len - pos) * start_g) / len,
			       (pos * end_b + (len - pos) * start_b) / len,
			       s[pos]);
		pos++;
	}


	return retval;
}
