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

/*
 * icq.c
 * ICQ implementation
 */

unsigned int module_version()
{
	return CORE_VERSION;
}

#ifdef __MINGW32__
#define __IN_PLUGIN__
#endif
#include "intl.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#ifdef __MINGW32__
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include "message_parse.h"
#include "status.h"
#include "icq_mod.h"
#include "libicq/tcp.h"
#include "info_window.h"
#include "src/util.h"
#include "value_pair.h"
#include "dialog.h"
#include "globals.h"
#include "plugin_api.h"
#include "smileys.h"

#if defined( _WIN32 ) && !defined(__MINGW32__)
#include "../libicq/libicq.h"
#else
#include "libicq/libicq.h"
#include "libicq/send.h"
#endif

#include "pixmaps/icq_online.xpm"
#include "pixmaps/icq_away.xpm"

#define DBG_ICQ do_icq_debug

/*******************************************************************************
 *                             Begin Module Code
 ******************************************************************************/
/*  Module defines */
#ifndef USE_POSIX_DLOPEN
#define plugin_info icq_LTX_plugin_info
#define SERVICE_INFO icq_LTX_SERVICE_INFO
#define plugin_init icq_LTX_plugin_init
#define plugin_finish icq_LTX_plugin_finish
#define module_version icq_LTX_module_version
#endif

/* Function Prototypes */
static int plugin_init();
static int plugin_finish();

static int ref_count = 0;
static char icq_server[MAX_PREF_LEN] = "icq.mirabilis.com";
static char icq_port[MAX_PREF_LEN] = "4000";
static int do_icq_debug = 0;

/*  Module Exports */
PLUGIN_INFO plugin_info = {
	PLUGIN_SERVICE,
	"ICQ",
	"Provides support for the ICQ protocol",
	"$Revision: 1.13 $",
	"$Date: 2009/09/17 12:04:58 $",
	&ref_count,
	plugin_init,
	plugin_finish
};
struct service SERVICE_INFO = { "ICQ", -1, TRUE, TRUE, FALSE, TRUE, NULL };

/* End Module Exports */

static char *eb_icq_get_color(void)
{
	static char color[] = "#008800";
	return color;
}

