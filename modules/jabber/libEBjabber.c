/*
 * libEBjabber
 *
 * Copyright (C) 2000, Alex Wheeler <wheelear@yahoo.com>
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
 *	This code borrowed heavily from the jabber client jewel
 *	(http://jewel.sourceforge.net/)
 */

#include "intl.h"

#define DEBUG
#include <stdio.h>
#include <jabber/jabber.h>

#include "chat_room.h"
#include "plugin_api.h"
#include "libEBjabber.h"
#include "messages.h"
#include "dialog.h"

#ifdef __MINGW32__
#include <glib.h>
#define snprintf _snprintf
#endif

extern void JABBERStatusChange(struct jabber_buddy *jb);
extern void JABBERAddBuddy(void *data);
extern void JABBERInstantMessage(void *data);
extern void JABBERDialog(void *data);
extern void JABBERListDialog(const char **list, void *data);
extern void JABBERError(char *message, char *title);
extern void JABBERBuddy_typing(JABBER_Conn *JConn, char *from, int typing);
extern void JABBERLogout(void *data);
extern void JABBERChatRoomMessage(char *id, char *user, char *message);
extern void JABBERChatRoomBuddyStatus(char *id, char *user, int offline);
extern void JABBERDelBuddy(JABBER_Conn *JConn, void *data);
extern void JABBERConnected(void *data);
extern void JABBERNotConnected(void *data);

void j_on_state_handler(jconn conn, int state);
void j_on_packet_handler(jconn conn, jpacket packet);
void j_on_create_account(void *data);
void j_allow_subscribe(void *data);
void j_unsubscribe(void *data);
void j_on_pick_account(void *data);
void j_remove_agents_from_host(char *host);
Agent *j_find_agent_by_type(char *agent_type);

/******************************************************
 * Support routines for multiple simultaneous accounts
 *****************************************************/

JABBER_Conn *Connections = NULL;
GList *agent_list = NULL;

JABBER_Conn *JCnewConn(void)
{
	JABBER_Conn *jnew = NULL;

	/* Create a new connection struct, and put it on the top of the list */
	jnew = calloc(1, sizeof(JABBER_Conn));
	jnew->next = Connections;
	eb_debug(DBG_JBR, "JCnewConn: %p\n", jnew);
	Connections = jnew;
	jnew->state = JCONN_STATE_OFF;
	return (Connections);
}

JABBER_Conn *JCfindConn(jconn conn)
{
	JABBER_Conn *current = Connections;

	/*FIXME: Somehow current==current->next, and we get an endless loop */
	while (current) {
		eb_debug(DBG_JBR, "conn=%p current=%p\n", conn, current);
		if (conn == current->conn)
			return (current);
		/*FIXME: This is not the right place to fix this, why is it happening? */
		if (current == current->next) {
			current->next = NULL;
			fprintf(stderr,
				"Endless jabber connection loop broken\n");
		}
		current = current->next;
	}
	return (NULL);
}

char *JCgetServerName(JABBER_Conn *JConn)
{
	if (!JConn || !JConn->conn || !JConn->conn->user)
		return (NULL);
	return (JConn->conn->user->server);
}

JABBER_Conn *JCfindServer(char *server)
{
	JABBER_Conn *current = Connections;

	while (current) {
		if (current->conn) {
			eb_debug(DBG_JBR, "Server: %s\n",
				current->conn->user->server);
			if (!strcmp(server, current->conn->user->server)) {
				return (current);
			}
		}
		current = current->next;
	}
	return (NULL);
}

JABBER_Conn *JCfindJID(char *JID)
{
	JABBER_Conn *current = Connections;

	while (current) {
		eb_debug(DBG_JBR, "JID: %s\n", current->jid);
		if (!strcmp(JID, current->jid)) {
			return (current);
		}
		current = current->next;
	}
	return (NULL);
}

int JCremoveConn(JABBER_Conn *JConn)
{
	JABBER_Conn *current = Connections, *last = current;

	while (current) {
		if (JConn == current) {
			if (last != current)
				last->next = current->next;
			else
				Connections = current->next;
			g_free(current);
			return (0);
		}
		last = current;
		current = current->next;
	}
	return (-1);
}

char **JCgetJIDList(void)
{
	JABBER_Conn *current = Connections;
	char **list = NULL;
	int count = 0;

	while (current) {
		/* Leave room for the last null entry */
		list = realloc(list, sizeof(char *) * (count + 2));
		eb_debug(DBG_JBR, "current->jid[%p]\n", current->jid);
		list[count] = current->jid;
		current = current->next;
		count++;
	}
	if (list)
		list[count] = NULL;
	return (list);
}

void JCfreeServerList(char **list)
{
	int count = 0;
	while (list[count]) {
		free(list[count++]);
	}
	free(list);
}

/****************************************
 * End jabber multiple connection routines
 ****************************************/

void jabber_callback_handler(AyConnection *con, eb_input_condition cond,
	void *data)
{
	JABBER_Conn *JConn = data;
	/* Let libjabber do the work, it calls what we setup in jab_packet_handler */
	jab_poll(JConn->conn);
	/* Is this connection still good? */
	if (!JConn->conn) {
		eb_debug(DBG_JBR, "Logging out because JConn->conn is NULL\n");
		JABBERLogout(JConn);
		ay_connection_input_remove(JConn->listenerID);
	} else if (JConn->conn->state == JCONN_STATE_OFF) {
		/* No, clean up */
		JABBERLogout(JConn);
		ay_connection_input_remove(JConn->listenerID);
		jab_delete(JConn->conn);
		JConn->conn = NULL;
	}
}

