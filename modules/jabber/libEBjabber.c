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

#include "plugin_api.h"
#include "libEBjabber.h"
#include "messages.h"


#ifdef __MINGW32__
#include <glib.h>
#define snprintf _snprintf
#endif


extern void JABBERStatusChange(void *data);
extern void JABBERAddBuddy(void *data);
extern void JABBERInstantMessage(void *data);
extern void JABBERDialog(void *data);
extern void JABBERListDialog(const char **list, void *data);
extern void	JABBERError( char *message, char *title );
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

JABBER_Conn *Connections=NULL;
GList *agent_list=NULL;

JABBER_Conn *JCnewConn(void)
{
	JABBER_Conn *jnew=NULL;

	/* Create a new connection struct, and put it on the top of the list */
	jnew=calloc(1, sizeof(JABBER_Conn));
	jnew->next=Connections;
	eb_debug(DBG_JBR, "JCnewConn: %p\n", jnew);
	Connections=jnew;
	return(Connections);
}

JABBER_Conn *JCfindConn(jconn conn)
{
	JABBER_Conn *current=Connections;

	/*FIXME: Somehow current==current->next, and we get an endless loop */
	while(current)
	{
		eb_debug(DBG_JBR, "conn=%p current=%p\n", conn, current);
		if(conn == current->conn)
			return(current);
		/*FIXME: This is not the right place to fix this, why is it happening? */
		if(current==current->next) {
			current->next=NULL;
			fprintf(stderr, "Endless jabber connection loop broken\n");
		}
		current=current->next;
	}
	return(NULL);
}

char *JCgetServerName(JABBER_Conn * JConn)
{
	if(!JConn || !JConn->conn || !JConn->conn->user)
		return(NULL);
	return(JConn->conn->user->server);
}

JABBER_Conn *JCfindServer(char *server)
{
	JABBER_Conn *current=Connections;

	while(current) {
		if(current->conn) {
			eb_debug(DBG_JBR, "Server: %s\n", current->conn->user->server);
			if(!strcmp(server, current->conn->user->server)) {
				return(current);
			}
		}
		current=current->next;
	}
	return(NULL);
}

JABBER_Conn *JCfindJID(char *JID)
{
	JABBER_Conn *current=Connections;

	while(current) {
		eb_debug(DBG_JBR, "JID: %s\n", current->jid);
		if(!strcmp(JID, current->jid)) {
			return(current);
		}
		current=current->next;
	}
	return(NULL);
}

int JCremoveConn(JABBER_Conn *JConn)
{
	JABBER_Conn *current=Connections, *last=current;

	while(current)
	{
		if(JConn==current)
		{
			last->next=current->next;
			free(current);
			return(0);
		}
		last=current;
		current=current->next;
	}
	return(-1);
}

char **JCgetJIDList(void)
{
	JABBER_Conn *current=Connections;
	char **list=NULL;
	int count=0;

	while(current)
	{
		/* Leave room for the last null entry */
		list=realloc(list, sizeof(char *)*(count+2));
		eb_debug(DBG_JBR, "current->jid[%p]\n", current->jid);
		list[count]=current->jid;
		current=current->next;
		count++;
	}
	if(list)
		list[count]=NULL;
	return(list);
}

void JCfreeServerList(char **list)
{
	int count=0;
	while(list[count])
	{
		free(list[count++]);
	}
	free(list);
}

/****************************************
 * End jabber multiple connection routines
 ****************************************/

void jabber_callback_handler(void *data, int source, eb_input_condition cond)
{
    JABBER_Conn *JConn=data;
    /* Let libjabber do the work, it calls what we setup in jab_packet_handler */
    jab_poll(JConn->conn, 0);
    /* Is this connection still good? */
    if(!JConn->conn) {
	eb_debug(DBG_JBR, "Logging out because JConn->conn is NULL\n");
        JABBERLogout(NULL);
        eb_input_remove(JConn->listenerID);
    }
    else if(JConn->conn->state==JCONN_STATE_OFF || JConn->conn->fd==-1) {
        /* No, clean up */
        JABBERLogout(NULL);
        eb_input_remove(JConn->listenerID);
        jab_delete(JConn->conn);
        JConn->conn=NULL;
    }
}

/* Functions called from the ayttm jabber.c file */