static int plugin_init()
{
	input_list *il = g_new0(input_list, 1);
	eb_debug(DBG_MOD, "ICQ\n");
	ref_count = 0;
	plugin_info.prefs = il;
	il->widget.entry.value = icq_server;
	il->name = "icq_server";
	il->label = _("Server:");
	il->type = EB_INPUT_ENTRY;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.entry.value = icq_port;
	il->name = "icq_port";
	il->label = _("Port:");
	il->type = EB_INPUT_ENTRY;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.checkbox.value = &do_icq_debug;
	il->name = "do_icq_debug";
	il->label = _("Enable debugging");
	il->type = EB_INPUT_CHECKBOX;
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

/*this is an evil hack to deal with the single user nature of libICQ*/
static eb_local_account *icq_user_account = NULL;
static char *ICQ_STATUS_STRINGS[] =
	{ "", "Away", "N/A", "Occ", "DND", "Offline", "Invis", "Chat" };

static LList *icq_buddies = NULL;

/*	There are no prefs for ICQ at the moment.
static input_list * icq_prefs = NULL;
*/

/*
 * Now here is just some stuff to deal with ICQ group chat
 */

typedef struct _icq_buff {
	unsigned long uin;
	char str[2048];
	int len;
	char r;
	char g;
	char b;
	char style;
	char active;
} icq_buff;

typedef struct _icq_info_data {
	USER_EXT_INFO *ext_info;
	USER_INFO_PTR user_info;
	char *away;
} icq_info_data;

static LList *icq_chat_messages = NULL;

static icq_buff *find_icq_buff(unsigned long uin)
{
	LList *node;
	for (node = icq_chat_messages; node; node = node->next) {
		if (((icq_buff *)(node->data))->uin == uin) {
			return node->data;
		}
	}

	return NULL;
}

/*this is to make things look pretty*/

static char *icq_to_html(char *r, char *g, char *b, char *style, char *input,
	int len)
{
	GString *string = g_string_sized_new(2048);
	int i;
	int font_stack = 0;
	char *result;
	assert(len >= 0);

	if (*style & 0x1) {
		g_string_append(string, "<B>");
	}
	if (*style & 0x2) {
		g_string_append(string, "<I>");
	}
	if (*style & 0x4) {
		g_string_append(string, "<U>");
	}
	g_string_append(string, "<font color=\"#");
	{
		char buff[3];
		g_snprintf(buff, 3, "%02x", *r);
		g_string_append(string, buff);
		g_snprintf(buff, 3, "%02x", *g);
		g_string_append(string, buff);
		g_snprintf(buff, 3, "%02x", *b);
		g_string_append(string, buff);
	}
	g_string_append(string, "\">");
	font_stack++;

	for (i = 0; i < len; i++) {
		if (input[i] == 0x03 || input[i] == 0x04 || input[i] == '\n') {
			continue;
		}
		if (input[i] == 0x11) {
			*style = input[++i];
			i += 3;
			if (*style & 0x1) {
				g_string_append(string, "<B>");
			}
			if (*style & 0x2) {
				g_string_append(string, "<I>");
			}
			if (*style & 0x4) {
				g_string_append(string, "<U>");
			}
		}
		if (input[i] == 0x00) {
			char buff[3];
			g_string_append(string, "<font color=\"#");
			g_snprintf(buff, 3, "%02x", input[++i]);
			*r = input[i];
			g_string_append(string, buff);
			g_snprintf(buff, 3, "%02x", input[++i]);
			*g = input[i];
			g_string_append(string, buff);
			g_snprintf(buff, 3, "%02x", input[++i]);
			*b = input[i];
			g_string_append(string, buff);
			g_string_append(string, "\">");
			i++;
			font_stack++;
			continue;
		}
		if (input[i] == 0x10) {
			short len = input[i] + (((short)input[i + 1]) << 8);
			i += 2 + len + 2;
			continue;
		}
		if (input[i] == 0x11 || input[i] == 0x12) {
			i += 4;
			continue;
		}
		g_string_append_c(string, input[i]);

	}
	for (i = 0; i < font_stack; i++) {
		g_string_append(string, "</font>");
	}

	result = string->str;
	g_string_free(string, FALSE);
	eb_debug(DBG_ICQ, "%s\n", result);
	return result;
}

/*
 *  this is to help deypher the statuses that you send and receive
 */
enum {
	ICQ_ONLINE,
	ICQ_AWAY,
	ICQ_NA,
	ICQ_OCCUPIED,
	ICQ_DND,
	ICQ_OFFLINE,
	ICQ_INVISIBLE,
	ICQ_FREE_CHAT
};
struct icq_account_data {
	int status;
};

typedef struct icq_local_account_data {
	char password[1024];
	int timeout_id;
	int select_id;
} icq_local_account;

static int connection = STATUS_OFFLINE;

static CONTACT_PTR getContact(glong uin)
{
	int i;
	for (i = 0; i < Num_Contacts; i++) {
		if (Contacts[i].uin == uin)
			return &Contacts[i];
	}
	return 0;
}

static int icq_get_current_state(eb_local_account *account);
static void icq_logout(eb_local_account *account);
static void icq_login(eb_local_account *account);
static eb_chat_room *icq_make_chat_room(char *name, eb_local_account *account,
	int is_public);
static void icq_info_update(eb_account *sender);
static void icq_info_data_cleanup(info_window *iw);
static void icq_get_info(eb_local_account *account_from,
	eb_account *account_to);

static void authorize_callback(gpointer data, int response)
{
	long uin = (long)data;

	if (response) {
		Send_AuthMessage(uin);
	}
}

/* the callbacks that get used by libicq  and get 
   triggered on incomming events*/

static void EventLogin(void *ignore)
{
	eb_debug(DBG_ICQ, "EventLogin\n");
	set_status = STATUS_ONLINE;
	connection = STATUS_ONLINE;

	icq_user_account->connected = 1;
	if (icq_user_account->status_menu)
		eb_set_active_menu_status(icq_user_account->status_menu,
			ICQ_ONLINE);

	ref_count++;
}

#define SRV_FORCE_DISCONNECT 0x0028
#define SRV_MULTI_PACKET   0x0212
#define SRV_BAD_PASS    0x6400
#define SRV_GO_AWAY        0x00F0
#define SRV_WHAT_THE_HELL 0x0064

static void EventDisconnect(void *data)
{
	eb_debug(DBG_ICQ, "EventDisconnect\n");
	icq_logout(icq_user_account);
	connection = STATUS_OFFLINE;
	set_status = STATUS_OFFLINE;

	icq_user_account->connected = 0;
	if (icq_user_account->status_menu)
		eb_set_active_menu_status(icq_user_account->status_menu,
			ICQ_OFFLINE);

	ref_count--;
}

static void EventChatDisconnect(void *data)
{
	unsigned long uin = (unsigned long)data;
	icq_buff *ib = find_icq_buff(uin);
	eb_account *ea;
	eb_chat_room *ecr = find_chat_room_by_id("ICQ");
	char buffer[20];

	if (ib) {
		icq_chat_messages = l_list_remove(icq_chat_messages, ib);
	}

	ea = find_account_by_handle(buffer, SERVICE_INFO.protocol_id);
	g_snprintf(buffer, 20, "%ld", uin);

	if (ecr) {
		eb_chat_room_buddy_leave(ecr, buffer);
	}
}

static void EventChatConnect(void *data)
{
	unsigned long uin = (unsigned long)data;
	icq_buff *ib = g_new0(icq_buff, 1);
	char buffer[20];
	eb_account *ea;
	eb_chat_room *ecr = find_chat_room_by_id("ICQ");

	if (!ecr) {
		fprintf(stderr, "Chat room not found!!!\n");
		return;
	}

	g_snprintf(buffer, 20, "%ld", uin);
	ea = find_account_by_handle(buffer, SERVICE_INFO.protocol_id);

	eb_debug(DBG_ICQ, "EventChatConnect\n");
	eb_debug(DBG_ICQ, "EventChatConnect %ld\n", uin);

	ib->uin = uin;

	icq_chat_messages = l_list_append(icq_chat_messages, ib);
	eb_debug(DBG_ICQ, "EventChatConnected done\n");

	if (ea) {
		eb_chat_room_buddy_arrive(ecr, ea->account_contact->nick,
			ea->handle);
	} else {
		eb_chat_room_buddy_arrive(ecr, buffer, buffer);
	}

}

static void EventChatRead(void *data)
{
	CHAT_DATA_PTR cd = data;
	eb_chat_room *ecr = find_chat_room_by_id("ICQ");
	char buff[20];

	icq_buff *ib = find_icq_buff(cd->uin);
	eb_account *ea;

	if (!isgraph(cd->c)) {
		fprintf(stderr, "Reading Value 0x%02x\n", cd->c);
	} else {
		fprintf(stderr, "Reading Value '%c'\n", cd->c);
	}

	if (!ecr) {
		fprintf(stderr, "ICQ: EventChatRead chat room not found!\n");
		return;
	}

	if (!ib) {
		fprintf(stderr, "ICQ: EventChatRead chat buffer not found!\n");
		return;
	}

	g_snprintf(buff, 20, "%d", cd->uin);
	ea = find_account_by_handle(buff, SERVICE_INFO.protocol_id);

	if (cd->c == '\r' && (ib->active ||
			(ib->len > 1 && isprint(ib->str[ib->len - 1])))) {
		char *message;

		if (ib->active || ib->len < 47) {
 ICQ_MESSAGE_OK:
			message =
				icq_to_html(&(ib->r), &(ib->g), &(ib->b),
				&(ib->style), ib->str, ib->len);

			ib->active = 1;
		} else {
			/*
			 * Occasionaly libicq messes up and we get the information from
			 * the first packet the client sends after the chat is initiated
			 * if that does end up being the case, we may as well parse
			 * some information out of it while we are filtering it out
			 * this is a hack, and as a result I have added some checks
			 * to make sure we don't overstep our buffer, and if we do
			 * assume that it is not have that beginning packet, and processes
			 * it like any other chat message
			 *
			 * -Torrey
			 */

			short name_len =
				ib->str[8] + (((short)ib->str[9]) << 8);
			short font_len;
			short adjust_factor = 0;
			short adjust_factor2 = 0;
			int i;

			if (10 + name_len + 35 > ib->len) {
				eb_debug(DBG_ICQ,
					"Message length test 1 faild, skipping\n");
				goto ICQ_MESSAGE_OK;
			}

			eb_debug(DBG_ICQ, "Checkpoints %d %d\n",
				ib->str[10 + name_len + 24],
				ib->str[10 + name_len + 26]);

			for (i = 24; i <= 32; i++) {
				if (ib->str[10 + name_len + i] == 4)
					break;
			}
			adjust_factor = i - 24;

			font_len = ib->str[10 + name_len + 35 + adjust_factor] +
				(((short)ib->str[10 + name_len + 36 +
						adjust_factor]) << 8);

			if (47 + name_len + font_len > ib->len) {
				eb_debug(DBG_ICQ,
					"Message length test 2 faild, skipping\n");
				goto ICQ_MESSAGE_OK;
			}

			/*
			   ib->r = ib->str[10+name_len+adjust_factor];
			   ib->g = ib->str[11+name_len+adjust_factor];
			   ib->b = ib->str[12+name_len+adjust_factor];
			 */

			ib->active = 1;

			for (adjust_factor2 = 0;
				!ib->str[10 + name_len + 36 + font_len + 3 +
					adjust_factor + adjust_factor2]
				&& 10 + name_len + 36 + font_len + 3 +
				adjust_factor + adjust_factor2 < ib->len;
				adjust_factor2++) ;

			message =
				icq_to_html(&(ib->r), &(ib->g), &(ib->b),
				&(ib->style),
				ib->str + 10 + name_len + 36 + font_len + 3 +
				adjust_factor + adjust_factor2,
				ib->len - 10 - name_len - 36 - font_len - 3 -
				adjust_factor - adjust_factor2);
		}

		if (!ea) {
			eb_chat_room_show_message(ecr, buff, message);
		} else {
			eb_chat_room_show_message(ecr,
				ea->account_contact->nick, message);
		}

		ib->len = 0;
		g_free(message);
	} else if (cd->c == '\b' && (ib->active || ib->len > 47)) {
		if (ib->len > 0) {
			ib->len--;
		}
	} else {
		ib->str[ib->len++] = cd->c;
	}
	eb_debug(DBG_ICQ, "EventChatRead end\n");

}

static void EventMessage(void *data)
{
	char buff[255];
	char message[1024];
	eb_account *sender = NULL;
	CLIENT_MESSAGE_PTR c_messg;

	c_messg = (CLIENT_MESSAGE_PTR) data;

	sprintf(buff, "%d", c_messg->uin);
	sender = find_account_by_handle(buff, SERVICE_INFO.protocol_id);

	if (!sender) {
		eb_account *ea = g_new0(eb_account, 1);
		struct icq_account_data *iad =
			g_new0(struct icq_account_data, 1);
		strcpy(ea->handle, buff);
		ea->service_id = SERVICE_INFO.protocol_id;
		iad->status = STATUS_OFFLINE;
		ea->protocol_account_data = iad;

		if (!iGetLocalPref("do_ignore_unknown")) {
			add_unknown(ea);
			//switch these two lines to make it not automatically display on unknow user
			ICQ_Get_Info(c_messg->uin);
		} else {
			return;
		}
		//        icq_get_info(find_suitable_local_account(NULL,SERVICE_INFO.protocol_id),ea); 
		sender = ea;

		g_warning("Unknown user %s", buff);
	}
	if ((c_messg->type & 0x00FF) == URL_MESS) {
		g_snprintf(message, 1024, "<a href=\"%s\">%s</a><BR>%s",
			c_messg->url, c_messg->url, c_messg->msg);
	} else if (c_messg->type == AWAY_MESS) {
		if (sender->infowindow != NULL) {
			if (sender->infowindow->info_data == NULL)
				sender->infowindow->info_data =
					malloc(sizeof(icq_info_data));
			if (((icq_info_data *)sender->infowindow->info_data)->
				away != NULL)
				free(((icq_info_data *)sender->infowindow->
						info_data)->away);
			((icq_info_data *)sender->infowindow->info_data)->away =
				malloc(strlen(c_messg->msg) + 1);
			strcpy(((icq_info_data *)sender->infowindow->
					info_data)->away, c_messg->msg);
			icq_info_update(sender);
		}
		if (sender->account_contact->chatwindow != NULL) {
			g_snprintf(message, 1024, _("User is away: %s"),
				c_messg->msg);
		} else
			return;
	} else if (c_messg->type == USER_ADDED_MESS) {
		g_snprintf(message, 1024,
			_("I have just added you to my contact list."));
	} else if (c_messg->type == MSG_MESS || c_messg->type == MSG_MULTI_MESS) {
		g_snprintf(message, 1024, "%s", c_messg->msg);
	} else if (c_messg->type == CHAT_MESS) {
		eb_debug(DBG_ICQ, "accepting chat request\n");
		invite_dialog(icq_user_account, sender->account_contact->nick,
			"ICQ", (gpointer) c_messg->uin);
		return;
	} else if (c_messg->type == FILE_MESS) {
		char message[1024];

		g_snprintf(message, 1024,
			_
			("ICQ user %d would like to\nsend you the file\n%s\ndo you want to accept?"),
			c_messg->uin, c_messg->filename);

		//do_dialog(message, _("Incomming ICQ File Request"), NULL, NULL);
		return;
	} else if (c_messg->type == AUTH_REQ_MESS) {
		char *c = strchr(c_messg->msg, '\xFE');
		char dialog_message[1024];

		*c = '\0';

		c = strchr(c + 1, '\xFE');
		c = strchr(c + 1, '\xFE');
		c = strchr(c + 1, '\xFE');
		c = strchr(c + 1, '\xFE');

		g_snprintf(dialog_message, 1024,
			_
			("ICQ user %s would like to add you to their contact list.\nReason: %s\nWould you like to authorize them?"),
			c_messg->msg, c + 1);

		eb_do_dialog(dialog_message, _("Authorize ICQ User"),
			authorize_callback, (gpointer) c_messg->uin);

		return;
	} else {
		g_snprintf(message, 1024,
			_("Unknown packet type %x contents %s"), c_messg->type,
			c_messg->msg);
	}

	eb_parse_incoming_message(icq_user_account, sender, message);
	eb_debug(DBG_ICQ, "EventMessage\n");

}

static void EventChangeStatus(void *data)
{
	USER_UPDATE_PTR user_update;
	eb_account *ea;
	char buff[255];
	user_update = (USER_UPDATE_PTR) data;

	if (!user_update) {
		return;
	}
	if ((int)data == SRV_GO_AWAY || (int)data == SRV_FORCE_DISCONNECT
		|| (int)data == SRV_MULTI_PACKET) {
		icq_logout(icq_user_account);
		fprintf(stderr, "ICQ has terminated this connection.  "
			"To prevent a spin of death, Ayttm\n"
			"will NOT automatically reconnect you.\n");
		/* icq_login(icq_user_account); */
		return;
	}
	if ((int)data == SRV_BAD_PASS || (int)data == SRV_WHAT_THE_HELL) {
		icq_logout(icq_user_account);
		return;
	}
	sprintf(buff, "%d", user_update->uin);
	ea = find_account_by_handle(buff, SERVICE_INFO.protocol_id);

	if (ea) {
		int newstat;
		struct icq_account_data *iad = ea->protocol_account_data;
		newstat = (user_update->status + 1) % 65536 - 1;
		if (iad->status == STATUS_OFFLINE && newstat != STATUS_OFFLINE)
			buddy_login(ea);
		else if (iad->status != STATUS_OFFLINE
			&& newstat == STATUS_OFFLINE)
			buddy_logoff(ea);
		iad->status = newstat;
		buddy_update_status(ea);
	}

	eb_debug(DBG_ICQ, "EventChangeStatus %d %d\n", user_update->uin,
		user_update->status);
}

static void EventSearchResults(void *ignore)
{
	eb_debug(DBG_ICQ, "EventSearchResults\n");
}

static void EventInfo(void *data)
{
	char buff[255];
	USER_INFO_PTR ui = data;
	eb_local_account *ela;
	eb_account *ea;
	g_snprintf(buff, 255, "%d", ui->uin);

	/*Now this is what I call an ugly solution!! */
	ICQ_Get_Away_Message(ui->uin);

	ela = find_local_account_by_handle(buff, SERVICE_INFO.protocol_id);
	if (ela && strlen(ui->nick) > 0) {
		strcpy(ela->alias, ui->nick);
	} else {
		ea = find_account_by_handle(buff, SERVICE_INFO.protocol_id);
		if (ea && strlen(ui->nick) > 0) {
			if (!strcmp(ea->handle, ea->account_contact->nick)) {
				strcpy(ea->account_contact->nick, ui->nick);
				contact_update_status(ea->account_contact);
				update_contact_list();
			}
			/* Since this lookup was just to get the unknown person's
			 * screen name, we just stop here
			 */

			return;
		}
		if (ea) {
			ela = find_suitable_local_account(NULL,
				SERVICE_INFO.protocol_id);

			if (ea->infowindow != NULL) {
				if (ea->infowindow->info_type !=
					SERVICE_INFO.protocol_id) {
					/*hmm, I wonder what should really be done here */
					return;
				}

				if (((icq_info_data *)ea->infowindow->
						info_data)->user_info != NULL)
					g_free(((icq_info_data *)ea->
							infowindow->info_data)->
						user_info);

				((icq_info_data *)ea->infowindow->info_data)->
					user_info = malloc(sizeof(USER_INFO));
				memcpy(((icq_info_data *)ea->infowindow->
						info_data)->user_info, ui,
					sizeof(USER_INFO));
				icq_info_update(ea);
			}
		}
	}
	eb_debug(DBG_ICQ, "EventInfo\n");
	eb_debug(DBG_ICQ, "%s\n", ui->nick);

}

static void EventExtInfo(void *data)
{

	USER_EXT_INFO_PTR ui = data;
	char buff[255];
	eb_local_account *ela;
	eb_account *ea;
	g_snprintf(buff, 255, "%d", ui->uin);
	ela = find_local_account_by_handle(buff, SERVICE_INFO.protocol_id);
	if (ela) {
		// This is me, I don't think I need to do anything.
	} else {
		ea = find_account_by_handle(buff, SERVICE_INFO.protocol_id);
		if (ea) {
			ela = find_suitable_local_account(NULL,
				SERVICE_INFO.protocol_id);

			if (ea->infowindow != NULL) {
				if (ea->infowindow->info_type !=
					SERVICE_INFO.protocol_id) {
					/*hmm, I wonder what should really be done here */
					return;
				}

				if (((icq_info_data *)ea->infowindow->
						info_data)->ext_info != NULL)
					g_free(((icq_info_data *)ea->
							infowindow->info_data)->
						ext_info);

				((icq_info_data *)ea->infowindow->info_data)->
					ext_info =
					malloc(sizeof(USER_EXT_INFO));
				memcpy(((icq_info_data *)ea->infowindow->
						info_data)->ext_info, ui,
					sizeof(USER_EXT_INFO));
				icq_info_update(ea);
			}
		}
	}

	eb_debug(DBG_ICQ, "EventExtInfo\n");
}

static void RegisterCallbacks()
{
	ICQ_Register_Callback(EVENT_LOGIN, EventLogin);
	ICQ_Register_Callback(EVENT_DISCONNECT, EventDisconnect);
	ICQ_Register_Callback(EVENT_MESSAGE, EventMessage);
	ICQ_Register_Callback(EVENT_ONLINE, EventChangeStatus);
	ICQ_Register_Callback(EVENT_OFFLINE, EventChangeStatus);
	ICQ_Register_Callback(EVENT_STATUS_UPDATE, EventChangeStatus);
	ICQ_Register_Callback(EVENT_SEARCH_RESULTS, EventSearchResults);
	ICQ_Register_Callback(EVENT_INFO, EventInfo);
	ICQ_Register_Callback(EVENT_EXT_INFO, EventExtInfo);
	ICQ_Register_Callback(EVENT_CHAT_READ, EventChatRead);
	ICQ_Register_Callback(EVENT_CHAT_CONNECT, EventChatConnect);
	ICQ_Register_Callback(EVENT_CHAT_DISCONNECT, EventChatDisconnect);
}

/* here are the timers that will maintain the ICQ connection and poll for events*/

static int SelectServer(gpointer data)
{
	ICQ_Check_Response(10000);
	return TRUE;
}

static int KeepAlive(gpointer data)
{
	if (connection == STATUS_ONLINE)
		ICQ_Keep_Alive();
	return TRUE;
}

static void RemoveTimers(icq_local_account *ila)
{
	eb_timeout_remove(ila->select_id);
	eb_timeout_remove(ila->timeout_id);
}

static void AddTimers(icq_local_account *ila)
{
	ila->select_id = eb_timeout_add(100, SelectServer, NULL);
	ila->timeout_id = eb_timeout_add((guint32) 60000, KeepAlive, NULL);
}

/* the callbacks that get used by EveryBuddy */

static void icq_send_chat_room_message(eb_chat_room *room, char *message)
{
	LList *node;
	icq_buff *ib;
	char *buffer = g_new0(char, strlen(message) + 3);

	strcpy(buffer, message);
	strcat(buffer, "\r\n");

	for (node = icq_chat_messages; node; node = node->next) {
		ib = node->data;
		eb_debug(DBG_ICQ, "sending \"%s\" to %ld\n", message, ib->uin);
		ICQ_Send_Chat(ib->uin, buffer);
	}
	eb_chat_room_show_message(room, room->local_user->alias, message);
	g_free(buffer);
}

static int icq_query_connected(eb_account *account)
{
	CONTACT_PTR contact = getContact(atol(account->handle));
	struct icq_account_data *iad = account->protocol_account_data;
	assert(eb_services[account->service_id].protocol_id ==
		SERVICE_INFO.protocol_id);

	/*if the user is not in the local contact, add it */
	if (!contact && icq_user_account && icq_user_account->connected) {
		ICQ_Add_User(atol(account->handle), account->handle);
		contact = getContact(atol(account->handle));
	}
#if 0
	if (connection == STATUS_OFFLINE)
		g_warning("connection == STATUS_OFFLINE\n");
	if (contact->status == STATUS_OFFLINE)
		g_warning("%s status == offline\n", contact->nick);
#endif
	return iad->status != STATUS_OFFLINE && connection != STATUS_OFFLINE;

}

static void icq_add_user(eb_account *account)
{
	assert(eb_services[account->service_id].protocol_id ==
		SERVICE_INFO.protocol_id);
	icq_buddies = l_list_append(icq_buddies, account->handle);
	if (icq_user_account && icq_user_account->connected) {
		ICQ_Add_User(atol(account->handle), account->handle);
	}
}

static eb_account *icq_new_account(const char *account)
{
	eb_account *a = g_new0(eb_account, 1);
	struct icq_account_data *iad = g_new(struct icq_account_data, 1);

	a->protocol_account_data = iad;
	a->service_id = SERVICE_INFO.protocol_id;
	strcpy(a->handle, account);
	iad->status = STATUS_OFFLINE;

	return a;
}

static void icq_del_user(eb_account *account)
{
	assert(eb_services[account->service_id].protocol_id ==
		SERVICE_INFO.protocol_id);
	ICQ_Delete_User(atol(account->handle));
}

static void icq_login(eb_local_account *account)
{
	LList *node;
	icq_local_account *ila;
	int count = 0;
//      ICQ_SetDebug(ICQ_VERB_ERR|ICQ_VERB_WARN);
	RegisterCallbacks();
	ila = (icq_local_account *)account->protocol_local_account_data;
	assert(eb_services[account->service_id].protocol_id ==
		SERVICE_INFO.protocol_id);
	UIN = atol(account->handle);
	strcpy(passwd, ila->password);
	set_status = STATUS_OFFLINE;
	connection = STATUS_ONLINE;
	remote_port = atoi(icq_port);
	strcpy(server, icq_server);

	AddTimers(ila);
	eb_debug(DBG_ICQ, "n/n %d /n/n\n", UIN /*, passwd */ );
	ICQ_Change_Status(STATUS_ONLINE);
	//account->connected=1;

	for (node = icq_buddies, count = 0; node && count < 100;
		node = node->next, count++) {
		char *handle = node->data;
		ICQ_Add_User(atol(handle), handle);
	}
}

static eb_account *icq_read_config(eb_account *ea, LList *config)
{
	struct icq_account_data *iad = g_new0(struct icq_account_data, 1);

	iad->status = STATUS_OFFLINE;
	ea->protocol_account_data = iad;
	icq_add_user(ea);

	return ea;
}

static eb_local_account *icq_read_local_config(LList *pairs)
{

	eb_local_account *ela = g_new0(eb_local_account, 1);
	icq_local_account *ila = g_new0(icq_local_account, 1);
	char *str = NULL;

	/*you know, eventually error handling should be put in here */
	ela->handle = value_pair_get_value(pairs, "SCREEN_NAME");
	strncpy(ela->alias, ela->handle, 255);
	str = value_pair_get_value(pairs, "PASSWORD");
	strncpy(ila->password, str, 255);
	free(str);

	str = value_pair_get_value(pairs, "CONNECT");
	ela->connect_at_startup = (str && !strcmp(str, "1"));
	free(str);

	set_status = STATUS_OFFLINE;

	ela->service_id = SERVICE_INFO.protocol_id;
	ela->protocol_local_account_data = ila;

	icq_user_account = ela;
	return ela;
}

static LList *icq_write_local_config(eb_local_account *account)
{
	icq_local_account *ila = account->protocol_local_account_data;
	LList *list = NULL;
	value_pair *vp;

	vp = g_new0(value_pair, 1);

	strcpy(vp->key, "SCREEN_NAME");
	strcpy(vp->value, account->handle);

	list = l_list_append(list, vp);

	vp = g_new0(value_pair, 1);

	strcpy(vp->key, "PASSWORD");
	strcpy(vp->value, ila->password);

	list = l_list_append(list, vp);

	vp = g_new0(value_pair, 1);
	strcpy(vp->key, "CONNECT");
	if (account->connect_at_startup)
		strcpy(vp->value, "1");
	else
		strcpy(vp->value, "0");

	list = l_list_append(list, vp);

	return list;
}

static void icq_logout(eb_local_account *account)
{
	LList *l;
	icq_local_account *ila;
	ila = (icq_local_account *)account->protocol_local_account_data;
	assert(eb_services[account->service_id].protocol_id ==
		SERVICE_INFO.protocol_id);
	ICQ_Change_Status(STATUS_OFFLINE);
	RemoveTimers(ila);
	account->connected = 0;

	for (l = icq_buddies; l; l = l->next) {
		eb_account *ea =
			find_account_by_handle(l->data,
			SERVICE_INFO.protocol_id);
		if (ea) {	/* find_account_by_handle can return NULL --eib */
			struct icq_account_data *iad =
				ea->protocol_account_data;
			buddy_logoff(ea);
			iad->status = STATUS_OFFLINE;
		}
	}
}

static void icq_send_im(eb_local_account *account_from,
	eb_account *account_to, char *message)
{
	CONTACT_PTR contact = getContact(atol(account_to->handle));
	struct icq_account_data *iad = account_to->protocol_account_data;
	assert(eb_services[account_from->service_id].protocol_id ==
		SERVICE_INFO.protocol_id);
	assert(eb_services[account_to->service_id].protocol_id ==
		SERVICE_INFO.protocol_id);

	/*if the user is not in the local contact, add it */
	if (!contact) {
		ICQ_Add_User(atol(account_to->handle), account_to->handle);
		contact = getContact(atol(account_to->handle));
	}

	/* if the person is away get the away message */

	if (iad->status != ICQ_ONLINE && iad->status != ICQ_OFFLINE
		&& iad->status != ICQ_INVISIBLE) {
		ICQ_Get_Away_Message(contact->uin);
	}

	ICQ_Send_Message(contact->uin, message);
}

static LList *icq_get_states()
{
	LList *states = NULL;
	states = l_list_append(states, "Online");
	states = l_list_append(states, "Away");
	states = l_list_append(states, "Not Available");
	states = l_list_append(states, "Occupied");
	states = l_list_append(states, "Do Not Disturb");
	states = l_list_append(states, "Offline");
	states = l_list_append(states, "Free for Chat");
	return states;
}

static int icq_get_current_state(eb_local_account *account)
{
	assert(eb_services[account->service_id].protocol_id ==
		SERVICE_INFO.protocol_id);
	switch (set_status) {
	case STATUS_OFFLINE:
		return ICQ_OFFLINE;
	case STATUS_NA:
		return ICQ_NA;
	case STATUS_ONLINE:
		return ICQ_ONLINE;
	case STATUS_OCCUPIED:
		return ICQ_OCCUPIED;
	case STATUS_AWAY:
		return ICQ_AWAY;
	case STATUS_DND:
		return ICQ_DND;
	case STATUS_INVISIBLE:
		return ICQ_INVISIBLE;
	case STATUS_FREE_CHAT:
		return ICQ_FREE_CHAT;
	default:
		printf("error unknown state %d\n", set_status);
		/* exit(0); */
		return 0;
	}
}

static void icq_set_current_state(eb_local_account *account, int state)
{
	assert(eb_services[account->service_id].protocol_id ==
		SERVICE_INFO.protocol_id);
	switch (state) {
	case ICQ_OFFLINE:
		ICQ_Change_Status(STATUS_OFFLINE);
		set_status = STATUS_OFFLINE;
		if (connection != STATUS_OFFLINE) {
			icq_logout(account);
		}
		connection = STATUS_OFFLINE;
		account->connected = 0;
		break;
	case ICQ_NA:
		ICQ_Change_Status(STATUS_NA);
		set_status = STATUS_NA;
		connection = STATUS_ONLINE;
		account->connected = 1;
		break;
	case ICQ_ONLINE:
		if (connection == STATUS_OFFLINE) {
			icq_login(account);
		}
		ICQ_Change_Status(STATUS_ONLINE);
		set_status = STATUS_ONLINE;
		connection = STATUS_ONLINE;
		account->connected = 1;
		break;
	case ICQ_OCCUPIED:
		ICQ_Change_Status(STATUS_OCCUPIED);
		connection = STATUS_ONLINE;
		account->connected = 1;
		break;
	case ICQ_AWAY:
		ICQ_Change_Status(STATUS_AWAY);
		set_status = STATUS_AWAY;
		connection = STATUS_ONLINE;
		account->connected = 1;
		break;
	case ICQ_DND:
		ICQ_Change_Status(STATUS_DND);
		set_status = STATUS_DND;
		connection = STATUS_ONLINE;
		account->connected = 1;
		break;
	case ICQ_FREE_CHAT:
		ICQ_Change_Status(STATUS_FREE_CHAT);
		set_status = STATUS_FREE_CHAT;
		connection = STATUS_ONLINE;
		account->connected = 1;
		break;
	default:
		ICQ_Change_Status(STATUS_OFFLINE);
		set_status = STATUS_OFFLINE;
		connection = STATUS_OFFLINE;
		account->connected = 0;
		break;

	}
}

static char *icq_check_login(char *user, char *pass)
{
	return NULL;
}

static char **icq_get_status_pixmap(eb_account *account)
{
	struct icq_account_data *iad;
	iad = account->protocol_account_data;

	switch (iad->status) {
	case STATUS_OFFLINE:
	case STATUS_NA:
	case STATUS_OCCUPIED:
	case STATUS_AWAY:
	case STATUS_DND:
	case STATUS_INVISIBLE:
		return icq_away_xpm;
	default:
		/* online, freechat */
		return icq_online_xpm;
	}
}

static char *icq_get_status_string(eb_account *account)
{

	struct icq_account_data *iad = account->protocol_account_data;
//      iad->status = iad->status% 65536;
	switch (iad->status) {
		/*status offline */
	case STATUS_OFFLINE:
		//case -1:
		return ICQ_STATUS_STRINGS[ICQ_OFFLINE];
		/*status NA */
	case STATUS_NA:
		//case 65541:
		return ICQ_STATUS_STRINGS[ICQ_NA];
		/*status Online */
	case STATUS_ONLINE:
		//case 65536:
		return ICQ_STATUS_STRINGS[ICQ_ONLINE];
		/*status Occupied */
	case STATUS_OCCUPIED:
		//case 65553:
		return ICQ_STATUS_STRINGS[ICQ_OCCUPIED];
		/*status Away */
	case STATUS_AWAY:
		//case 65537:
		return ICQ_STATUS_STRINGS[ICQ_AWAY];
		/*status DND */
	case STATUS_DND:
		//case 65555:
		return ICQ_STATUS_STRINGS[ICQ_DND];
	case STATUS_FREE_CHAT:
		return ICQ_STATUS_STRINGS[ICQ_FREE_CHAT];
	case STATUS_INVISIBLE:
		return ICQ_STATUS_STRINGS[ICQ_INVISIBLE];
	default:
		return ICQ_STATUS_STRINGS[ICQ_OFFLINE];
	}
}

static void icq_set_idle(eb_local_account *account, int idle)
{
	if ((idle == 0) && icq_get_current_state(account) == ICQ_AWAY) {
		if (account->status_menu)
			eb_set_active_menu_status(account->status_menu,
				ICQ_ONLINE);
	} else if ((idle >= 600)
		&& (icq_get_current_state(account) == ICQ_ONLINE)) {
		if (account->status_menu)
			eb_set_active_menu_status(account->status_menu,
				ICQ_AWAY);

	}
}

static void icq_send_file(eb_local_account *from, eb_account *to, char *file)
{
	int i;
	long remote_uin = atol(to->handle);
	for (i = 0; i < ICQ_MAX_CONTACTS; i++) {
		if (Contacts[i].uin == remote_uin) {
			struct in_addr ip;
			char port[8];
			ip.s_addr = htonl(Contacts[i].current_ip);
			g_snprintf(port, 8, "%d", Contacts[i].tcp_port);
			ICQSendFile(inet_ntoa(ip),
				port, from->handle, file, "Ayttm file x-fer");
		}

	}
}

static void icq_send_invite(eb_local_account *account, eb_chat_room *room,
	char *user, char *message)
{
	long uin = atol(user);
	ICQ_Request_Chat(uin, message);
}

static eb_chat_room *icq_make_chat_room(char *name, eb_local_account *account,
	int is_public)
{
	eb_chat_room *ecr = find_chat_room_by_id("ICQ");

	if (ecr) {
		return NULL;
	}

	ecr = g_new0(eb_chat_room, 1);

	strcpy(ecr->room_name, "ICQ");
	strcpy(ecr->id, "ICQ");
	ecr->connected = 0;
	ecr->local_user = account;

	eb_join_chat_room(ecr, TRUE);
	eb_chat_room_buddy_arrive(ecr, account->alias, account->handle);

	return ecr;
}

static void icq_join_chat_room(eb_chat_room *room)
{
	/* nothing to see here... */
	return;
}

static void icq_leave_chat_room(eb_chat_room *room)
{
	LList *node;

	for (node = icq_chat_messages; node; node = node->next) {
		icq_buff *ib = node->data;
		TCP_TerminateChat(ib->uin);
		g_free(ib);
	}
	l_list_free(icq_chat_messages);
	icq_chat_messages = NULL;
}

static void icq_accept_invite(eb_local_account *account, void *invitation)
{
	long uin = (long)invitation;
	eb_chat_room *ecr = icq_make_chat_room("", icq_user_account);
	if (!ecr) {
		ecr = find_chat_room_by_id("ICQ");
	}
	eb_join_chat_room(ecr, TRUE);
	ICQ_Accept_Chat(uin);
}

static void icq_decline_invite(eb_local_account *account, void *invitation)
{
	long uin = (long)invitation;
	ICQ_Refuse_Chat(uin);
}

static void icq_set_away(eb_local_account *account, char *message)
{
	if (message) {
		if (account->status_menu)
			eb_set_active_menu_status(account->status_menu,
				ICQ_AWAY);
	} else {
		if (account->status_menu)
			eb_set_active_menu_status(account->status_menu,
				ICQ_ONLINE);
	}
}

static void icq_get_info(eb_local_account *account_from, eb_account *account_to)
{
	long remote_uin = atol(account_to->handle);
	if (!account_to->infowindow) {
		account_to->infowindow =
			eb_info_window_new(account_from, account_to);
		account_to->infowindow->cleanup = icq_info_data_cleanup;
	}
	if (account_to->infowindow->info_type == -1
		|| account_to->infowindow->info_data == NULL) {
		if (account_to->infowindow->info_data == NULL) {
			account_to->infowindow->info_data =
				g_new0(icq_info_data, 1);
			account_to->infowindow->cleanup = icq_info_data_cleanup;
		}
		account_to->infowindow->info_type = SERVICE_INFO.protocol_id;
	}

	if (account_to->infowindow->info_type != SERVICE_INFO.protocol_id) {
		/*hmm, I wonder what should really be done here */
		return;
	}
	Send_InfoRequest(remote_uin);
	Send_ExtInfoRequest(remote_uin);

}

static void icq_info_update(eb_account *sender)
{
	char buff[255];
	info_window *iw = sender->infowindow;

	icq_info_data *iid = (icq_info_data *)iw->info_data;
	eb_info_window_clear(iw);

	eb_info_window_add_info(sender, _("ICQ Info:<BR>"), 0, 0, 0);
	if (iid->away != NULL) {
		eb_info_window_add_info(sender, iid->away, 0, 0, 0);
		eb_info_window_add_info(sender, "<BR><HR>", 0, 0, 0);
	}
	if (iid->user_info != NULL) {
		USER_INFO_PTR ui = iid->user_info;
		g_snprintf(buff, 255, _("UIN: %d<br>"), ui->uin);
		eb_info_window_add_info(sender, buff, 0, 0, 0);
		g_snprintf(buff, 255, _("Nickname: %s<br>"), ui->nick);
		eb_info_window_add_info(sender, buff, 0, 0, 0);
		g_snprintf(buff, 255, _("First Name: %s<br>"), ui->first);
		eb_info_window_add_info(sender, buff, 0, 0, 0);
		g_snprintf(buff, 255, _("Last Name: %s<br>"), ui->last);
		eb_info_window_add_info(sender, buff, 0, 0, 0);
		g_snprintf(buff, 255, _("Email: %s<br>"), ui->email);
		eb_info_window_add_info(sender, buff, 0, 0, 0);
		if (ui->auth_required)
			eb_info_window_add_info(sender,
				_("Authorization Required<BR>"), 0, 0, 0);

	}
	if (iid->ext_info != NULL) {
		USER_EXT_INFO_PTR ui = iid->ext_info;
		if (iid->user_info == NULL) {
			g_snprintf(buff, 255, _("UIN: %d<br>"), ui->uin);
			eb_info_window_add_info(sender, buff, 0, 0, 0);
		}
		g_snprintf(buff, 255, _("City: %s<br>"), ui->city);
		eb_info_window_add_info(sender, buff, 0, 0, 0);
		g_snprintf(buff, 255, _("State: %s<br>"), ui->state);
		eb_info_window_add_info(sender, buff, 0, 0, 0);
		g_snprintf(buff, 255, _("Age: %s<br>"), ui->age);
		eb_info_window_add_info(sender, buff, 0, 0, 0);
		g_snprintf(buff, 255, _("Sex: %s<br>"), ui->sex);
		eb_info_window_add_info(sender, buff, 0, 0, 0);
		g_snprintf(buff, 255, _("Phone: %s<br>"), ui->phone);
		eb_info_window_add_info(sender, buff, 0, 0, 0);
		g_snprintf(buff, 255, _("Url: %s<br>"), ui->url);
		eb_info_window_add_info(sender, buff, 0, 0, 0);
		g_snprintf(buff, 255, _("About: %s<br>"), ui->about);
		eb_info_window_add_info(sender, buff, 0, 0, 0);
	}
}

static void icq_info_data_cleanup(info_window *iw)
{
	icq_info_data *iid = (icq_info_data *)iw->info_data;
	if (iid->ext_info != NULL)
		free(iid->ext_info);
	if (iid->user_info != NULL)
		free(iid->user_info);
	if (iid->away != NULL)
		free(iid->away);
}

/*	There are no prefs for ICQ at the moment.

static input_list * eb_icq_get_prefs()
{
	return icq_prefs;
}
*/

static void eb_icq_read_prefs_config(LList *values)
{
	char *c;
	c = value_pair_get_value(values, "server");
	if (c) {
		strcpy(icq_server, c);
		free(c);
	}
	c = value_pair_get_value(values, "port");
	if (c) {
		strcpy(icq_port, c);
		free(c);
	}
	c = value_pair_get_value(values, "do_icq_debug");
	if (c) {
		do_icq_debug = atoi(c);
		free(c);
	}
}

static LList *eb_icq_write_prefs_config()
{
	LList *config = NULL;
	char buffer[5];

	config = value_pair_add(config, "server", icq_server);
	config = value_pair_add(config, "port", icq_port);
	sprintf(buffer, "%d", do_icq_debug);
	config = value_pair_add(config, "do_icq_debug", buffer);

	return config;
}

struct service_callbacks *query_callbacks()
{
	struct service_callbacks *sc;

	sc = g_new0(struct service_callbacks, 1);
	sc->query_connected = icq_query_connected;
	sc->login = icq_login;
	sc->logout = icq_logout;
	sc->send_im = icq_send_im;
	sc->read_local_account_config = icq_read_local_config;
	sc->write_local_config = icq_write_local_config;
	sc->read_account_config = icq_read_config;
	sc->get_states = icq_get_states;
	sc->get_current_state = icq_get_current_state;
	sc->set_current_state = icq_set_current_state;
	sc->check_login = icq_check_login;
	sc->add_user = icq_add_user;
	sc->del_user = icq_del_user;
	sc->new_account = icq_new_account;
	sc->get_status_string = icq_get_status_string;
	sc->get_status_pixmap = icq_get_status_pixmap;
	sc->set_idle = icq_set_idle;
	sc->set_away = icq_set_away;
	sc->send_file = icq_send_file;
	sc->send_invite = icq_send_invite;
	sc->make_chat_room = icq_make_chat_room;
	sc->send_chat_room_message = icq_send_chat_room_message;
	sc->leave_chat_room = icq_leave_chat_room;
	sc->join_chat_room = icq_join_chat_room;
	sc->decline_invite = icq_decline_invite;
	sc->accept_invite = icq_accept_invite;
	sc->get_info = icq_get_info;

	sc->get_prefs = NULL;
	sc->read_prefs_config = eb_icq_read_prefs_config;
	sc->write_prefs_config = eb_icq_write_prefs_config;

	sc->get_color = eb_icq_get_color;
	sc->get_smileys = eb_default_smileys;

	return sc;
}