int ext_jabber_connect(jconn j, AyConnectCallback callback)
{
	JABBER_Conn *conn = JCfindConn(j);

	conn->connection =
		ay_connection_new(j->serv, j->user->port,
		j->usessl ? AY_CONNECTION_TYPE_SSL : AY_CONNECTION_TYPE_PLAIN);

	if (!j->usessl)
		return ay_connection_connect(conn->connection, callback, NULL,
			NULL, j);
	else
		return ay_connection_connect(conn->connection, callback, NULL,
			eb_do_confirm_dialog, j);
}

void ext_jabber_disconnect(jconn j)
{
	JABBER_Conn *conn = JCfindConn(j);

	if (!conn) {
		printf("WHAT THE HELL ARE WE TRYING TO FREE(%p)?!?!?!\n", j);
		return;
	}

	printf("Freeing %p\n", conn->connection);

	ay_connection_free(conn->connection);
	conn->connection = NULL;
}

int ext_jabber_read(jconn j, char *buf, int len)
{
	JABBER_Conn *conn = JCfindConn(j);

	return ay_connection_read(conn->connection, buf, len);
}

int ext_jabber_write(jconn j, const char *buf, int len)
{
	JABBER_Conn *conn = JCfindConn(j);

	return ay_connection_write(conn->connection, buf, len);
}

/* Functions called from the ayttm jabber.c file */

int JABBER_Login(char *handle, char *passwd, char *host,
	eb_jabber_local_account_data *jlad, int port)
{
	/* At this point, we don't care about host and port */
	char jid[256 + 1];
	int tag;
	char buff[4096];
	char server[256], *ptr = NULL;
	JABBER_Conn *JConn = NULL;

	/* If a different connect server is not specified then atleast connect to the default */
	if (!strcmp(jlad->connect_server, "")) {
		eb_debug(DBG_JBR, "jlad->connect_server is BLANK!\n\n");
		strcpy(jlad->connect_server, host);
	}

	eb_debug(DBG_JBR, "%s %s %i\n", handle, host, port);
	/* Make sure the name is correct, support all formats
	 * handle       become handle@host/ayttm
	 * handle@foo   becomes handle@foo/ayttm
	 * handle@foo/bar is not changed
	 */

	if (!strchr(handle, '@')) {
		/* A host must be specified in the config file */
		if (!host) {
			JABBERError(_("No jabber server specified."),
				_("Cannot login"));
			return (0);
		}
		snprintf(jid, 256, "%s@%s/ayttm", handle, host);
	} else if (!strchr(handle, '/'))
		snprintf(jid, 256, "%s/ayttm", handle);
	else
		strncpy(jid, handle, 256);

	/* Extract the server name */
	strcpy(server, jid);
	ptr = strchr(server, '@');
	ptr++;
	host = strtok(ptr, "@/");

	eb_debug(DBG_JBR, "jid: %s\n", jid);
	JConn = JCnewConn();
	strncpy(JConn->jid, jid, LINE_LENGTH);
	/* We assume we have an account, and don't need to register one */
	JConn->reg_flag = 0;
	JConn->conn = jab_new(jid, passwd, jlad->connect_server);
	if (!JConn->conn) {
		snprintf(buff, 4096, "Connection to server '%s' failed.", host);
		JABBERError(buff, _("Jabber Error"));
		JABBERNotConnected(JConn);
		free(JConn);
		return (0);
	} else if (JConn->conn->user == NULL) {
		snprintf(buff, 4096,
			"Error connecting to server '%s':\n   Invalid user name.",
			host);
		JABBERError(buff, _("Jabber Error"));
		JABBERNotConnected(JConn);
		free(JConn);
		return (0);
	}

	jab_packet_handler(JConn->conn, j_on_packet_handler);
	jab_state_handler(JConn->conn, j_on_state_handler);

	JConn->conn->user->port = port;
	JConn->conn->usessl = jlad->use_ssl;
	JConn->do_request_gmail = jlad->request_gmail;

	tag = jab_start(JConn->conn);

	return tag;
}

int JABBER_AuthorizeContact(JABBER_Conn *conn, char *handle)
{
	eb_debug(DBG_JBR, "%s\n", handle);
	return (0);
}

int JABBER_SendChatRoomMessage(JABBER_Conn *JConn, char *room_name,
	char *message, char *nick)
{
	char from[256], to[256];
	xmlnode x;
	Agent *agent = j_find_agent_by_type("groupchat");
	if (!JConn) {
		eb_debug(DBG_JBR,
			"******Called with NULL JConn for room %s!!!\n",
			room_name);
		return (0);
	}

	if (!agent) {
		eb_debug(DBG_JBR,
			"Could not find private group chat agent to send message\n");
		return (-1);
	}

	if (strstr(room_name, "@")) {
		sprintf(to, "%s", room_name);
		sprintf(from, "%s/%s", room_name, nick);
	} else {
		sprintf(to, "%s@%s", room_name, agent->alias);
		sprintf(from, "%s@%s/%s", room_name, agent->alias, nick);
	}
	x = jutil_msgnew(TMSG_GROUPCHAT, to, NULL, message);
	xmlnode_put_attrib(x, "from", from);
	jab_send(JConn->conn, x);
	xmlnode_free(x);
	return (0);
}