int JABBER_Login(char *handle, char *passwd, char *host, int use_ssl, int port) {
	/* At this point, we don't care about host and port */
	char jid[256+1];
	int tag;
	char buff[4096];
	char server[256], *ptr=NULL;
	JABBER_Conn *JConn=NULL;

	eb_debug(DBG_JBR,  "%s %s %i\n", handle, host, port);
	/* Make sure the name is correct, support all formats
	 * handle	become handle@host/ayttm
	 * handle@foo	becomes handle@foo/ayttm
	 * handle@foo/bar is not changed
	 */

	if(!strchr(handle, '@')) {
		/* A host must be specified in the config file */
		if(!host) {
			JABBERError( _("No jabber server specified."), _("Cannot login") );
			return(0);
		}
		snprintf(jid, 256, "%s@%s/ayttm", handle, host);
	}
	else if(!strchr(handle, '/'))
		snprintf(jid, 256, "%s/ayttm", handle);
	else
		strncpy(jid, handle, 256);

	/* Extract the server name */
	strcpy(server, jid);
	ptr=strchr(server, '@');
	ptr++;
	host=strtok(ptr, "@/");

	eb_debug(DBG_JBR,  "jid: %s\n", jid);
	JConn=JCnewConn();
	strncpy(JConn->jid,jid, LINE_LENGTH);
	/* We assume we have an account, and don't need to register one */
	JConn->reg_flag = 0;
	JConn->conn=jab_new(jid, passwd);
	if(!JConn->conn) {
		snprintf(buff, 4096, "Connection to server '%s' failed.", host);
		JABBERError(buff, _("Jabber Error"));
		JABBERNotConnected(NULL);
		free(JConn);
		return(0);
	}
	else if ( JConn->conn->user == NULL )
	{
		snprintf( buff, 4096, "Error connecting to server '%s':\n   Invalid user name.", host );
		JABBERError( buff, _("Jabber Error") );
		JABBERNotConnected( NULL );
		free( JConn );
		return( 0 );
	}
	
	jab_packet_handler(JConn->conn, j_on_packet_handler);
	jab_state_handler(JConn->conn, j_on_state_handler);
	tag = jab_start(JConn->conn, port, use_ssl);

	return tag;
}

int JABBER_AuthorizeContact(JABBER_Conn *conn, char *handle) {
	eb_debug(DBG_JBR,  "%s\n", handle);
	return(0);
}

int JABBER_SendChatRoomMessage(JABBER_Conn *JConn, char *room_name, char *message, char *nick)
{
	char from[256], to[256];
	xmlnode x;
	Agent *agent=j_find_agent_by_type("groupchat");
	if(!JConn) {
		eb_debug(DBG_JBR, "******Called with NULL JConn for room %s!!!\n", room_name);
		return(0);
	}

	if(!agent)
	{
		eb_debug(DBG_JBR, "Could not find private group chat agent to send message\n");
		return(-1);
	}

	sprintf(to, "%s@%s", room_name, agent->alias);
	sprintf(from, "%s@%s/%s", room_name, agent->alias, nick);
	x = jutil_msgnew(TMSG_GROUPCHAT, to, NULL, message);
	xmlnode_put_attrib (x, "from", from);
	jab_send(JConn->conn, x);
	xmlnode_free(x);
	return(0);
}

int JABBER_SendMessage(JABBER_Conn *JConn, char *handle, char *message) {
	xmlnode x;

	if(!JConn) {
		eb_debug(DBG_JBR, "******Called with NULL JConn for user %s!!!\n", handle);
		return(0);
	}
	eb_debug(DBG_JBR,  "handle: %s message: %s\n", handle, message);
	eb_debug(DBG_JBR,  "********* %s -> %s\n", JConn->jid, handle);
	/* We always want to chat.  :) */
	x = jutil_msgnew(TMSG_CHAT, handle, NULL, message);
	jab_send(JConn->conn, x);
	xmlnode_free(x);
	return(0);
}

