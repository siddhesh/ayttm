/*
 * IRC protocol implementation
 *
 * Copyright (C) 2008, Siddhesh Poyarekar and the Ayttm Team
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

#include "libirc.h"
#include "ctcp.h"

static char irc_modes[] = {
	'a',
	'i',
	'w',
	'r',
	'o',
	'O',
	's'
};

/* LOGIN */
void irc_login(const char *password, int mode, irc_account *ia)
{
	char buff[BUF_LEN];
	memset(buff, 0, BUF_LEN);

	if (password && password[0]) {
		sprintf(buff, "PASS %s\n", password);
		ia->callbacks->irc_send_data(buff, strlen(buff), ia);
	}
	if (ia->nick) {
		sprintf(buff, "NICK %s\n", ia->nick);
		ia->callbacks->irc_send_data(buff, strlen(buff), ia);
	}
	if (ia->user) {
		sprintf(buff, "USER %s %d * :Ayttm user %s\n", ia->user, mode,
			ia->user);
		ia->callbacks->irc_send_data(buff, strlen(buff), ia);
	}
}

/* LOGOUT */
void irc_logout(irc_account *ia)
{
	char buff[BUF_LEN];
	memset(buff, 0, BUF_LEN);

	sprintf(buff, "QUIT :Ayttm logging off\n");
	ia->callbacks->irc_send_data(buff, strlen(buff), ia);
}

/* Send a PRIVMSG */
int irc_send_privmsg(const char *recipient, char *message, irc_account *ia)
{
	char *out_msg;
	int offset = 0;
	char buff[BUF_LEN];
	memset(buff, 0, BUF_LEN);
	int type = 0;

	if (!message)
		return IRC_NOECHO;

	while (message[offset] == ' ' || message[offset] == '\t')
		offset++;

	if (message[offset] == '/') {
		char *param_offset = NULL;

		/* It is some kind of command */

		message += offset + 1;

		param_offset = strchr(message, ' ');

		if (param_offset) {
			*param_offset = '\0';
			param_offset++;
		}

		type = irc_get_command_string(buff, recipient, message, param_offset,
			ia);

		/* reinstate the space so that we can put the message in our window intact */
		if (param_offset) {
			*(param_offset - 1) = ' ';
		}
	} else {
		out_msg = ctcp_encode(message, strlen(message));
		snprintf(buff, sizeof(buff), "PRIVMSG %s :%s\n", recipient,
			out_msg);

		if (out_msg)
			free(out_msg);
	}

	if (*buff)
		ia->callbacks->irc_send_data(buff, strlen(buff), ia);

	return type;
}

/* Send a NOTICE */
void irc_send_notice(const char *recipient, char *message, irc_account *ia)
{
	char *out_msg;
	char buff[BUF_LEN];
	memset(buff, 0, BUF_LEN);

	out_msg = ctcp_encode(message, strlen(message));

	sprintf(buff, "NOTICE %s :%s\n", recipient, out_msg);
	ia->callbacks->irc_send_data(buff, strlen(buff), ia);
}

/* Send WHOIS query */
void irc_send_whois(const char *target, const char *mask, irc_account *ia)
{
	char buff[BUF_LEN];
	memset(buff, 0, BUF_LEN);

	if (target)
		sprintf(buff, "WHOIS %s ", target);
	else
		sprintf(buff, "WHOIS ");

	strcat(buff, mask);
	strcat(buff, "\n");

	ia->callbacks->irc_send_data(buff, strlen(buff), ia);
}

/* Set Away */
void irc_set_away(char *message, irc_account *ia)
{
	char buff[BUF_LEN];
	memset(buff, 0, BUF_LEN);

	if (message) {
		/* Actually set away */
		sprintf(buff, "AWAY :%s\n", message);
		ia->callbacks->irc_send_data(buff, strlen(buff), ia);

	} else {
		/* Unset away */
		sprintf(buff, "AWAY\n");
		ia->callbacks->irc_send_data(buff, strlen(buff), ia);
	}
}

/* Set Modes */
void irc_set_mode(int irc_mode, irc_account *ia)
{
	char buff[BUF_LEN];
	memset(buff, 0, BUF_LEN);

	sprintf(buff, "MODE %s +%c\n", ia->nick, irc_modes[irc_mode]);
	ia->callbacks->irc_send_data(buff, strlen(buff), ia);
}

/* Unset modes */
void irc_unset_mode(int irc_mode, irc_account *ia)
{
	char buff[BUF_LEN];
	memset(buff, 0, BUF_LEN);

	sprintf(buff, "MODE %s -%c\n", ia->nick, irc_modes[irc_mode]);
	ia->callbacks->irc_send_data(buff, strlen(buff), ia);
}

/* Join a Channel */
void irc_join(const char *room, irc_account *ia)
{
	char buff[BUF_LEN];
	memset(buff, 0, BUF_LEN);

	sprintf(buff, "JOIN :%s\n", room);

	ia->callbacks->irc_send_data(buff, strlen(buff), ia);
}

/* Leave a Channel */
void irc_leave_chat_room(const char *room, irc_account *ia)
{
	char buff[BUF_LEN];
	memset(buff, 0, BUF_LEN);

	sprintf(buff, "PART :%s\n", room);

	ia->callbacks->irc_send_data(buff, strlen(buff), ia);
}