int JABBER_SendMessage(JABBER_Conn *JConn, char *handle, char *message)
{
	xmlnode x;

	if (!JConn) {
		eb_debug(DBG_JBR,
			"******Called with NULL JConn for user %s!!!\n",
			handle);
		return (0);
	}
	if (!strcmp(handle, "mailbox@gmail"))
		return 0;
	eb_debug(DBG_JBR, "%s -> %s:\nOUT.msg: %s\n", JConn->jid, handle,
		message);
	/* We always want to chat.  :) */
	x = jutil_msgnew(TMSG_CHAT, handle, NULL, message);
	jab_send(JConn->conn, x);
	xmlnode_free(x);
	return (0);
}

int JABBER_AddContact(JABBER_Conn *JConn, char *handle)
{
	xmlnode x, y, z;
	char *jid = strdup(handle), *ojid = jid;
	JABBER_Dialog *JD;
	char *buddy_server = NULL;
	char **server_list;
	char buffer[1024];

	eb_debug(DBG_JBR, ">\n");
	if (!JConn) {
		buddy_server = strchr(handle, '@');
		if (!buddy_server) {
			/* This happens for tranposrt buddies */
			buddy_server = strchr(handle, '.');
			if (!buddy_server) {
				eb_debug(DBG_JBR,
					"<Something weird, buddy without an '@' or a '.'");
				free(ojid);
				return (1);
			}
		}
		buddy_server++;
//              JConn=JCfindServer(buddy_server);
		if (JConn) {
			eb_debug(DBG_JBR, "Found matching server for %s\n",
				handle);
		} else {
			server_list = JCgetJIDList();
			if (!server_list) {
				eb_debug(DBG_JBR,
					"<No server list found, returning error\n");
				free(ojid);
				return (1);
			}
			JD = calloc(sizeof(JABBER_Dialog), 1);
			JD->heading = "Pick an account";
			sprintf(buffer,
				"Unable to automatically determine which account to use for %s:\n"
				"Please select the account that can talk to this buddy's server",
				handle);
			JD->message = strdup(buffer);
			JD->callback = j_on_pick_account;
			JD->requestor = strdup(handle);
			JABBERListDialog((const char **)server_list, JD);
			free(server_list);
			eb_debug(DBG_JBR, "<Creating dialog and leaving\n");
			free(ojid);
			return (0);
		}
	}
	/* For now, ayttm does not understand resources */
	jid = strtok(jid, "/");
	if (!jid)
		jid = ojid;
	eb_debug(DBG_JBR, "%s now %s\n", handle, jid);

	/* Ask to be subscribed */
	x = jutil_presnew(JPACKET__SUBSCRIBE, jid, NULL);
	jab_send(JConn->conn, x);
	xmlnode_free(x);
	/* Request roster update */
	x = jutil_iqnew(JPACKET__SET, NS_ROSTER);
	y = xmlnode_get_tag(x, "query");
	z = xmlnode_insert_tag(y, "item");
	xmlnode_put_attrib(z, "jid", jid);
	jab_send(JConn->conn, x);
	xmlnode_free(x);
	eb_debug(DBG_JBR, "<Added contact to %s and leaving\n", JConn->jid);
	free(ojid);
	return (0);
}

int JABBER_RemoveContact(JABBER_Conn *JConn, char *handle)
{
	xmlnode x, y, z;

	if (!JConn) {
		fprintf(stderr,
			"**********NULL JConn sent to JABBER_RemoveContact!\n");
		return (-1);
	}
	x = jutil_presnew(JPACKET__UNSUBSCRIBE, handle, NULL);

	jab_send(JConn->conn, x);
	xmlnode_free(x);

	x = jutil_iqnew(JPACKET__SET, NS_ROSTER);
	y = xmlnode_get_tag(x, "query");
	z = xmlnode_insert_tag(y, "item");
	xmlnode_put_attrib(z, "jid", handle);
	xmlnode_put_attrib(z, "subscription", "remove");
	jab_send(JConn->conn, x);
	xmlnode_free(x);
	return (0);
}

int JABBER_EndChat(JABBER_Conn *JConn, char *handle)
{
	eb_debug(DBG_JBR, "Empty function\n");
	return (0);
}

int JABBER_Logout(JABBER_Conn *JConn)
{
	eb_debug(DBG_JBR, "Entering\n");
	if (JConn) {
		if (JConn->conn) {
			eb_debug(DBG_JBR,
				"JConn->conn exists, closing everything up\n");
			j_remove_agents_from_host(JCgetServerName(JConn));
			ay_connection_input_remove(JConn->listenerID);
			jab_stop(JConn->conn);
			jab_delete(JConn->conn);
		}
		JABBERLogout(JConn);
		JConn->conn = NULL;
		JCremoveConn(JConn);
	}
	eb_debug(DBG_JBR, "Leaving\n");

	return (0);
}

int JABBER_ChangeState(JABBER_Conn *JConn, int state)
{
	xmlnode x, y;
	/* Unique away states are possible, but not supported by
	   ayttm yet.  status would hold that value */
	char show[7] = "", *status = "";

	eb_debug(DBG_JBR, "(%i)\n", state);
	if (!JConn->conn)
		return (-1);
	x = jutil_presnew(JPACKET__UNKNOWN, NULL, NULL);

	if (state != JABBER_ONLINE) {
		y = xmlnode_insert_tag(x, "show");
		switch (state) {
		case JABBER_AWAY:
			strcpy(show, "away");
			break;
		case JABBER_DND:
			strcpy(show, "dnd");
			break;
		case JABBER_XA:
			strcpy(show, "xa");
			break;
		case JABBER_CHAT:
			strcpy(show, "chat");
			break;
		default:
			strcpy(show, "unknown");
			eb_debug(DBG_JBR, "Unknown state: %i suggested\n",
				state);
		}
		xmlnode_insert_cdata(y, show, -1);
	}
	if (strlen(status) > 0) {
		y = xmlnode_insert_tag(x, "status");
		xmlnode_insert_cdata(y, status, -1);
	}
	eb_debug(DBG_JBR, "Setting status to: %s - %s\n", show, status);
	jab_send(JConn->conn, x);
	xmlnode_free(x);
	return (0);
}