int JABBER_AddContact(JABBER_Conn *JConn, char *handle) {
	xmlnode x,y,z;
	char *jid=strdup(handle), *ojid=jid;
	JABBER_Dialog *JD;
	char *buddy_server=NULL;
	char **server_list;
	char buffer[1024];

	eb_debug(DBG_JBR, ">\n");
	if(!JConn) {
		buddy_server=strchr(handle, '@');
		if(!buddy_server) {
			/* This happens for tranposrt buddies */
			buddy_server=strchr(handle, '.');
			if(!buddy_server) {
				eb_debug(DBG_JBR, "<Something weird, buddy without an '@' or a '.'");
				free(ojid);
				return(1);
			}
		}
		buddy_server++;
//		JConn=JCfindServer(buddy_server);
		if(JConn) {
			eb_debug(DBG_JBR, "Found matching server for %s\n", handle);
		}
		else {
			server_list=JCgetJIDList();
			if(!server_list) {
				eb_debug(DBG_JBR, "<No server list found, returning error\n");
				free(ojid);
				return(1);
			}
			JD=calloc(sizeof(JABBER_Dialog), 1);
			JD->heading="Pick an account";
			sprintf(buffer, "Unable to automatically determine which account to use for %s:\n"
				"Please select the account that can talk to this buddy's server", handle);
			JD->message=strdup(buffer);
			JD->callback=j_on_pick_account;
			JD->requestor=strdup(handle);
			JABBERListDialog((const char **)server_list, JD);
			free(server_list);
			eb_debug(DBG_JBR, "<Creating dialog and leaving\n");
			free(ojid);
			return(0);
		}
	}
	/* For now, ayttm does not understand resources */
	jid=strtok(jid, "/");
	if(!jid)
		jid=ojid;
	eb_debug(DBG_JBR,  "%s now %s\n", handle, jid);

	/* Ask to be subscribed */
	x=jutil_presnew(JPACKET__SUBSCRIBE, jid, NULL);
	jab_send(JConn->conn, x);
	xmlnode_free(x);
	/* Request roster update */
	x = jutil_iqnew (JPACKET__SET, NS_ROSTER);
	y = xmlnode_get_tag (x, "query");
	z = xmlnode_insert_tag (y, "item");
	xmlnode_put_attrib (z, "jid", jid);
	jab_send (JConn->conn, x);
	xmlnode_free(x);
	eb_debug(DBG_JBR, "<Added contact to %s and leaving\n", JConn->jid);
	free(ojid);
	return(0);
}

int JABBER_RemoveContact(JABBER_Conn *JConn, char *handle) {
	xmlnode x,y,z;

	if(!JConn) {
		fprintf(stderr, "**********NULL JConn sent to JABBER_RemoveContact!\n");
		return(-1);
	}
	x = jutil_presnew (JPACKET__UNSUBSCRIBE, handle, NULL);

	jab_send (JConn->conn, x);
	xmlnode_free(x);

	x = jutil_iqnew (JPACKET__SET, NS_ROSTER);
	y = xmlnode_get_tag (x, "query");
	z = xmlnode_insert_tag (y, "item");
	xmlnode_put_attrib (z, "jid", handle);
	xmlnode_put_attrib (z, "subscription", "remove");
	jab_send (JConn->conn, x);
	xmlnode_free(x);
	return(0);
}

int JABBER_EndChat(JABBER_Conn *JConn, char *handle) {
	eb_debug(DBG_JBR,  "Empty function\n");
	return(0);
}


int JABBER_Logout(JABBER_Conn *JConn) {
	eb_debug(DBG_JBR,  "Entering\n");
	if(JConn) {
		if(JConn->conn) {
			eb_debug(DBG_JBR,  "JConn->conn exists, closing everything up\n");
			j_remove_agents_from_host(JCgetServerName(JConn));
			eb_input_remove(JConn->listenerID);
			jab_stop(JConn->conn);
			jab_delete(JConn->conn);
		}
		JConn->conn=NULL;
		JCremoveConn(JConn);
	}
	eb_debug(DBG_JBR,  "Leaving\n");
	JABBERLogout(NULL);
	return(0);
}

int JABBER_ChangeState(JABBER_Conn *JConn, int state) {
	xmlnode x,y;
	/* Unique away states are possible, but not supported by
	ayttm yet.  status would hold that value */
	char show[7]="", *status="";

	eb_debug(DBG_JBR, "(%i)\n", state);
	if(!JConn->conn)
		return(-1);
	x = jutil_presnew (JPACKET__UNKNOWN, NULL, NULL);

	if (state != JABBER_ONLINE) {
		y = xmlnode_insert_tag (x, "show");
		switch (state) {
		case JABBER_AWAY:
			strcpy (show, "away");
			break;
		case JABBER_DND:
			strcpy (show, "dnd");
			break;
		case JABBER_XA:
			strcpy (show, "xa");
			break;
		case JABBER_CHAT:
			strcpy (show, "chat");
			break;
		default:
			strcpy (show, "unknown");
			eb_debug(DBG_JBR,  "Unknown state: %i suggested\n", state);
		}
		xmlnode_insert_cdata (y, show, -1);
	}
	if (strlen (status) > 0) {
		y = xmlnode_insert_tag (x, "status");
		xmlnode_insert_cdata (y, status, -1);
	}
	eb_debug(DBG_JBR,  "Setting status to: %s - %s\n", show, status);
	jab_send (JConn->conn, x);
	xmlnode_free(x);
	return(0);
}

Agent *j_find_agent_by_alias(char *alias)
{
	Agent *agent=NULL;
	GList *list=agent_list;
	for(list=agent_list; list; list=list->next)
	{
		agent=list->data;
		if(!strcmp(agent->alias, alias)) {
			eb_debug(DBG_JBR, "Found agent %s\n", agent->alias);
			break;
		}
	}
	return(agent);
}
Agent *j_find_agent_by_type(char *agent_type)
{
	Agent *agent=NULL;
	GList *list=agent_list;
	for(list=agent_list; list; list=list->next)
	{
		agent=list->data;
		if(!strcmp(agent->type, agent_type))
			break;
	}
	return(agent);
}

