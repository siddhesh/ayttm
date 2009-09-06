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


/* Here we handle stuff related to messages like receiving and building,
 * sending, etc. */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "msn-message.h"
#include "msn-sb.h"
#include "msn-util.h"
#include "msn-connection.h"
#include "msn-ext.h"

/* You can fill this only if you think that 1664 bytes for payload ought 
 * not to be enough for anybody right...
 */
#define MAX_PARMS 64

static void msn_command_got_VER (MsnConnection *mc);
static void msn_command_got_CVR (MsnConnection *mc);
static void msn_command_got_USR (MsnConnection *mc);
static void msn_command_got_XFR (MsnConnection *mc);
 
static void msn_command_got_ILN (MsnConnection *mc);
static void msn_command_got_BLP (MsnConnection *mc);
static void msn_command_got_MSG (MsnConnection *mc);
                   
static void msn_command_got_ADL (MsnConnection *mc);
static void msn_command_got_ADG (MsnConnection *mc);
static void msn_command_got_CHG (MsnConnection *mc);
static void msn_command_got_FQY (MsnConnection *mc);
static void msn_command_got_GCF (MsnConnection *mc);
static void msn_command_got_OUT (MsnConnection *mc);
static void msn_command_got_PNG (MsnConnection *mc);
static void msn_command_got_QNG (MsnConnection *mc);
static void msn_command_got_QRY (MsnConnection *mc);
static void msn_command_got_SBS (MsnConnection *mc);
static void msn_command_got_REA (MsnConnection *mc);
static void msn_command_got_RML (MsnConnection *mc);
static void msn_command_got_RMG (MsnConnection *mc);
static void msn_command_got_UBX (MsnConnection *mc);
static void msn_command_got_SDC (MsnConnection *mc);
static void msn_command_got_IMS (MsnConnection *mc);
                   
static void msn_command_got_CHL (MsnConnection *mc);
static void msn_command_got_FLN (MsnConnection *mc);
static void msn_command_got_NLN (MsnConnection *mc);
static void msn_command_got_RNG (MsnConnection *mc);
static void msn_command_got_NOT (MsnConnection *mc);
                   
static void msn_command_got_ANS (MsnConnection *mc);
static void msn_command_got_IRO (MsnConnection *mc);
static void msn_command_got_CAL (MsnConnection *mc);
static void msn_command_got_JOI (MsnConnection *mc);
static void msn_command_got_PRP (MsnConnection *mc);
static void msn_command_got_BYE (MsnConnection *mc);

static void msn_command_got_ACK (MsnConnection *mc);
static void msn_command_got_NAK (MsnConnection *mc);

static void msn_command_parse_payload_MSG (MsnMessage *msg);
static void msn_command_parse_payload_GCF (MsnMessage *msg);
static void msn_command_parse_payload_QRY (MsnMessage *msg);
static void msn_command_parse_payload_ADL (MsnMessage *msg);
static void msn_command_parse_payload_FQY (MsnMessage *msg);
static void msn_command_parse_payload_UBX (MsnMessage *msg);

typedef struct {
	char *name;
	int command_num;
	int num_args;
	int payload_size_arg;
	MsnCommandHandler handler;
	MsnCommandPayloadHandler payload_handler;
} MsnCommandInfo ;


typedef struct {
	char *names[MAX_PARMS];
	char *values[MAX_PARMS];
	int parm_count;
	char * body;
} MsnMessagePayload;


