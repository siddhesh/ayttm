/*
 * XMPP Extension for ayttm 
 *
 * Copyright (C) 2009, Ayttm Team
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
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "service.h"
#include "plugin_api.h"
#include "activity_bar.h"

#include "status.h"

#include "input_list.h"
#include "value_pair.h"
#include "util.h"

#include "message_parse.h"
#include "messages.h"

#include "xmpp_httplib.h"

/*******************************************************************************
 *                             Begin Module Code
 ******************************************************************************/
/*  Module defines */
#ifndef USE_POSIX_DLOPEN
#define plugin_info xmpp_LTX_plugin_info
#define SERVICE_INFO xmpp_LTX_SERVICE_INFO
#define module_version xmpp_LTX_module_version
#endif

/* Function Prototypes */
static int plugin_init();
static int plugin_finish();
struct service_callbacks *query_callbacks();

static int is_setting_state = 0;

static int ref_count = 0;

static int do_xmpp_debug = 0;

typedef {
	struct service *service;
	XmppExtensionCallbacks callbacks;
} XmppService;

static LList *xmpp_services = NULL;

/*  Module Exports */
PLUGIN_INFO plugin_info = {
	PLUGIN_SERVICE,
	"XMPP",
	"Ayttm client for XMPP",
	"$Revision: 1.6 $",
	"$Date: 2009/09/17 12:04:59 $",
	&ref_count,
	plugin_init,
	plugin_finish,
	NULL
};

static const int XMPP_DEFAULT_CAPABILITIES = SERVICE_CAN_OFFLINEMSG |
						SERVICE_CAN_FILETRANSFER

struct service SERVICE_INFO = {
	"XMPP",
	-1,
	XMPP_DEFAULT_CAPABILITIES,
	NULL
};

/* End Module Exports */

unsigned int module_version()
{
	return CORE_VERSION;
}

static void init_xmpp_services(LList *services)
{
	LList *l = services;

	while (l) {
		struct service *s = l->data;
		char *val = NULL;
		LList *prefs = NULL;
		XmppService *newservice = g_new0(XmppService, 1);

		s->capabilities = XMPP_DEFAULT_CAPABILITIES;
		is->sc = calloc(1, sizeof(struct service_callbacks));
		memcpy(s->sc, SERVICE_INFO->sc, sizeof(struct service_callbacks));

		newservice->service = s;

		LList *prefs = GetPref (s->name);

		char *val = value_pair_get_value(prefs, "extension");

		if (val) {
			void *extension = ayttm_dlopen(val);
			void (*extension_init)(XmppService) = ayttm_dlsym(extension,
						 			  "extension_init");

			extension_init(newservice);
		}

		xmpp_services = l_list_append(xmpp_services, newservice);
	}
}

static int plugin_init()
{
	LList *xmpp_services = NULL;
	input_list *il = calloc(1, sizeof(input_list));
	ref_count = 0;

	plugin_info.prefs = il;
	il->widget.checkbox.value = &do_xmpp_debug;
	il->name = "do_xmpp_debug";
	il->label = _("Enable debugging");
	il->type = EB_INPUT_CHECKBOX;

	ayttm_prefs_read_file(XMPP_PREFS);

	xmpp_services = get_services_for_provider(SERVICE_INFO.name);
	init_xmpp_services(xmpp_services);

	l_list_free(xmpp_services);

	return (0);
}

static int plugin_finish()
{
	eb_debug(DBG_MOD, "Returning the ref_count: %i\n", ref_count);
	return (ref_count);
}

/*******************************************************************************
 *                             End Module Code
 ******************************************************************************/

#define XMPP_MSG_COLOUR	"#88aa00"
static char *ay_xmpp_get_color(void)
{
	return XMPP_MSG_COLOUR;
}

enum xmpp_state_code { XMPP_STATE_ONLINE, XMPP_STATE_OFFLINE };

typedef struct {
	int state;
	char *status;
} xmpp_account_data;

typedef struct {
	char password[MAX_PREF_LEN];
	int state;
	char *status;
	int connect_progress_tag;
	LList *buddies;
	XmppImplementation *impl;
} xmpp_local_account;

static void xmpp_account_prefs_init(eb_local_account *ela)
{
	xmpp_local_account *lla = ela->protocol_local_account_data;

	input_list *il = calloc(1, sizeof(input_list));

	ela->prefs = il;
	il->widget.entry.value = ela->handle;
	il->name = "SCREEN_NAME";
	il->label = _("_Username:");
	il->type = EB_INPUT_ENTRY;

	il->next = calloc(1, sizeof(input_list));
	il = il->next;
	il->widget.entry.value = lla->password;
	il->name = "PASSWORD";
	il->label = _("_Password:");
	il->type = EB_INPUT_PASSWORD;
}