int JABBER_LeaveChatRoom(JABBER_Conn *JConn, char *room_name, char *nick)
{
	Agent *agent=NULL;
	xmlnode z;
	char buffer[256];

	agent = j_find_agent_by_type("groupchat");
	/* no private conference agent found */
	if(!agent)
	{
		eb_debug(DBG_JBR, "No groupchat agent found!\n");
		return(-1);
	}
	sprintf(buffer, "%s@%s/%s", room_name, agent->alias, nick);
	z = jutil_presnew (JPACKET__UNAVAILABLE, buffer, "Online");
	jab_send(JConn->conn, z);
	xmlnode_free(z);
	return(0);
}

int JABBER_IsChatRoom(char *from)
{
	char buffer[256];
	char *ptr=NULL;
	Agent *agent=NULL;

	if (!from)
		return 0;
	
	strncpy(buffer, from, 256);
	strtok(buffer, "/");
	ptr=strchr(buffer, '@');
	if(ptr)
		ptr++;
	else
		ptr=buffer;
	eb_debug(DBG_JBR, "Looking for %s\n", ptr);
	agent = j_find_agent_by_alias(ptr);
	if(agent && !strcmp(agent->type, "groupchat")) {
		eb_debug(DBG_JBR, "Returning True\n");
		return(1);
	}
	eb_debug(DBG_JBR, "Returning False\n");
	return(0);

}

int JABBER_JoinChatRoom(JABBER_Conn *JConn, char *room_name, char *nick)
{
	Agent *agent=NULL;
	xmlnode z;
	char buffer[256];

	eb_debug(DBG_JBR, ">\n");
	agent = j_find_agent_by_type("groupchat");
	/* no private conference agent found */
	if(!agent)
	{
		eb_debug(DBG_JBR, "No groupchat agent found!\n");
		return(-1);
	}
	eb_debug(DBG_JBR, "private conference agent found: %s\n", agent->alias);
	/* send a time packet to make sure server is there
	z = jutil_iqnew (jpacket__GET, NS_TIME);
	xmlnode_put_attrib(z, "id", "Time");
	xmlnode_put_attrib(z, "to", agent->alias);
	jab_send (JConn->conn, z);
	xmlnode_free(z);
	*/
	sprintf(buffer, "%s@%s/%s", room_name, agent->alias, nick);
	z = jutil_presnew (JPACKET__GROUPCHAT, buffer, "Online");
	xmlnode_put_attrib(z, "id", "GroupChat");
	jab_send(JConn->conn, z);
	xmlnode_free(z);
	eb_debug(DBG_JBR, "<\n");
	return(-1);
}

void j_list_agents()
{
	Agent *agent=NULL;
	GList *list=agent_list;

	for(list=agent_list; list; list=list->next) {
		agent=list->data;
		fprintf(stderr, "Agent: %s - %s %s %s %s\n",
			agent->host, agent->name, agent->alias, agent->desc, agent->service);
	}
}

void j_remove_agents_from_host(char *host)
{
	Agent *agent=NULL;
	GList *list=agent_list;

	eb_debug(DBG_JBR, "Removing host: %s\n", host);
	while(list)
	{
		agent=list->data;
		/* Move on before the data disappears */
		list=list->next;
		if(!strcmp(agent->host, host)) {
			eb_debug(DBG_JBR, "Removing %s\n", agent->alias);
			agent_list=g_list_remove(agent_list, agent);
			g_free(agent);
		}
	}
}

void j_add_agent(char *name, char *alias, char *desc, char *service, char *host, char *agent_type)
{
	Agent *agent=g_new0(Agent, 1);

	eb_debug(DBG_JBR,  "Agent %s[%s]: %s [%s] [%s]\n",
		host, name, alias, desc, service);
	if(!host)
	{
		g_warning("No host specified for service, not registering!");
		g_free(agent);
		return;
	}
	strncpy(agent->host, host, 256);
	if(agent_type)
		strncpy(agent->type, agent_type, 256);
	if(name)
		strncpy(agent->name, name, 256);
	if(alias)
		strncpy(agent->alias, alias, 256);
	if(desc)
		strncpy(agent->desc, desc, 256);
	if(service)
		strncpy(agent->service, service, 256);

	agent_list = g_list_append( agent_list, agent );
	return;
}

/* ----------------------------------------------*/


/* Functions to handle jabber responses */