MsnCommandInfo msn_commands[] = {
	{ "VER", MSN_COMMAND_VER,  3, 0, msn_command_got_VER, NULL },
	{ "CVR", MSN_COMMAND_CVR,  9, 0, msn_command_got_CVR, NULL },
	{ "USR", MSN_COMMAND_USR, -1, 0, msn_command_got_USR, NULL },
	{ "XFR", MSN_COMMAND_XFR, -1, 0, msn_command_got_XFR, NULL },
                                          
	{ "ILN", MSN_COMMAND_ILN,  7, 0, msn_command_got_ILN, NULL },
	{ "BLP", MSN_COMMAND_BLP,  2, 0, msn_command_got_BLP, NULL },
	{ "MSG", MSN_COMMAND_MSG, -1, 3, msn_command_got_MSG, msn_command_parse_payload_MSG },
                                                            
	{ "ADL", MSN_COMMAND_ADL,  2, 2, msn_command_got_ADL, msn_command_parse_payload_ADL },
	{ "ADG", MSN_COMMAND_ADG,  0, 0, msn_command_got_ADG, NULL },
	{ "CHG", MSN_COMMAND_CHG,  2, 0, msn_command_got_CHG, NULL },
	{ "FQY", MSN_COMMAND_FQY,  2, 2, msn_command_got_FQY, msn_command_parse_payload_FQY },
	{ "GCF", MSN_COMMAND_GCF,  2, 2, msn_command_got_GCF, msn_command_parse_payload_GCF },
	{ "OUT", MSN_COMMAND_OUT,  1, 0, msn_command_got_OUT, NULL },
	{ "PNG", MSN_COMMAND_PNG,  0, 0, msn_command_got_PNG, NULL },
	{ "QNG", MSN_COMMAND_QNG,  0, 0, msn_command_got_QNG, NULL },
	{ "QRY", MSN_COMMAND_QRY,  3, 3, msn_command_got_QRY, msn_command_parse_payload_QRY },
	{ "SBS", MSN_COMMAND_SBS,  2, 0, msn_command_got_SBS, NULL },
	{ "REA", MSN_COMMAND_REA,  0, 0, msn_command_got_REA, NULL },
	{ "RML", MSN_COMMAND_RML,  2, 2, msn_command_got_RML, NULL },
	{ "RMG", MSN_COMMAND_RMG,  0, 0, msn_command_got_RMG, NULL },
	{ "UBX", MSN_COMMAND_UBX,  3, 3, msn_command_got_UBX, msn_command_parse_payload_UBX },
	{ "SDC", MSN_COMMAND_SDC,  0, 0, msn_command_got_SDC, NULL },
	{ "IMS", MSN_COMMAND_IMS,  0, 0, msn_command_got_IMS, NULL },
                                                            
	{ "CHL", MSN_COMMAND_CHL,  0, 0, msn_command_got_CHL, NULL },
	{ "FLN", MSN_COMMAND_FLN,  2, 0, msn_command_got_FLN, NULL },
	{ "NLN", MSN_COMMAND_NLN,  6, 0, msn_command_got_NLN, NULL },
	{ "RNG", MSN_COMMAND_RNG,  6, 0, msn_command_got_RNG, NULL },
	{ "NOT", MSN_COMMAND_NOT,  0, 0, msn_command_got_NOT, NULL },
                                                            
	{ "ANS", MSN_COMMAND_ANS,  4, 0, msn_command_got_ANS, NULL },
	{ "IRO", MSN_COMMAND_IRO,  0, 0, msn_command_got_IRO, NULL },
	{ "CAL", MSN_COMMAND_CAL,  2, 0, msn_command_got_CAL, NULL },
	{ "JOI", MSN_COMMAND_JOI,  2, 0, msn_command_got_JOI, NULL },
	{ "BYE", MSN_COMMAND_BYE,  0, 0, msn_command_got_BYE, NULL },
	{ "PRP", MSN_COMMAND_PRP,  3, 0, msn_command_got_PRP, NULL },
	{ "ACK", MSN_COMMAND_ACK,  0, 0, msn_command_got_ACK, NULL },
	{ "NAK", MSN_COMMAND_NAK,  0, 0, msn_command_got_NAK, NULL }
} ;


MsnCommand msn_command_get_from_string(char *cmd)
{
	int prospective_command=0;

	/* Save us the agony of browsing through the list for an error message */
	if( (prospective_command = atoi(cmd)) > 0 )
		return prospective_command;

	for ( prospective_command = 0; prospective_command<MSN_COMMAND_COUNT; prospective_command++)
		if(!strcmp(msn_commands[prospective_command].name, cmd))
			return prospective_command;

	return MSN_COMMAND_INVALID;
}