Agent *j_find_agent_by_alias(char *alias)
{
	Agent *agent = NULL;
	GList *list = agent_list;
	for (list = agent_list; list; list = list->next) {
		agent = list->data;
		if (!strcmp(agent->alias, alias)) {
			eb_debug(DBG_JBR, "Found agent %s\n", agent->alias);
			break;
		}
	}
	return (agent);
}

Agent *j_find_agent_by_type(char *agent_type)
{
	Agent *agent = NULL;
	GList *list = agent_list;
	for (list = agent_list; list; list = list->next) {
		agent = list->data;
		if (!strcmp(agent->type, agent_type))
			break;
	}
	return (agent);
}

int JABBER_LeaveChatRoom(JABBER_Conn *JConn, char *room_name, char *nick)
{
	Agent *agent = NULL;
	xmlnode z;
	char buffer[256];

	agent = j_find_agent_by_type("groupchat");
	/* no private conference agent found */
	if (!agent) {
		eb_debug(DBG_JBR, "No groupchat agent found!\n");
		return (-1);
	}
	if (strstr(room_name, "@"))
		sprintf(buffer, "%s/%s", room_name, nick);
	else
		sprintf(buffer, "%s@%s/%s", room_name, agent->alias, nick);
	z = jutil_presnew(JPACKET__UNAVAILABLE, buffer, "Online");
	jab_send(JConn->conn, z);
	xmlnode_free(z);
	return (0);
}

static char nt_time[13] = "0";

void request_new_gmail(JABBER_Conn *JConn, char *id)
{
	gchar *request, *stime;

	if (!JConn->do_request_gmail)
		return;

	stime = (strcmp(nt_time, "0"))
		? g_strdup_printf(" newer-than-time='%s'", nt_time)
		: g_strdup("");

	request = g_strdup_printf("<iq type='get' "
		"from='%s' "
		"to='%s@%s' "
		"id='mail-request-%s'>"
		"<query xmlns='google:mail:notify'"
		"%s"
		"/></iq>",
		JConn->jid,
		JConn->conn->user->user, JConn->conn->user->server, id, stime);
	jab_send_raw(JConn->conn, request);
	g_free(request);
	g_free(stime);
}

void print_new_gmail(JABBER_Conn *JConn, xmlnode x)
{
	xmlnode y, z;
	char *subject, *snippet;
	JABBER_InstantMessage JIM;
	struct jabber_buddy JB;
	char *total_matched, *result_time;
	int new_mail;

	result_time = xmlnode_get_attrib(x, "result-time");
	total_matched = xmlnode_get_attrib(x, "total-matched");
	new_mail = strcmp(total_matched, "0");
	JB.description = total_matched;
	JB.jid = "mailbox@gmail";
	JB.status = new_mail ? JABBER_ONLINE : JABBER_AWAY;
	JB.JConn = JConn;
	JABBERStatusChange(&JB);

	if (!new_mail)
		return;

	for (y = xmlnode_get_tag(x, "mail-thread-info"); y;
		y = xmlnode_get_nextsibling(y)) {
		if (strcmp(nt_time, xmlnode_get_attrib(y, "date")) > 0)
			/* if nt_time > date of email, then I don't want to know about it */
			continue;
		z = xmlnode_get_tag(y, "subject");
		subject = xmlnode_get_data(z);
		z = xmlnode_get_tag(y, "snippet");
		snippet = xmlnode_get_data(z);
		JIM.msg =
			g_strconcat(_("You have new email: \n"), subject, "\n",
			snippet, NULL);
		JIM.JConn = JConn;
		JIM.sender = "mailbox@gmail";
		JABBERInstantMessage(&JIM);
		g_free(JIM.msg);
	}
	eb_debug(DBG_JBR, "old %s, new %s\n", nt_time, result_time);
	strncpy(nt_time, result_time, 13);
}

void JABBER_Send_typing(JABBER_Conn *JConn, const char *from, const char *to,
	int typing)
{
	/*
	 * Obsolete, has to be rewritten to conform to XEP-0085
	 * http://xmpp.org/extensions/xep-0085.html

	 char buffer[4096];
	 if (!JConn || !JConn->conn)
	 return;

	 if (typing) {
	 snprintf(buffer, 4095,
	 "<message "
	 "from='%s' "
	 "to='%s'>"
	 "<x xmlns='jabber:x:event'>"
	 "<composing/>"
	 "<id>typ%s</id>"
	 "</x>"
	 "</message>",
	 from, to, from);

	 } else {
	 snprintf(buffer, 4095,
	 "<message "
	 "from='%s' "
	 "to='%s'>"
	 "<x xmlns='jabber:x:event'>"
	 "<id>typ%s</id>"
	 "</x>"
	 "</message>",
	 from,to,from);
	 }
	 eb_debug(DBG_JBR, "sending %s\n", buffer);
	 jab_send_raw(JConn->conn, buffer); */
}