void j_on_packet_handler(jconn conn, jpacket packet) {
	xmlnode x, y;
	char *id, *ns, *alias, *sub, *name, *from, *group;
	char *desc, *service, *to, *url;
	char *show, *type, *body, *subj, *ask, *code;
	char *room, *user, *agent_type;
	char buff[8192+1];	/* FIXME: Find right size */
	int iid, status;
	jid  jabber_id=NULL;
	JABBER_Conn *JConn=NULL;
	JABBER_InstantMessage JIM;
	JABBER_Dialog *JD;
	struct jabber_buddy JB;

	/* This should not return NULL, but might */
	eb_debug(DBG_JBR, ">\n");
	JConn=JCfindConn(conn);
	if(!JConn) {
		sprintf(buff, "%s@%s/%s - connection error, can't find connection, packets are being dropped!",
			conn->user->user, conn->user->server, conn->user->resource);
		ay_do_error( _("Jabber Error"), buff );
		eb_debug(DBG_JBR, "<Returning error - can't find connection\n");
		return;
	}
	jpacket_reset(packet);
	eb_debug(DBG_JBR,  "Packet: %s\n", packet->x->name);
	switch(packet->type) {
	case JPACKET_MESSAGE:
		eb_debug(DBG_JBR,  "MESSAGE received\n");
		from = xmlnode_get_attrib (packet->x, "from");
		type = xmlnode_get_attrib (packet->x, "type");
		/* type can be empty, chat, groupchat, or error */
		eb_debug(DBG_JBR, "Message type: %s\n", type);
		x = xmlnode_get_tag (packet->x, "body");
		body = xmlnode_get_data (x);
		x = xmlnode_get_tag (packet->x, "subject");
		if (x) {
			subj = xmlnode_get_data (x);
			snprintf(buff, 8192, "%s: %s", subj, body);
		}
		else {
			subj = NULL;
			if(body)
				strncpy(buff, body, 8192);
			else
				buff[0]='\0';
		}
		eb_debug (DBG_JBR, "from %s, type %s, body: %s, subj %s\n", from, type, body, subj);
		if(type && !strcmp(type, "groupchat"))
		{
			user=strchr(from, '/');
			room=strtok(from, "@");
			if(!user) {
				user=room;
				user+=strlen(room)+1;
			}
			else
				user++;
			JABBERChatRoomMessage(room, user, buff);
		}
		else
		{
			/* For now, ayttm does not understand resources */
			JIM.msg=buff;
			JIM.JConn = JConn;
			eb_debug(DBG_JBR,  "JIM.msg: %s\n", JIM.msg);
			eb_debug(DBG_JBR,  "Rendering message\n");
			JIM.sender=strtok(from, "/");
			if(!JIM.sender)
				JIM.sender=from;
			eb_debug(DBG_JBR,  "JIM.sender: %s\n", JIM.sender);
			JABBERInstantMessage(&JIM);
		}
		break;
	case JPACKET_IQ:
		eb_debug(DBG_JBR,  "IQ packet received\n");
		type = xmlnode_get_attrib(packet->x, "type");
		from = xmlnode_get_attrib(packet->x, "from");
		if(!type) {
			eb_debug(DBG_JBR,  "<JPACKET_IQ: NULL type\n");
			return;
		}
		eb_debug(DBG_JBR,  "IQ received: %s\n", type);
		if(!strcmp(type, "result")) {
			id = xmlnode_get_attrib (packet->x, "id");
			if (id) {
				iid = atoi (id);
				eb_debug(DBG_JBR,  "iid: %i\n", iid);
				if (iid == JConn->id) {
					if (!JConn->reg_flag) {
						eb_debug (DBG_JBR, "requesting roster\n");
						x = jutil_iqnew (JPACKET__GET, NS_ROSTER);
						xmlnode_put_attrib(x, "id", "Roster");
						jab_send (conn, x);
						xmlnode_free(x);
						eb_debug (DBG_JBR, "requesting agent list\n");
						x = jutil_iqnew (JPACKET__GET, NS_AGENTS);
						xmlnode_put_attrib(x, "id", "Agent List");
						jab_send (conn, x);
						xmlnode_free(x);
						eb_debug(DBG_JBR,  "<Requested roster and agent list\n");
						return;
					}
					else {
						JConn->reg_flag = 0;
						JConn->id = atoi (jab_auth (conn));
						eb_debug (DBG_JBR, "<Performing auth\n");
						return;
					}
				}
			}
			else {
				eb_debug(DBG_JBR,  "No ID!\n");
			}
			x = xmlnode_get_tag (packet->x, "query");
			if (!x) {
//				eb_debug(DBG_JBR,  "No Query tag in packet, don't care.\n");
				break;
			}
			ns = xmlnode_get_attrib (x, "xmlns");
			eb_debug(DBG_JBR,  "ns: %s\n", ns);
			if (strcmp (ns, NS_ROSTER) == 0) {
				y = xmlnode_get_tag (x, "item");
				for (;;) {
					alias = xmlnode_get_attrib (y, "jid");
					sub = xmlnode_get_attrib (y, "subscription");
					name = xmlnode_get_attrib (y, "name");
					group = xmlnode_get_attrib (y, "group");
					eb_debug(DBG_JBR,  "Buddy: %s(%s) %s %s\n", alias, group, sub, name);
					if(alias && sub && name) {
						JB.jid=strtok(alias, "/");
						if(!JB.jid) {
							JB.jid=strdup(alias);
						}
						else {
							JB.jid=strdup(JB.jid);
						}
						JB.name=strdup(name);
						JB.sub=strdup(sub);
						/* State does not matter */
						JB.status=JABBER_OFFLINE;
						JB.JConn=JConn;
						JABBERAddBuddy(&JB);
						free(JB.name);
						free(JB.sub);
						free(JB.jid);
					}
					y = xmlnode_get_nextsibling (y);
					if (!y) break;
				}
			}
			else if(strcmp (ns, NS_AGENTS)==0) {
				y = xmlnode_get_tag (x, "agent");
				iid=1001;
				for (;;) {
					alias = xmlnode_get_attrib (y, "jid");
					if(alias) {
						name = xmlnode_get_tag_data (y, "name");
						desc = xmlnode_get_tag_data (y, "description");
						service = xmlnode_get_tag_data (y, "service");
						agent_type=NULL;
						if(xmlnode_get_tag(y, "groupchat"))
							agent_type="groupchat";
						else if(xmlnode_get_tag(y, "transport"))
							agent_type="transport";
						else if (xmlnode_get_tag(y, "search"))
							agent_type="search";
						eb_debug(DBG_JBR, "Agent type: %s found from: %s!\n", agent_type, from);
						j_add_agent(name, alias, desc, service, from, agent_type);
/*						z = jutil_iqnew (JPACKET__GET, NS_AGENT);
						xmlnode_put_attrib(z, "to", alias);
						xmlnode_put_attrib(z, "id", name);

//						xmlnode_put_attrib(z, "id", "1");
						to=xmlnode_get_attrib(z, "to");
						eb_debug(DBG_JBR, "to: %s - %s - %s\n", to,
							xmlnode_get_attrib(z, "type"),
							xmlnode_get_attrib(z, "id"));
*/
						/* Don't send the request until we can support the results */
						//jab_send (conn, z);
						//xmlnode_free(z);
					}
					y = xmlnode_get_nextsibling (y);
					if (!y) break;
				}
			}
			else if(strcmp (ns, NS_AGENT)==0) {
				name=xmlnode_get_tag_data(x, "name");
				url =xmlnode_get_tag_data(x, "url");
				eb_debug(DBG_JBR, "agent: %s - %s\n", name, url);
			}
			jab_send_raw (conn, "<presence/>");
			eb_debug (DBG_JBR, "<Announcing presenceh\n");
			return;
			
		}
		else
		if(!strcmp(type, "set")) {
			x = xmlnode_get_tag (packet->x, "query");
			ns = xmlnode_get_attrib (x, "xmlns");
			if (strcmp (ns, NS_ROSTER) == 0) {
				y = xmlnode_get_tag (x, "item");
				alias = xmlnode_get_attrib (y, "jid");
				sub = xmlnode_get_attrib (y, "subscription");
				name = xmlnode_get_attrib (y, "name");
				ask = xmlnode_get_attrib (y, "ask");
				eb_debug(DBG_JBR,  "Need to find a buddy: %s %s %s %s\n", alias, sub, name, ask);
/*				bud = buddy_find (alias);
*/
				if (sub) {
					if (strcmp (sub, "remove") == 0) {
						eb_debug(DBG_JBR,  "Need to remove a buddy: %s\n", alias);
					}
				}
/*
				if (!bud) {
					buddy_add_to_clist (buddy_add (alias, sub, name));
					if (!name && !ask) roster_new ("Edit User", alias, NULL);
				}
				else if (name) {
					free (bud->name);
					bud->name = strdup (name);
					set_name (bud);
				}
*/
			}

		}
		else
		if(!strcmp(type, "error")) {
			x = xmlnode_get_tag (packet->x, "error");
			from = xmlnode_get_attrib (packet->x, "from");
			to=xmlnode_get_attrib(packet->x, "to");
			code = xmlnode_get_attrib (x, "code");
			name = xmlnode_get_attrib (x, "id");
			desc = xmlnode_get_tag_data(packet->x, "error");
			eb_debug(DBG_JBR, "Received error: [%i]%s from %s[%s], sent to %s\n", atoi(code), desc, from, name, to);
			switch(atoi(code)) {
			case 401: /* Unauthorized */
				if (JConn->reg_flag)
					break;
				//FIXME: We need to change our status to offline first
				JD=calloc(sizeof(JABBER_Dialog), 1);
				name = xmlnode_get_tag_data(packet->iq, "username");
				jabber_id = jab_getjid(conn);
				JD->heading=_("Register? You're not authorized");
				sprintf(buff, _("Do you want to try to create the account \"%s\" on the jabber server %s?"), 
						jabber_id->user, jabber_id->server );
				JD->message=strdup(buff);
				JD->callback=j_on_create_account;
				JD->JConn=JConn;
				JABBERDialog(JD);
				break;
			case 302: /* Redirect */
			case 400: /* Bad request */
			case 402: /* Payment Required */
			case 403: /* Forbidden */
			case 404: /* Not Found */
			case 405: /* Not Allowed */
			case 406: /* Not Acceptable */
			case 407: /* Registration Required */
			case 408: /* Request Timeout */
			case 409: /* Conflict */
			case 500: /* Internal Server Error */
			case 501: /* Not Implemented */
			case 502: /* Remote Server Error */
			case 503: /* Service Unavailable */
			case 504: /* Remote Server Timeout */
			default:
				sprintf(buff, "%s@%s/%s - %s (%s)", conn->user->user, conn->user->server, conn->user->resource, desc, code);
				ay_do_error( _("Jabber Error"), buff );
				fprintf(stderr, "Error: %s\n", code);
			}
		}
		break;
	case JPACKET_PRESENCE:
		eb_debug(DBG_JBR,  "Presence packet received\n");
		status = JABBER_ONLINE;
		type = xmlnode_get_attrib (packet->x, "type");
		from = xmlnode_get_attrib (packet->x, "from");
		/* For now, ayttm does not understand resources */
		eb_debug(DBG_JBR,  "PRESENCE received type: %s from: %s\n", type, from);
		x = xmlnode_get_tag (packet->x, "show");
		if (x) {
			show = xmlnode_get_data (x);
			if(show) {
				if (strcmp (show, "away") == 0) status = JABBER_AWAY;
				else if (strcmp (show, "dnd") == 0) status = JABBER_DND;
				else if (strcmp (show, "xa") == 0) status = JABBER_XA;
				else if (strcmp (show, "chat") == 0) status = JABBER_CHAT;
			}
			else
			{
				eb_debug(DBG_JBR, "Presence packet received from %s w/o a status string, is this a conference room?\n",
					 from);
				break;
			}
		}
		if (type) {
			if (strcmp (type, "unavailable") == 0) status = JABBER_OFFLINE;
		}
		if(JABBER_IsChatRoom(from))
		{
			eb_debug(DBG_JBR, "Presence received from a conference room\n");
			user=strchr(from, '/');
			room=strtok(from, "@");
			if(!user) {
				user=room;
				user+=strlen(room)+1;
			}
			else
				user++;
			JABBERChatRoomBuddyStatus(room, user, status);
		}
		else
		{
			JB.jid=strtok(from, "/");
			if(!JB.jid)
				JB.jid=from;
			JB.status=status;
			JB.JConn=JConn;
			JABBERStatusChange(&JB);
		}
		break;
	case JPACKET_S10N:
		eb_debug(DBG_JBR,  "S10N packet received\n");
		from = xmlnode_get_attrib (packet->x, "from");
		type = xmlnode_get_attrib (packet->x, "type");
		if (type) {
			JD=calloc(sizeof(JABBER_Dialog), 1);
			if (strcmp (type, "subscribe") == 0) {
				sprintf(buff, _("%s wants to add you to his friends' list.\n\nDo you want to accept?"), 
						from);
				JD->message=strdup(buff);
				JD->heading="Subscribe Request";
				JD->callback=j_allow_subscribe;
				JD->requestor=strdup(from);
				JD->JConn=JConn;
				JABBERDialog(JD);
				eb_debug (DBG_JBR, "<Initiated dialog to allow or deny subscribe request\n");
				return;
			}
			else
			if (strcmp (type, "unsubscribe") == 0) {
				return;
			}
		}
		break;
	case JPACKET_UNKNOWN:
	default:
		from = xmlnode_get_attrib (packet->x, "from");
		if(from) {
			fprintf(stderr, "unrecognized packet: %i received from %s\n", packet->type, from);
		}
		else
			fprintf(stderr, "unrecognized packet: %i received\n", packet->type);
		break;
	}
	eb_debug (DBG_JBR, "<\n");
}