/* Send an Invite */
void irc_send_invite(const char *user, const char *room,
	const char *message, irc_account *ia)
{
	char buff[BUF_LEN];
	memset(buff, 0, BUF_LEN);

	if (*message) {
		sprintf(buff, "PRIVMSG %s :%s\n", user, message);

		ia->callbacks->irc_send_data(buff, strlen(buff), ia);
	}

	sprintf(buff, "INVITE %s %s\n", user, room);
	ia->callbacks->irc_send_data(buff, strlen(buff), ia);
}

void irc_get_names_list(const char *channel, irc_account *ia)
{
	char buff[BUF_LEN];
	memset(buff, 0, BUF_LEN);

	sprintf(buff, "NAMES %s\n", channel);
	ia->callbacks->irc_send_data(buff, strlen(buff), ia);
}

void irc_request_list(const char *channel, const char *target, irc_account *ia)
{
	char buff[BUF_LEN];
	memset(buff, 0, BUF_LEN);

	sprintf(buff, "LIST");
	if (channel) {
		strcat(buff, " ");
		strcat(buff, channel);
	}

	if (target) {
		strcat(buff, " ");
		strcat(buff, target);
	}

	strcat(buff, "\n");

	ia->callbacks->irc_send_data(buff, strlen(buff), ia);
}

/* Send a PONG message response. Called by the PING handler */
void irc_send_pong(const char *message, irc_account *ia)
{
	char buff[BUF_LEN];
	memset(buff, 0, BUF_LEN);

	sprintf(buff, "PONG :%s\n", message);
	ia->callbacks->irc_send_data(buff, strlen(buff), ia);
}

/* Get PRIVMSG. CTCP implementation called here. */
void irc_process_privmsg(const char *to, const char *message,
	irc_message_prefix *prefix, irc_account *ia)
{
	/* 
	 * This only decodes. You will want to call ctcp_get_extended_data() to actually 
	 * get the complete break-up of the string into constituent commands
	 */
	char *out_msg = ctcp_decode(message, strlen(message));

	ia->callbacks->got_privmsg(to, out_msg, prefix, ia);

	if (out_msg) {
		free(out_msg);
	}
}

/* Get PRIVMSG. CTCP implementation called here. */
void irc_process_notice(const char *to, const char *message,
	irc_message_prefix *prefix, irc_account *ia)
{
	/* 
	 * This only decodes. You will want to call ctcp_get_extended_data() to actually 
	 * get the complete break-up of the string into constituent commands
	 */
	char *out_msg = ctcp_decode(message, strlen(message));

	ia->callbacks->incoming_notice(to, out_msg, prefix, ia);

	if (out_msg) {
		free(out_msg);
	}
}

irc_name_list *irc_gen_name_list(char *message)
{
	char *offset = NULL;
	irc_name_list *list = NULL;
	irc_name_list *head = NULL;

	while (message && *message) {
		offset = strchr(message, ' ');

		if (list) {
			list->next =
				(irc_name_list *)calloc(1,
				sizeof(irc_name_list));
			list = list->next;
		} else if (!list) {
			list = (irc_name_list *)calloc(1,
				sizeof(irc_name_list));
			head = list;
		}

		if (offset) {
			*offset = '\0';
		}

		if (*message == '+' || *message == '@') {
			list->attribute = *message;
			message++;
		} else
			list->attribute = '\0';

		list->name = strdup(message);

		/* We're at the end of the list... get out now... */
		if (!offset)
			break;

		message = offset + 1;
	}

	return head;
}

/* Add a parameter to the list and return its new head */
irc_param_list *irc_param_list_add(irc_param_list *param_list,
	const char *param)
{
	irc_param_list *head = param_list;
	irc_param_list *new =
		(irc_param_list *)calloc(1, sizeof(irc_param_list));

	if (!param_list) {
		new->param = strdup(param);
		new->next = NULL;

		return new;
	}

	while (param_list->next)
		param_list = param_list->next;

	param_list->next = new;

	new->param = strdup(param);
	new->next = NULL;

	return head;
}

/* Free the parameter list */
void irc_param_list_free(irc_param_list *param_list)
{
	irc_param_list *to_be_freed = NULL;

	while (param_list) {
		to_be_freed = param_list;
		param_list = param_list->next;
		free(to_be_freed);
		to_be_freed = NULL;
	}
}

/* Get nth paramater in the list */
char *irc_param_list_get_at(irc_param_list *list, int position)
{
	int cur = 0;

	if (!list)
		return NULL;

	while (list->next && cur < position) {
		list = list->next;
		cur++;
	}

	if (list)
		return list->param;
	else
		return NULL;
}

int irc_recv(irc_account *ia, char *buf, int len)
{
	if (buf[len] == '\n') {
		char *fbuf;

		if (buf[len - 1] != '\r')
			return 0;

		fbuf = strdup(buf);

		fbuf[len - 1] = '\0';
		irc_message_parse(fbuf, ia);

		free(fbuf);

		return 1;
	}

	return 0;
}
