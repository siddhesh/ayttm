/*
 * Ayttm
 *
 * Copyright (C) 2009, the Ayttm team
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

#include "msn-connection.h"
#include "msn-account.h"
#include "msn-sb.h"
#include "llist.h"
#include "msn-ext.h"

#include <stdio.h>
#include <string.h>

#define MSN_SB "SB"

void msn_sb_got_join(MsnConnection *mc)
{
	SBPayload *payload = mc->sbpayload;

	payload->num_members++;

	if(payload->callback)
		payload->callback(mc, 0, payload->data);

	ext_buddy_joined_chat(mc, mc->current_message->argv[1], msn_urldecode(mc->current_message->argv[2]));
}

static void msn_sb_got_usr_response(MsnConnection *mc, void *data)
{
	SBPayload *payload = mc->sbpayload;
	MsnAccount *ma = mc->account;
	int error = 0;

	if(!strcmp(mc->current_message->argv[2], "OK"))
		error = 0;
	else {
		LList *l;

		error = 1;

		for(l = mc->account->connections; l; l = l_list_next(l)) {
			if(mc == l->data) {
				mc->account->connections = l_list_remove(mc->account->connections, l);
				break;
			}
		}

		free(mc->sbpayload);
		mc->account = NULL;
	}

	if(error)
		payload->callback(ma->ns_connection, error, payload->data);
	else
		msn_message_send(mc, NULL, MSN_COMMAND_CAL, payload->room);
}


static void msn_sb_got_ans_response(MsnConnection *mc, void *data)
{
	SBPayload *payload = mc->sbpayload;

	if(mc->current_message->command == MSN_COMMAND_IRO) {
		payload->num_members = atoi(mc->current_message->argv[3]);

		ext_buddy_joined_chat(mc, mc->current_message->argv[4], msn_urldecode(mc->current_message->argv[5]));

		msn_connection_push_callback(mc, msn_sb_got_ans_response, NULL);
	}
	else if (mc->current_message->command == MSN_COMMAND_ANS) {
		ext_got_ans(mc);
	}
	else {
		printf("failure in response\n");
	}
}


static void msn_sb_connected(MsnConnection *mc)
{
	SBPayload *payload = mc->sbpayload;

	if(payload->incoming) {
		msn_message_send(mc, NULL, MSN_COMMAND_ANS, mc->account->passport, payload->challenge, payload->session_id);
		msn_connection_push_callback(mc, msn_sb_got_ans_response, NULL);
		ext_new_sb(mc);
	}
	else {
		msn_message_send(mc, NULL, MSN_COMMAND_USR, 2, mc->account->passport, payload->challenge);
		msn_connection_push_callback(mc, msn_sb_got_usr_response, NULL);
	}

	free(payload->challenge);
}


void msn_connect_sb(MsnAccount *ma, const char *host, int port)
{
	MsnConnection *sbmc = msn_connection_new();

	sbmc->host = strdup(host);
	
	sbmc->port = port;
	sbmc->type = MSN_CONNECTION_SB;
	sbmc->account = ma;

	ma->connections = l_list_append(ma->connections, sbmc);

	sbmc->sbpayload = ma->ns_connection->sbpayload;
	ma->ns_connection->sbpayload = NULL;

	msn_connection_connect(sbmc, msn_sb_connected);
}


void msn_connect_sb_with_info(MsnConnection *mc, char *room_name, void *data)
{
	char *offset = NULL;
	SBPayload *payload = m_new0(SBPayload, 1);

	payload->data = data;
	payload->callback = NULL;
	payload->room = strdup(room_name);
	payload->incoming = 1;

	mc->sbpayload = payload;

	int port = 0;

	char *host = mc->current_message->argv[2];
	
	offset = strchr(host, ':');
	*offset = '\0';
	port = atoi(++offset);

	payload->challenge = strdup(mc->current_message->argv[4]);
	payload->session_id = strdup(mc->current_message->argv[1]);

	msn_connect_sb(mc->account, host, port);
}


static void msn_got_sb_info(MsnConnection *mc, void *data)
{
	char *offset = NULL;
	SBPayload *payload = mc->sbpayload;
	int port = 0;

	char *host = mc->current_message->argv[3];
	
	offset = strchr(host, ':');
	*offset = '\0';
	port = atoi(++offset);

	payload->challenge = strdup(mc->current_message->argv[5]);

	msn_connect_sb(mc->account, host, port);
}


void msn_get_sb(MsnAccount *ma, char *room_name, void *data, SBCallback callback)
{
	SBPayload *payload = m_new0(SBPayload, 1);

	payload->data = data;
	payload->callback = callback;
	payload->room = strdup(room_name);

	ma->ns_connection->sbpayload = payload;
	msn_message_send(ma->ns_connection, NULL, MSN_COMMAND_XFR, 1, MSN_SB);
	msn_connection_push_callback(ma->ns_connection, msn_got_sb_info, NULL);
}


void msn_sb_disconnected(MsnConnection *sb)
{
	LList *buddies = sb->account->buddies;

	while(buddies) {
		MsnBuddy *bud = buddies->data;

		if(bud->sb == sb) {
			bud->sb = NULL;
			break;
		}

		buddies = l_list_next(buddies);
	}

	sb->account->connections = l_list_remove(sb->account->connections, sb);

	msn_connection_free(sb);
}


void msn_sb_disconnect(MsnConnection *sb)
{
	msn_message_send(sb, NULL, MSN_COMMAND_OUT);

	msn_sb_disconnected(sb);
}