int JABBER_IsChatRoom(char *from)
{
	char buffer[257];
	char *ptr = NULL;
	Agent *agent = NULL;

	if (!from)
		return 0;

	strncpy(buffer, from, 256);
	strtok(buffer, "/");
	ptr = strchr(buffer, '@');
	if (ptr)
		ptr++;
	else
		ptr = buffer;
	eb_debug(DBG_JBR, "Looking for %s\n", ptr);
	agent = j_find_agent_by_alias(ptr);
	if (agent && (!strcmp(agent->type, "groupchat"))) {
		eb_debug(DBG_JBR, "Returning True\n");
		return (1);
	} else if (find_chat_room_by_id(ptr) != NULL) {
		eb_debug(DBG_JBR, "Returning True\n");
		return (1);
	}

	/* test with @ */
	strncpy(buffer, from, 256);
	if (strchr(buffer, '/'))
		*strchr(buffer, '/') = 0;

	eb_debug(DBG_JBR, "looking for %s\n", buffer);
	agent = j_find_agent_by_alias(buffer);
	if (agent && (!strcmp(agent->type, "groupchat"))) {
		eb_debug(DBG_JBR, "Returning True\n");
		return (1);
	} else if (find_chat_room_by_id(buffer) != NULL) {
		eb_debug(DBG_JBR, "Returning True\n");
		return (1);
	}

	eb_debug(DBG_JBR, "Returning False\n");
	return (0);

}

int JABBER_JoinChatRoom(JABBER_Conn *JConn, char *room_name, char *nick)
{
	Agent *agent = NULL;
	xmlnode z;
	char buffer[256];

	eb_debug(DBG_JBR, ">\n");
	agent = j_find_agent_by_type("groupchat");
	/* no private conference agent found */
	if (!agent) {
		eb_debug(DBG_JBR, "No groupchat agent found!\n");
		return (-1);
	}
	eb_debug(DBG_JBR, "private conference agent found: %s\n", agent->alias);
	/* send a time packet to make sure server is there
	   z = jutil_iqnew (jpacket__GET, NS_TIME);
	   xmlnode_put_attrib(z, "id", "Time");
	   xmlnode_put_attrib(z, "to", agent->alias);
	   jab_send (JConn->conn, z);
	   xmlnode_free(z);
	 */

	if (!strstr(room_name, "@"))
		sprintf(buffer, "%s@%s/%s", room_name, agent->alias, nick);
	else
		sprintf(buffer, "%s/%s", room_name, nick);

	z = jutil_presnew(JPACKET__GROUPCHAT, buffer, "Online");
	xmlnode_put_attrib(z, "id", "GroupChat");
	jab_send(JConn->conn, z);
	xmlnode_free(z);
	eb_debug(DBG_JBR, "<\n");
	return (-1);
}

void j_list_agents()
{
	Agent *agent = NULL;
	GList *list = agent_list;

	for (list = agent_list; list; list = list->next) {
		agent = list->data;
		fprintf(stderr, "Agent: %s - %s %s %s %s\n",
			agent->host, agent->name, agent->alias, agent->desc,
			agent->service);
	}
}

void j_remove_agents_from_host(char *host)
{
	Agent *agent = NULL;
	GList *list = agent_list;

	eb_debug(DBG_JBR, "Removing host: %s\n", host);
	while (list) {
		agent = list->data;
		/* Move on before the data disappears */
		list = list->next;
		if (!strcmp(agent->host, host)) {
			eb_debug(DBG_JBR, "Removing %s\n", agent->alias);
			agent_list = g_list_remove(agent_list, agent);
			g_free(agent);
		}
	}
}

void j_add_agent(char *name, char *alias, char *desc, char *service, char *host,
	char *agent_type)
{
	Agent *agent = g_new0(Agent, 1);

	eb_debug(DBG_JBR, "Agent %s[%s]: %s [%s] [%s]\n",
		host, name, alias, desc, service);
	if (!host) {
		g_warning("No host specified for service, not registering!");
		g_free(agent);
		return;
	}
	strncpy(agent->host, host, 256);
	if (agent_type)
		strncpy(agent->type, agent_type, 256);
	if (name)
		strncpy(agent->name, name, 256);
	if (alias)
		strncpy(agent->alias, alias, 256);
	if (desc)
		strncpy(agent->desc, desc, 256);
	if (service)
		strncpy(agent->service, service, 256);

	agent_list = g_list_append(agent_list, agent);
	return;
}

/* ----------------------------------------------*/

/* Functions to handle jabber responses */