void j_on_state_handler(jconn conn, int state) {
	static int previous_state=JCONN_STATE_OFF;
	JABBER_Conn *JConn=NULL;
	char buff[4096];

	eb_debug(DBG_JBR, "Entering: new state: %i previous_state: %i\n", state, previous_state);
	JConn=JCfindConn(conn);
	switch(state) {
	case JCONN_STATE_OFF:
		if(previous_state!=JCONN_STATE_OFF) {
			eb_debug(DBG_JBR, "The Jabber server has disconnected you: %i\n", previous_state);
			snprintf(buff, 4096, _("The Jabber server %s has disconnected you."),
				JCgetServerName(JConn));
			JABBERError(buff, _("Disconnect"));
			eb_input_remove(JConn->listenerID);
			/* FIXME: Do we need to free anything? */
			j_remove_agents_from_host(JCgetServerName(JConn));
			JConn->conn=NULL;
			JABBERLogout(NULL);
		}
		else if(!JConn->conn || JConn->conn->state==JCONN_STATE_OFF) {
			snprintf(buff, 4096, _("Connection to the jabber server %s failed!"), conn->user->server);
			JABBERError(buff, _("Jabber server not responding"));
			JABBERLogout(NULL);
			jab_delete(JConn->conn);
			JConn->conn=NULL;
		}
		break;
	case JCONN_STATE_CONNECTED:
		eb_debug(DBG_JBR,  "JCONN_STATE_CONNECTED\n");
		break;
	case JCONN_STATE_AUTH:
		eb_debug(DBG_JBR,  "JCONN_STATE_AUTH\n");
		break;
	case JCONN_STATE_ON:
		eb_debug(DBG_JBR,  "JCONN_STATE_ON\n");
		if (previous_state == JCONN_STATE_CONNECTED) {
			JConn->id = atoi (jab_auth(JConn->conn));
			JConn->listenerID = eb_input_add(JConn->conn->fd, EB_INPUT_READ,
	                        	jabber_callback_handler, JConn);
			eb_debug(DBG_JBR,  "*** ListenerID: %i FD: %i\n", JConn->listenerID, JConn->conn->fd);
			JABBERConnected(JConn);
		}
		break;
	default:
		eb_debug(DBG_JBR,  "UNKNOWN state: %i\n", state);
		break;
	}
	previous_state=state;
	eb_debug(DBG_JBR, "Leaving\n");
}