static void msn_command_parse_payload_FQY (MsnMessage *msg)
{
	char *start = NULL;
	LList *newbuds = NULL;
	start = strstr(msg->payload, "<d n=");

	while(start) {
		char *users = NULL, *user = NULL;
		char *domain = start+6;
		char *end = strchr(domain, '>');

		*(end-1) = '\0';
		users = end+1;
		end = strstr(end, "</d>");
		*end = '\0';

		while((user = strstr(users, "<c "))) {
			char buddy_buf[255];
			char *buddy_name;
			char *attr = NULL, *attr_end = NULL;
			int type = 0;
			int list = 0;
			char *uend = strstr(user, "/>");

			MsnBuddy *bud = m_new0(MsnBuddy, 1);

			user += 3;
			*uend = '\0';

			attr = strstr(user, "n=\"");
			attr += 3;
			attr_end = strchr(attr, '\"');

			*attr_end = '\0';
			buddy_name = strdup(attr);
			*attr_end = '\"';

			if((attr = strstr(user, "t=\"")))
				type = atoi(attr+3);

			snprintf(buddy_buf, sizeof(buddy_buf), "%s@%s", buddy_name, domain);

			bud->passport = strdup(buddy_buf);
			bud->type = type;
			bud->list = list;

			newbuds = l_list_append(newbuds, bud);

			free(buddy_name);

			users = uend+1;
		}

		start = strstr(end+1, "<d n=");
	}

	msg->payload_info = newbuds;
}


static void msn_command_parse_payload_ADL (MsnMessage *msg)
{
	char *start = NULL;
	LList *newbuds = NULL;
	start = strstr(msg->payload, "<d n=");

	while(start) {
		char *users = NULL, *user = NULL;
		char *domain = start+6;
		char *end = strchr(domain, '>');

		*(end-1) = '\0';
		users = end+1;
		end = strstr(end, "</d>");
		*end = '\0';

		while((user = strstr(users, "<c "))) {
			char buddy_buf[255];
			char *buddy_name;
			char *attr = NULL, *attr_end = NULL;
			int type = 0;
			int list = 0;
			char *uend = strstr(user, "/>");

			MsnBuddy *bud = m_new0(MsnBuddy, 1);

			user += 3;
			*uend = '\0';

			attr = strstr(user, "n=\"");
			attr += 3;
			attr_end = strchr(attr, '\"');

			*attr_end = '\0';
			buddy_name = strdup(attr);
			*attr_end = '\"';

			if((attr = strstr(user, "t=\"")))
				type = atoi(attr+3);

			if((attr = strstr(user, "l=\"")))
				list = atoi(attr+3);

			snprintf(buddy_buf, sizeof(buddy_buf), "%s@%s", buddy_name, domain);

			bud->passport = strdup(buddy_buf);
			bud->type = (type == 1)?MSN_BUDDY_PASSPORT:MSN_BUDDY_EMAIL;
			bud->list = list;

			newbuds = l_list_append(newbuds, bud);

			free(buddy_name);

			users = uend+1;
		}

		start = strstr(end+1, "<d n=");
	}

	msg->payload_info = newbuds;
}


static void msn_command_parse_payload_GCF (MsnMessage *msg)
{

}


static void msn_command_parse_payload_UBX (MsnMessage *msg)
{

}


static void msn_command_parse_payload_QRY (MsnMessage *msg)
{

}


static void msn_command_parse_payload_MSG (MsnMessage *msg)
{
	char *cur = msg->payload, *divide = NULL;

	MsnMessagePayload *payload = m_new0(MsnMessagePayload, 1);

	/* The divide between parameters and data */
	divide = strstr(msg->payload, "\r\n\r\n");

	/* You have a Message */
	if (divide) {
		*divide = '\0';
		payload->body = divide+4;
	}

	/* Ok, do the parameters now */
	while (cur && *cur) {
		char *eol = NULL, *separator = NULL;
		
		payload->names[payload->parm_count] = cur;

		/* Take a line... */
		eol = strstr(cur, "\r\n");

		if(eol)
			*eol = '\0';

		/* ... and split it. */
		separator = strstr(cur, ": ");

		if(separator) {
			*separator = '\0';
			payload->values[payload->parm_count] = separator+2;
		}

		payload->parm_count++;

		if ( payload->parm_count > MAX_PARMS ) {
			fprintf(stderr, "Somebody's gone insane. Let's get out of here...\n");
			break;
		}

		if(eol)
			cur = eol+2;
		else
			cur = NULL;

		/* rinse and repeat */
	}

	msg->payload_info = payload;
}