void j_on_packet_handler(jconn conn, jpacket packet)
{
	xmlnode x, y;
	char *id, *ns, *alias, *sub, *name, *from, *group;
	char *desc, *to;
	char *show, *type, *body, *subj, *ask, *code;
	char *room, *user;
	char buff[8192 + 1];	/* FIXME: Find right size */
	int status;
	jid jabber_id = NULL;
	JABBER_Conn *JConn = NULL;
	JABBER_InstantMessage JIM;
	JABBER_Dialog *JD;
	struct jabber_buddy JB;

	/* This should not return NULL, but might */
	eb_debug(DBG_JBR, ">Packet to: %s\n", conn->user->user);
	JConn = JCfindConn(conn);
	if (!JConn) {
		sprintf(buff,
			"%s@%s/%s - connection error, can't find connection, packets are being dropped!",
			conn->user->user, conn->user->server,
			conn->user->resource);
		ay_do_error(_("Jabber Error"), buff);
		eb_debug(DBG_JBR, "<Returning error - can't find connection\n");
		return;
	}
	jpacket_reset(packet);
	type = xmlnode_get_attrib(packet->x, "type");
	from = xmlnode_get_attrib(packet->x, "from");
	eb_debug(DBG_JBR, "Packet: %s. Type: %s. From: %s\n", packet->x->name,
		type, from);
	switch (packet->type) {
	case JPACKET_MESSAGE:
		x = xmlnode_get_tag(packet->x, "body");
		body = xmlnode_get_data(x);
		x = xmlnode_get_tag(packet->x, "x");
		if (x) {
			type = xmlnode_get_attrib(x, "xmlns");
			eb_debug(DBG_JBR, "type: %s\n", type);
			if (!strcmp(type, "jabber:x:event")) {
				xmlnode comp = xmlnode_get_tag(x, "composing");
				char *tfrom = strdup(from);
				JABBERBuddy_typing(JConn, tfrom, comp ? 1 : 0);
				free(tfrom);
			}
		}
		x = xmlnode_get_tag(packet->x, "subject");
		if (x) {
			subj = xmlnode_get_data(x);
			snprintf(buff, 8192, "%s: %s", subj, body);
		} else {
			subj = NULL;
			if (body)
				strncpy(buff, body, 8192);
			else
				buff[0] = '\0';
		}
		if (type && !strcmp(type, "groupchat")) {
			user = strchr(from, '/');
			room = from;
			if (!user)
				user = room;
			else {
				*user = 0;
				user++;
			}
			eb_debug(DBG_JBR, "chatroom: %s %s %s\n", room, user,
				buff);
			JABBERChatRoomMessage(room, user, buff);
		} else {
			/* For now, ayttm does not understand resources */
			JIM.msg = buff;
			JIM.JConn = JConn;
			JIM.sender = strtok(from, "/");
			if (!JIM.sender)
				JIM.sender = from;
			eb_debug(DBG_JBR, "JIM.sender: %s\nJIM.msg: %s\n",
				JIM.sender, JIM.msg);
			JABBERInstantMessage(&JIM);
		}
		break;
	case JPACKET_IQ:
		if (!type) {
			eb_debug(DBG_JBR, "<JPACKET_IQ: NULL type\n");
			return;
		}
		if (!(id = xmlnode_get_attrib(packet->x, "id"))) {
			eb_debug(DBG_JBR, "<JPACKET_IQ: NULL id\n");
			return;
		}
		if (!strcmp(type, "result")) {
			if (!(x = xmlnode_get_firstchild(packet->x))) {
				/* If there is no child, <id> is the only way to recognize IQ */
				if (!strcmp(id, "id_auth")) {
					eb_debug(DBG_JBR,
						"Authorization successful, announcing presence\n");
					jab_send_raw(conn, "<presence/>");

					x = jutil_iqnew(JPACKET__GET,
						NS_ROSTER);
					xmlnode_put_attrib(x, "id", "Roster");
					eb_debug(DBG_JBR,
						"Requesting roster\n");
					jab_send(conn, x);
					xmlnode_free(x);

					x = jutil_iqnew(JPACKET__GET,
						NS_DISCOINFO);
					xmlnode_put_attrib(x, "to",
						conn->user->server);
					xmlnode_put_attrib(x, "id", "disco");
					eb_debug(DBG_JBR,
						"<Sending discovery request to %s\n",
						conn->user->server);
					jab_send(conn, x);
					xmlnode_free(x);
					return;
				} else if (!strcmp(id, "id_reg")) {
					JConn->reg_flag = 0;
					eb_debug(DBG_JBR,
						"<Performing authorization\n");
					jab_auth(conn);
					return;
				}
				break;
			}
			ns = xmlnode_get_attrib(x, "xmlns");
			eb_debug(DBG_JBR, "ns: %s\n", ns);
			if (!strcmp(ns, NS_ROSTER)) {
				for (y = xmlnode_get_tag(x, "item"); y;
					y = xmlnode_get_nextsibling(y)) {
					alias = xmlnode_get_attrib(y, "jid");
					sub = xmlnode_get_attrib(y,
						"subscription");
					name = xmlnode_get_attrib(y, "name");
					group = xmlnode_get_attrib(y, "group");
					eb_debug(DBG_JBR,
						"Buddy: %s(%s) %s %s\n", alias,
						group, sub, name);
					if (alias && sub && name) {
						JB.jid = strtok(alias, "/");
						JB.jid = strdup(JB.jid ? JB.
							jid : alias);
						JB.name = strdup(name);
						JB.sub = strdup(sub);
						/* State does not matter */
						JB.status = JABBER_OFFLINE;
						JB.JConn = JConn;
						JABBERAddBuddy(&JB);
						free(JB.name);
						free(JB.sub);
						free(JB.jid);
					}
				}
			} else if (!strcmp(ns, NS_DISCOINFO)) {
				for (y = xmlnode_get_tag(x, "feature"); y;
					y = xmlnode_get_nextsibling(y)) {
					name = xmlnode_get_attrib(y, "var");
					eb_debug(DBG_JBR, "Feature: %s\n",
						name);
					if (!strcmp(name, "google:mail:notify"))
						JConn->server_features |=
							F_GMAIL_NOTIFY;
					/* More to come? */
				}
				if ((JConn->server_features & F_GMAIL_NOTIFY)
					&& JConn->do_request_gmail) {
					JB.jid = strdup("mailbox@gmail");
					JB.name = strdup("GMailbox");
					JB.sub = strdup("both");
					JB.status = JABBER_AWAY;
					JB.JConn = JConn;
					JABBERAddBuddy(&JB);
					free(JB.name);
					free(JB.sub);
					free(JB.jid);
					request_new_gmail(JConn, "0");
				}
			} else if (!strcmp(ns, "google:mail:notify"))
				if ((x = xmlnode_get_tag(packet->x, "mailbox")))
					print_new_gmail(JConn, x);

			eb_debug(DBG_JBR, "<\n");
			return;
		} else if (!strcmp(type, "set")) {
			if (!(x = xmlnode_get_firstchild(packet->x)))
				return;

			ns = xmlnode_get_attrib(x, "xmlns");
			if (!strcmp(ns, NS_ROSTER)) {
				y = xmlnode_get_tag(x, "item");
				alias = xmlnode_get_attrib(y, "jid");
				sub = xmlnode_get_attrib(y, "subscription");
				name = xmlnode_get_attrib(y, "name");
				ask = xmlnode_get_attrib(y, "ask");
				eb_debug(DBG_JBR,
					"Need to find a buddy: %s %s %s %s\n",
					alias, sub, name, ask);
				if (sub && !strcmp(sub, "remove"))
					eb_debug(DBG_JBR,
						"Need to remove a buddy: %s\n",
						alias);
			} else if (!strcmp(ns, "google:mail:notify")) {
				gchar *success;

				success = g_strdup_printf("<iq type='result' "
					"from='%s' "
					"to='%s@%s' "
					"id='%s' />",
					JConn->jid,
					JConn->conn->user->user,
					JConn->conn->user->server, id);
				jab_send_raw(JConn->conn, success);
				g_free(success);
				request_new_gmail(JConn, id);
			}
		} else if (!strcmp(type, "error")) {
			x = xmlnode_get_tag(packet->x, "error");
			from = xmlnode_get_attrib(packet->x, "from");
			to = xmlnode_get_attrib(packet->x, "to");
			code = xmlnode_get_attrib(x, "code");
			name = xmlnode_get_attrib(x, "id");
			desc = xmlnode_get_tag_data(packet->x, "error");
			eb_debug(DBG_JBR,
				"Received error: [%i]%s from %s[%s], sent to %s\n",
				atoi(code), desc, from, name, to);

			switch (atoi(code)) {
			case 401:	/* Unauthorized */
				if (JConn->reg_flag)
					break;
				//FIXME: We need to change our status to offline first
				JD = calloc(sizeof(JABBER_Dialog), 1);
				name = xmlnode_get_tag_data(packet->iq,
					"username");
				jabber_id = jab_getjid(conn);
				JD->heading =
					_("Register? You're not authorized");
				sprintf(buff,
					_
					("Do you want to try to create the account \"%s\" on the jabber server %s?"),
					jabber_id->user, jabber_id->server);
				JD->message = strdup(buff);
				JD->callback = j_on_create_account;
				JD->JConn = JConn;
				JABBERDialog(JD);
				break;
			case 302:	/* Redirect */
			case 400:	/* Bad request */
			case 402:	/* Payment Required */
			case 403:	/* Forbidden */
			case 404:	/* Not Found */
			case 405:	/* Not Allowed */
			case 406:	/* Not Acceptable */
			case 407:	/* Registration Required */
			case 408:	/* Request Timeout */
			case 409:	/* Conflict */
			case 500:	/* Internal Server Error */
			case 501:	/* Not Implemented */
			case 502:	/* Remote Server Error */
			case 503:	/* Service Unavailable */
			case 504:	/* Remote Server Timeout */
			default:
				sprintf(buff, "%s@%s/%s - %s (%s)",
					conn->user->user, conn->user->server,
					conn->user->resource, desc, code);
				ay_do_error(_("Jabber Error"), buff);
				fprintf(stderr, "Error: %s\n", code);
			}
		}
		break;
	case JPACKET_PRESENCE:
		status = JABBER_ONLINE;
		/* For now, ayttm does not understand resources */
		if (type && !strcmp(type, "error"))
			break;
		if ((x = xmlnode_get_tag(packet->x, "show"))) {
			show = xmlnode_get_data(x);
			if (show) {
				if (!strcmp(show, "away"))
					status = JABBER_AWAY;
				else if (!strcmp(show, "dnd"))
					status = JABBER_DND;
				else if (!strcmp(show, "xa"))
					status = JABBER_XA;
				else if (!strcmp(show, "chat"))
					status = JABBER_CHAT;
			} else {
				eb_debug(DBG_JBR,
					"Presence packet received from %s w/o a status string, is this a conference room?\n",
					from);
				break;
			}
		}
		if (type && !strcmp(type, "unavailable"))
			status = JABBER_OFFLINE;
		if (!strncmp(packet->from->server, "conference.", 11)) {
			eb_debug(DBG_JBR,
				"Presence received from a conference room\n");
			user = strchr(from, '/');
			room = from;
			if (!user) {
				user = room;
				user += strlen(room) + 1;
			} else {
				*user = 0;
				user++;
			}
			JABBERChatRoomBuddyStatus(room, user, status);
		} else {
			if ((x = xmlnode_get_tag(packet->x, "status")))
				JB.description = xmlnode_get_data(x);
			else
				JB.description = NULL;
			if (!(JB.jid = strtok(from, "/")))
				JB.jid = from;
			JB.status = status;
			JB.JConn = JConn;
			JABBERStatusChange(&JB);
		}
		break;
	case JPACKET_S10N:
		if (type) {
			JD = calloc(1, sizeof(JABBER_Dialog));
			if (!strcmp(type, "subscribe")) {
				sprintf(buff,
					_
					("%s wants to add you to his friends' list.\n\nDo you want to accept?"),
					from);
				JD->message = strdup(buff);
				JD->heading = "Subscribe Request";
				JD->callback = j_allow_subscribe;
				JD->requestor = strdup(from);
				JD->JConn = JConn;
				JABBERDialog(JD);
				eb_debug(DBG_JBR,
					"<Initiated dialog to allow or deny subscribe request\n");
				return;
			} else if (!strcmp(type, "unsubscribe"))
				return;
		}
		break;
	case JPACKET_UNKNOWN:
	default:
		eb_debug(DBG_JBR, "unrecognized packet: %i received from %s\n",
			packet->type, from)
	}
	eb_debug(DBG_JBR, "<\n");
}

