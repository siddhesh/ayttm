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

#ifdef __MINGW32__
#define __IN_PLUGIN__
#endif

#include "intl.h"
#include <stdlib.h>
#include <string.h>

#include "externs.h"
#include "plugin_api.h"


/*******************************************************************************
 *                             Begin Module Code
 ******************************************************************************/
/*  Module defines */
#define plugin_info middle_LTX_plugin_info
#define plugin_init middle_LTX_plugin_init
#define plugin_finish middle_LTX_plugin_finish

/* Function Prototypes */
static char *plstripHTML(const eb_local_account * local, const eb_account * remote,
			 const struct contact *contact, const char *s);
static int middle_init();
static int middle_finish();

static int doLeet = 0;

static int ref_count = 0;

/*  Module Exports */
PLUGIN_INFO plugin_info = {
	PLUGIN_UTILITY,
	"L33t-o-matic",
	"Turns all incoming and outgoing messages into l33t-speak",
	"$Revision: 1.1.1.1 $",
	"$Date: 2003/04/01 07:24:46 $",
	&ref_count,
	middle_init,
	middle_finish,
	NULL
};
/* End Module Exports */

static int middle_init()
{
	input_list *il = calloc(1, sizeof(input_list));
	plugin_info.prefs = il;

	il->widget.checkbox.value = &doLeet;
	il->widget.checkbox.name = "doLeet";
	il->widget.entry.label = strdup(_("Enable L33t-speak conversion"));
	il->type = EB_INPUT_CHECKBOX;

	eb_debug(DBG_MOD, "L33tSp33k initialised\n");

	outgoing_message_filters =
	    l_list_append(outgoing_message_filters, &plstripHTML);
	incoming_message_filters =
	    l_list_append(incoming_message_filters, &plstripHTML);

	return 0;
}

static int middle_finish()
{
	eb_debug(DBG_MOD, "L33tSp33k shutting down\n");
	outgoing_message_filters =
	    l_list_remove(outgoing_message_filters, &plstripHTML);
	incoming_message_filters =
	    l_list_remove(incoming_message_filters, &plstripHTML);

	return 0;
}

/*******************************************************************************
 *                             End Module Code
 ******************************************************************************/
static char *plstripHTML(const eb_local_account * local, const eb_account * remote,
			 const struct contact *contact, const char *in)
{
	int pos = 0;
	char * s = strdup(in);

	if (!doLeet) {
		return s;
	}

	while (s[pos] != '\0') {
		if (s[pos] == 'e' || s[pos] == 'E') {
			s[pos] = '3';
		}
		if (s[pos] == 'i' || s[pos] == 'I') {
			s[pos] = '1';
		}
		if (s[pos] == 'o' || s[pos] == 'O') {
			s[pos] = '0';
		}
		if (s[pos] == 'a' || s[pos] == 'A') {
			s[pos] = '4';
		}
		if (s[pos] == 's' || s[pos] == 'S') {
			s[pos] = '5';
		}

		pos++;
	}

	return s;
}