MsnCommandPayloadHandler msn_command_get_payload_handler(MsnMessage *msg)
{
	int payload_size = 0;

	if (msg->command <= MSN_COMMAND_INVALID || msg->command >= MSN_COMMAND_COUNT )
		return NULL;

	payload_size = msn_commands[msg->command].payload_size_arg;

	/* Don't send a handler for 0 size */
	if( payload_size && msg->argc >= payload_size + 1 && atoi(msg->argv[payload_size]))
		return msn_commands[msg->command].payload_handler;
	else
		return NULL;
}


int msn_command_set_payload_size(MsnMessage *msg)
{
	if (!msn_command_get_payload_handler(msg))
		return 0;

	if ( !msg || !msg->argv || msg->argc < msn_commands[msg->command].payload_size_arg + 1 )
		return 0;

	msg->size = atoi(msg->argv[msn_commands[msg->command].payload_size_arg]);

	return 1;
}


void msn_command_parse_payload (MsnMessage *msg)
{
	MsnCommandPayloadHandler parse_payload;

	 if ( (parse_payload = msn_command_get_payload_handler(msg)) ) {
	 	parse_payload(msg);
	}
}


int msn_command_handle(MsnConnection *mc)
{
	MsnCommandHandler handler;
	if( !(handler = msn_commands[mc->current_message->command].handler) )
		return 0;
	
	handler(mc);

	return 1;
}

int msn_command_get_num_args(MsnCommand cmd)
{
	return msn_commands[cmd].num_args;
}


const char *msn_command_get_name(MsnCommand cmd)
{
	return msn_commands[cmd].name;
}


/* Command handlers */

static void msn_command_got_VER(MsnConnection *mc)
{

}


static void msn_command_got_CVR (MsnConnection *mc)
{

}


static void msn_command_got_USR (MsnConnection *mc)
{

}


static void msn_command_got_XFR (MsnConnection *mc)
{

}

 
static void msn_command_got_ILN (MsnConnection *mc)
{
	MsnMessage *msg = mc->current_message;
	LList *buds = mc->account->buddies;
	MsnBuddy *bud = NULL;

	while(buds) {
		bud = buds->data;

		if(!strcmp(bud->passport, msg->argv[3])) {
			if(!bud->friendlyname || strcmp(bud->friendlyname, msg->argv[5])) {
				free(bud->friendlyname);
				bud->friendlyname = msn_urldecode(msg->argv[5]);
			}

			bud->status = msn_get_status_num(msg->argv[2]);
			break;
		}

		buds = l_list_next(buds);
	}

	if(buds && bud)
		ext_got_buddy_status(mc, bud);
	else
		fprintf(stderr, "Got ILN for some unknown person %s(%s)\n", msg->argv[5], msg->argv[3]);
}


static void msn_command_got_BLP (MsnConnection *mc)
{
	/* Simply echoed my BLP back. Don't care */
}