void ext_jabber_connect_error(void *data, int error, jconn j)
{
	JABBER_Conn *JConn = JCfindConn(j);

	if (error == AY_CANCEL_CONNECT) {
		ay_connection_input_remove(JConn->listenerID);
		JABBERLogout(JConn);
		j_remove_agents_from_host(JCgetServerName(JConn));
		JConn->conn = NULL;
	} else {
		j_on_state_handler(j, JCONN_STATE_OFF);
	}
}

void j_on_state_handler(jconn conn, int state)
{
	int previous_state;
	JABBER_Conn *JConn = NULL;
	char buff[4096];

	JConn = JCfindConn(conn);
	previous_state = JConn->state;

	eb_debug(DBG_JBR, "Entering: new state: %i previous_state: %i\n", state,
		previous_state);

	switch (state) {
	case JCONN_STATE_OFF:
		if (previous_state != JCONN_STATE_OFF) {
			eb_debug(DBG_JBR,
				"The Jabber server has disconnected you: %i\n",
				previous_state);
			snprintf(buff, 4096,
				_("The Jabber server %s has disconnected you."),
				JCgetServerName(JConn));
			JABBERError(buff, _("Disconnect"));
			JABBERLogout(JConn);
			ay_connection_input_remove(JConn->listenerID);
			/* FIXME: Do we need to free anything? */
			j_remove_agents_from_host(JCgetServerName(JConn));
		} else if (!JConn->conn
			|| JConn->conn->state == JCONN_STATE_OFF) {
			snprintf(buff, 4096,
				_("Connection to the jabber server %s failed!"),
				conn->user->server);
			JABBERError(buff, _("Jabber server not responding"));
			JABBERLogout(JConn);
			jab_delete(JConn->conn);
		}
		break;
	case JCONN_STATE_CONNECTED:
		eb_debug(DBG_JBR, "JCONN_STATE_CONNECTED\n");
		break;
	case JCONN_STATE_AUTH:
		eb_debug(DBG_JBR, "JCONN_STATE_AUTH\n");
		break;
	case JCONN_STATE_ON:
		eb_debug(DBG_JBR, "JCONN_STATE_ON\n");
		if (previous_state == JCONN_STATE_CONNECTED) {
			jab_auth(JConn->conn);
			JConn->listenerID =
				ay_connection_input_add(JConn->connection,
				EB_INPUT_READ, jabber_callback_handler, JConn);
			eb_debug(DBG_JBR, "*** ListenerID: %i\n",
				JConn->listenerID);
			JABBERConnected(JConn);
		}
		break;
	default:
		eb_debug(DBG_JBR, "UNKNOWN state: %i\n", state);
		break;
	}
	JConn->state = state;
	eb_debug(DBG_JBR, "Leaving\n");
}