static eb_local_account *ay_xmpp_read_local_account_config(LList *pairs)
{
	eb_local_account *ela;
	xmpp_local_account *lla;

	if (!pairs) {
		WARNING(("ay_xmpp_read_local_account_config: pairs == NULL"));
		return NULL;
	}

	ela = calloc(1, sizeof(eb_local_account));
	lla = calloc(1, sizeof(xmpp_local_account));

	lla->state = XMPP_STATE_OFFLINE;
	strcpy(lla->last_update, "0");
	lla->poll_interval = 1800;

	ela->service_id = SERVICE_INFO.protocol_id;
	ela->protocol_local_account_data = lla;

	xmpp_account_prefs_init(ela);
	eb_update_from_value_pair(ela->prefs, pairs);

	return ela;
}

static LList *ay_xmpp_write_local_config(eb_local_account *account)
{
	return eb_input_to_value_pair(account->prefs);
}

static LList *ay_xmpp_get_states()
{
}

static int ay_xmpp_get_current_state(eb_local_account *account)
{
	xmpp_local_account *lla = account->protocol_local_account_data;

	return lla->state;
}

static void ay_xmpp_set_idle(eb_local_account *account, int idle)
{
}

static void ay_xmpp_set_away(eb_local_account *account, char *message, int away)
{
}

static eb_account *ay_xmpp_new_account(eb_local_account *ela, const char *handle)
{
	eb_account *ea = calloc(1, sizeof(eb_account));
	xmpp_account_data *lad = calloc(1, sizeof(xmpp_account_data));

	LOG(("ay_xmpp_new_account"));

	ea->protocol_account_data = lad;
	strncpy(ea->handle, handle, sizeof(ea->handle));
	ea->service_id = SERVICE_INFO.protocol_id;
	lad->state = XMPP_STATE_OFFLINE;
	ea->ela = ela;
	return ea;
}

static void ay_xmpp_add_user(eb_account *ea)
{
}

static void ay_xmpp_del_user(eb_account *ea)
{
}

static eb_account *ay_xmpp_read_account_config(eb_account *ea, LList *config)
{
	xmpp_account_data *lad = calloc(1, sizeof(xmpp_account_data));
	lad->state = XMPP_STATE_OFFLINE;
	ea->protocol_account_data = lad;

	return ea;
}

static int ay_xmpp_query_connected(eb_account *account)
{
	xmpp_account_data *lad = account->protocol_account_data;

	return (lad->state == XMPP_STATE_ONLINE);
}

static char *state_strings[] = {
	"",
	"Offline"
};

static const char *ay_xmpp_get_status_string(eb_account *account)
{
	xmpp_account_data *lad = account->protocol_account_data;

	return lad->status;
}

static const char *ay_xmpp_get_state_string(eb_account *account)
{
	xmpp_account_data *lad = account->protocol_account_data;

	return state_strings[lad->state];
}

static const char **ay_xmpp_get_state_pixmap(eb_account *account)
{
	xmpp_account_data *lad;

	lad = account->protocol_account_data;

	if (lad->state == XMPP_STATE_ONLINE)
		return NULL;
	else
		return NULL;
}

static void ay_xmpp_send_file(eb_local_account *from, eb_account *to, char *file)
{
}

static void ay_xmpp_send_im(eb_local_account *account_from,
	eb_account *account_to, char *message)
{
}

static void ay_xmpp_login(eb_local_account *ela)
{
	ref_count++;
}

static void ay_xmpp_logout(eb_local_account *ela)
{
	ref_count--;
}

static void ay_xmpp_set_current_state(eb_local_account *account, int state)
{
	xmpp_local_account *lla = account->protocol_local_account_data;

	if (is_setting_state)
		return;

	if (lla->state == XMPP_STATE_OFFLINE && state == XMPP_STATE_ONLINE)
		ay_xmpp_login(account);
	else if (lla->state == XMPP_STATE_ONLINE && state == XMPP_STATE_OFFLINE)
		ay_xmpp_logout(account);

	lla->state = state;
}

static char *ay_xmpp_check_login(const char *user, const char *pass)
{
	return NULL;
}

struct service_callbacks *query_callbacks()
{
	struct service_callbacks *sc;

	sc = calloc(1, sizeof(struct service_callbacks));

	sc->query_connected = ay_xmpp_query_connected;
	sc->login = ay_xmpp_login;
	sc->logout = ay_xmpp_logout;
	sc->check_login = ay_xmpp_check_login;

	sc->send_im = ay_xmpp_send_im;

	sc->read_local_account_config = ay_xmpp_read_local_account_config;
	sc->write_local_config = ay_xmpp_write_local_config;
	sc->read_account_config = ay_xmpp_read_account_config;

	sc->get_states = ay_xmpp_get_states;
	sc->get_current_state = ay_xmpp_get_current_state;
	sc->set_current_state = ay_xmpp_set_current_state;

	sc->new_account = ay_xmpp_new_account;
	sc->add_user = ay_xmpp_add_user;
	sc->del_user = ay_xmpp_del_user;

	sc->get_state_string = ay_xmpp_get_state_string;
	sc->get_status_string = ay_xmpp_get_status_string;
	sc->get_status_pixmap = ay_xmpp_get_state_pixmap;

	sc->set_idle = ay_xmpp_set_idle;
	sc->set_away = ay_xmpp_set_away;

	sc->send_file = ay_xmpp_send_file;

	sc->get_color = ay_xmpp_get_color;

	return sc;
}