static void msn_command_got_MSG (MsnConnection *mc)
{
	int i=0;
	MsnMessagePayload *payload_info = mc->current_message->payload_info;
	MsnIM *im = NULL;

	LList *l;
	MsnBuddy *bud = NULL;

	char *nick = mc->current_message->argv[1];

	for(i=0;i < payload_info->parm_count; i++) {
		if(!strcmp(payload_info->names[i], "TypingUser")) {
			LList *l;
			MsnBuddy *bud = NULL;

			for(l = mc->account->buddies; l; l = l_list_next(l)) {
				bud = l->data;

				if(!strcmp(bud->passport, payload_info->values[i]))
					break;
			}

			if(l && bud)
				ext_got_typing(mc, bud);
			else
				printf("Got typing info for an unknown user %s\n", payload_info->values[i]);

			return;
		}

		if(!strcmp(payload_info->names[i], "Content-Type")) {
			/* Not interested in non-IM messages right now */
			if(strncmp(payload_info->values[i], "text/plain;", 11))
				return;
		}


		if(!strcmp(payload_info->names[i], "X-MMS-IM-Format")) {
			char *top, *start = NULL, *end = NULL;
			im = m_new0(MsnIM, 1);

			top = payload_info->values[i];
			start = strstr(payload_info->values[i], "FN=");

			if(start) {
				start += 3;
				end = strchr(start, ';');

				if(end)
					*end = '\0';

				im->font = strdup(start);

				if(end)
					start = end + 1;
				else
					start = end;
			}

			if(!start)
				start = top;

			top = start;

			start = strstr(start, "EF=");

			if(start) {
				start += 3;
				end = strchr(start, ';');

				if(end)
					*end = '\0';

				if(strchr(start, 'B'))
					im->bold = 1;
				if(strchr(start, 'I'))
					im->italic = 1;
				if(strchr(start, 'U'))
					im->underline = 1;

				if(end)
					start = end + 1;
				else
					start = end;
			}

			if(!start)
				start = top;

			top = start;

			start = strstr(start, "CO=");

			if(start) {
				start += 3;
				end = strchr(start, ';');

				if(end)
					*end = '\0';

				im->color = strdup(start);

				if(end)
					start = end + 1;
				else
					start = end;
			}
		}
	}

	if(!im)
		im = m_new0(MsnIM, 1);

	im->body = payload_info->body?strdup(payload_info->body):strdup("");

	for(l = mc->account->buddies; l; l = l_list_next(l)) {
		bud = l->data;

		if(!strcmp(bud->passport, nick))
			break;
	}

	if(l && bud)
		ext_got_IM(mc, im, bud);
	else {
		printf("%s is trying to message me despite not being in my list\n", nick);
		ext_got_unknown_IM(mc, im, nick);
	}

	free(payload_info);
	free(im->body);
	free(im->color);
	free(im->font);
	free(im);

	mc->current_message->payload_info = NULL;
}

                   
static void msn_command_got_ADL (MsnConnection *mc)
{
	LList *l1, *l2;
	LList *newbuds = mc->current_message->payload_info;
	MsnAccount *ma = mc->account;

	for(l1 = newbuds; l1; l1 = l_list_next(l1)) {
		int exists = 0;
		MsnBuddy *newbud = l1->data;

		/* See if this buddy exists */
		for(l2 = ma->buddies; l2; l2 = l_list_next(l2)) {
			MsnBuddy *bud = l2->data;
			if(!strcmp(newbud->passport, bud->passport)) {
				/* Don't want to mess up the allow and block lists accidentally */
				if(!(bud->list & MSN_BUDDY_ALLOW || bud->list & MSN_BUDDY_BLOCK))
					bud->list = newbud->list;

				msn_buddy_free(newbud);
				exists = 1;
				break;
			}
		}

		if(exists)
			continue;

		if(ext_buddy_request(ma, newbud))
			msn_buddy_allow(ma, newbud);

		ma->buddies = l_list_append(ma->buddies, newbud);
	}

	l_list_free(newbuds);
	mc->current_message->payload_info = NULL;
}


static void msn_command_got_ADG (MsnConnection *mc)
{

}


static void msn_command_got_CHG (MsnConnection *mc)
{
	mc->account->status = msn_get_status_num(mc->current_message->argv[2]);

	ext_got_status_change(mc->account);
}


static void msn_command_got_FQY (MsnConnection *mc)
{
	LList *l1, *l2;
	LList *newbuds = mc->current_message->payload_info;
	MsnAccount *ma = mc->account;

	for(l1 = newbuds; l1; l1 = l_list_next(l1)) {
		MsnBuddy *newbud = l1->data;

		/* See if this buddy exists */
		for(l2 = ma->buddies; l2; l2 = l_list_next(l2)) {
			MsnBuddy *bud = l2->data;
			if(!strcmp(newbud->passport, bud->passport)) {
				newbud->list = bud->list;
				break;
			}
		}
	}

	msn_buddies_send_adl(mc->account, newbuds, 0, 0);

	for(l1 = newbuds; l1; l1 = l_list_next(l1))
		msn_buddy_free(l1->data);

	l_list_free(newbuds);
	mc->current_message->payload_info = NULL;
}