void j_on_create_account(void *data) {
	JABBER_Dialog_PTR jd;

	eb_debug(DBG_JBR,  "Entering, but doing little\n");
	jd=(JABBER_Dialog_PTR)data;
	jd->JConn->reg_flag = 1;
	jd->JConn->id = atoi (jab_reg (jd->JConn->conn));
	eb_debug(DBG_JBR,  "Leaving\n");
	jd->JConn->conn->sid = NULL;
}

void j_allow_subscribe(void *data) {
	JABBER_Dialog_PTR jd;
	xmlnode x, y, z;

	jd=(JABBER_Dialog_PTR)data;
	eb_debug(DBG_JBR,  "%s: Entering\n", jd->requestor);
	x=jutil_presnew(JPACKET__SUBSCRIBED, jd->requestor, NULL);
	jab_send(jd->JConn->conn, x);
	xmlnode_free(x);

	/* We want to request their subscription now */
	x=jutil_presnew(JPACKET__SUBSCRIBE, jd->requestor, NULL);
	jab_send(jd->JConn->conn, x);
	xmlnode_free(x);

	/* Request roster update */
	x = jutil_iqnew (JPACKET__SET, NS_ROSTER);
	y = xmlnode_get_tag (x, "query");
	z = xmlnode_insert_tag (y, "item");
	xmlnode_put_attrib (z, "jid", jd->requestor);
	xmlnode_put_attrib (z, "name", jd->requestor);
	jab_send (jd->JConn->conn, x);
	xmlnode_free(x);
	eb_debug(DBG_JBR,  "Leaving\n");
}

void j_unsubscribe(void *data) {
	JABBER_Dialog_PTR jd;

	jd=(JABBER_Dialog_PTR)data;
	JABBERDelBuddy(jd->JConn, jd->requestor);
}

void j_on_pick_account(void *data) {
	JABBER_Dialog_PTR jd;
	JABBER_Conn *JConn=NULL;

	jd=(JABBER_Dialog_PTR)data;
	JConn=JCfindJID(jd->response);
	eb_debug(DBG_JBR, "Found JConn for %s: %p\n", jd->requestor, JConn);
	/* Call it again, this time with a good JConn */
	if(!JConn) {
		fprintf(stderr, "NULL Jabber connector for buddy, should not happen!\n");
		return;
	}
	JABBER_AddContact(JConn, jd->requestor);
}