void j_on_create_account(void *data)
{
	JABBER_Dialog_PTR jd;

	eb_debug(DBG_JBR, "Entering, but doing little\n");
	jd = (JABBER_Dialog_PTR) data;
	jd->JConn->reg_flag = 1;
	jab_reg(jd->JConn->conn);
	eb_debug(DBG_JBR, "Leaving\n");
	jd->JConn->conn->sid = NULL;
}

void j_allow_subscribe(void *data)
{
	JABBER_Dialog_PTR jd;
	xmlnode x, y, z;

	jd = (JABBER_Dialog_PTR) data;
	eb_debug(DBG_JBR, "%s: Entering\n", jd->requestor);
	x = jutil_presnew(JPACKET__SUBSCRIBED, jd->requestor, NULL);
	jab_send(jd->JConn->conn, x);
	xmlnode_free(x);

	/* We want to request their subscription now */
	x = jutil_presnew(JPACKET__SUBSCRIBE, jd->requestor, NULL);
	jab_send(jd->JConn->conn, x);
	xmlnode_free(x);

	/* Request roster update */
	x = jutil_iqnew(JPACKET__SET, NS_ROSTER);
	y = xmlnode_get_tag(x, "query");
	z = xmlnode_insert_tag(y, "item");
	xmlnode_put_attrib(z, "jid", jd->requestor);
	xmlnode_put_attrib(z, "name", jd->requestor);
	jab_send(jd->JConn->conn, x);
	xmlnode_free(x);
	eb_debug(DBG_JBR, "Leaving\n");
}

void j_unsubscribe(void *data)
{
	JABBER_Dialog_PTR jd;

	jd = (JABBER_Dialog_PTR) data;
	JABBERDelBuddy(jd->JConn, jd->requestor);
}

void j_on_pick_account(void *data)
{
	JABBER_Dialog_PTR jd;
	JABBER_Conn *JConn = NULL;

	jd = (JABBER_Dialog_PTR) data;
	JConn = JCfindJID(jd->response);
	eb_debug(DBG_JBR, "Found JConn for %s: %p\n", jd->requestor, JConn);
	/* Call it again, this time with a good JConn */
	if (!JConn) {
		fprintf(stderr,
			"NULL Jabber connector for buddy, should not happen!\n");
		return;
	}
	JABBER_AddContact(JConn, jd->requestor);
}