static void msn_command_got_GCF (MsnConnection *mc)
{
	msn_connection_free_current_message(mc);	/* Doing nothing with this for now */
}


static void msn_command_got_OUT (MsnConnection *mc)
{

}


static void msn_command_got_PNG (MsnConnection *mc)
{

}


static void msn_command_got_QNG (MsnConnection *mc)
{

}


static void msn_command_got_QRY (MsnConnection *mc)
{

}


static void msn_command_got_SBS (MsnConnection *mc)
{
	/* Got SBS. Don't know what it is but it means we're logged in */
}


static void msn_command_got_REA (MsnConnection *mc)
{

}


static void msn_command_got_RML (MsnConnection *mc)
{

}


static void msn_command_got_RMG (MsnConnection *mc)
{

}


static void msn_command_got_UBX (MsnConnection *mc)
{

}


static void msn_command_got_SDC (MsnConnection *mc)
{

}


static void msn_command_got_IMS (MsnConnection *mc)
{

}


                   
static void msn_command_got_CHL (MsnConnection *mc)
{
	msn_send_chl_response(mc->account, mc->current_message->argv[2]);
}


static void msn_command_got_FLN (MsnConnection *mc)
{
	MsnMessage *msg = mc->current_message;
	LList *buds = mc->account->buddies;
	MsnBuddy *bud = NULL;

	while(buds) {
		bud = buds->data;

		if(!strcmp(bud->passport, msg->argv[1])) {
			bud->status = MSN_STATE_OFFLINE;
			break;
		}

		buds = l_list_next(buds);
	}

	if(buds && bud)
		ext_got_buddy_status(mc, bud);
	else
		fprintf(stderr, "Got FLN for some unknown person %s\n", msg->argv[1]);
}


static void msn_command_got_NLN (MsnConnection *mc)
{
	MsnMessage *msg = mc->current_message;
	LList *buds = mc->account->buddies;
	MsnBuddy *bud = NULL;

	while(buds) {
		bud = buds->data;

		if(!strcmp(bud->passport, msg->argv[2])) {
			if(strcmp(bud->friendlyname, msg->argv[4])) {
				free(bud->friendlyname);
				bud->friendlyname = msn_urldecode(msg->argv[4]);
			}

			bud->status = msn_get_status_num(msg->argv[1]);
			break;
		}

		buds = l_list_next(buds);
	}

	if(buds && bud)
		ext_got_buddy_status(mc, bud);
	else
		fprintf(stderr, "Got NLN for some unknown person %s(%s)\n", msg->argv[4], msg->argv[2]);
}


static void msn_command_got_RNG (MsnConnection *mc)
{
	MsnBuddy *bud;
	int do_connect = 1;
	LList *buds = mc->account->buddies;

	char *nick = mc->current_message->argv[5];

	while(buds) {
		bud = buds->data;

		if(!strcmp(bud->passport, nick) && (bud->type & ~MSN_BUDDY_BLOCK))
			break;

		buds = l_list_next(buds);
	}

	/* TODO Get confirmation from the user to know if she would like to chat with someone unknown */
	if(!buds) {
		do_connect = ext_confirm_invitation(mc, nick);
		bud = NULL;
	}

	if(do_connect)
		msn_connect_sb_with_info(mc, nick, bud);
}


static void msn_command_got_NOT (MsnConnection *mc)
{

}


                   
static void msn_command_got_ANS (MsnConnection *mc)
{

}


static void msn_command_got_IRO (MsnConnection *mc)
{

}


static void msn_command_got_CAL (MsnConnection *mc)
{

}


static void msn_command_got_JOI (MsnConnection *mc)
{
	msn_sb_got_join(mc);
}


static void msn_command_got_PRP (MsnConnection *mc)
{
	free(mc->account->friendlyname);
	mc->account->friendlyname = strdup(mc->current_message->argv[3]);

	ext_update_friendlyname(mc);
}


static void msn_command_got_BYE (MsnConnection *mc)
{
	mc->sbpayload->num_members--;
	ext_buddy_left(mc, mc->current_message->argv[1]);
}


static void msn_command_got_ACK (MsnConnection *mc)
{

}


static void msn_command_got_NAK (MsnConnection *mc)
{

}


