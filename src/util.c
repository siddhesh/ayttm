/*
 * Ayttm 
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
 * util.c
 * just some handy functions to have around
 *
 */

#include "intl.h"

#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/stat.h>
#include <assert.h>

#ifdef __MINGW32__
#include <winsock2.h>
#else
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

#include "util.h"
#include "status.h"
#include "globals.h"
#include "chat_window.h"
#include "value_pair.h"
#include "plugin.h"
#include "dialog.h"
#include "service.h"
#include "offline_queue_mgmt.h"
#ifdef __MINGW32__
#define snprintf _snprintf
#endif


#ifndef NAME_MAX
#define NAME_MAX 4096
#endif

int clean_pid(void * dummy)
{
#ifndef __MINGW32__
    int status;
    pid_t pid;

    pid = waitpid(-1, &status, WNOHANG);

    if (pid == 0)
        return TRUE;
#endif

    return FALSE;
}

char * get_local_addresses()
{
	static char addresses[1024];
	char buff[1024];
	char gateway[16];
	char  * c;
	struct hostent * hn;
	int i;
	FILE * f;
	//system("getip.pl > myip");
	f = popen("netstat -nr", "r");
	if((int)f < 1)
			goto IP_TEST_2;
	while( fgets(buff, sizeof(buff), f)  != NULL ) {
			c = strtok( buff, " " );
			if( (strstr(c, "default") || strstr(c,"0.0.0.0") ) &&
							!strstr(c, "127.0.0" ) )
					break;
	}
	c = strtok( NULL, " " );
	pclose(f);

	strncpy(gateway,c, 16);



	for(i = strlen(gateway); gateway[i] != '.'; i-- )
		gateway[i] = 0;

	gateway[i] = 0;

	for(i = strlen(gateway); gateway[i] != '.'; i-- )
		gateway[i] = 0;

	//g_snprintf(buff, 1024, "/sbin/ifconfig -a|grep inet|grep %s", gateway );
	f = popen("/sbin/ifconfig -a", "r");
	if((int)f < 1)
		goto IP_TEST_2;

	while( fgets(buff, sizeof(buff), f) != NULL ) {
		if( strstr(buff, "inet") && strstr(buff,gateway) )
			break;
	}
	pclose(f);

	c = strtok( buff, " " );
	c = strtok( NULL, " " );

	strncpy ( addresses, c, sizeof(addresses) );
	c = strtok(addresses, ":" );
	strncpy ( buff, c, sizeof(buff) );
	if((c=strtok(NULL, ":")))
		strncpy( buff, c, sizeof(buff) );

	strncpy(addresses, buff, sizeof(addresses));

	return addresses;
		
		
IP_TEST_2:

	gethostname(buff,sizeof(buff));

	hn = gethostbyname(buff);
	if(hn)
		strncpy(addresses, inet_ntoa( *((struct in_addr*)hn->h_addr)), sizeof(addresses) );
	else
		addresses[0] = 0;

	return addresses;
}


/* a valid domain name is one that contains only alphanumeric, - and . 
 * if it has anything else, then it can't be a domain name
 */

static int is_valid_domain( char * name )
{
	int i;
	int dot_count = 0;
	if( name[0] == '-' || name[0] == '.' )
		return FALSE;

	for( i = 0; name[i] && name[i] != '/' && name[i] != ':'; i++ ) {
		if( !isalnum(name[i]) && name[i] != '.' && name[i] != '-' )
			return FALSE;

		if( name[i] == '.' ) {
			if( name[i-1] == '.' || name[i-1] == '-' 
			||  name[i+1] == '.' || name[i+1] == '-' )
				return FALSE;

			dot_count++;
		}
	}
	if( name[i] == ':' ) {
		for( i = i+1; name[i] && name[i] != '/'; i++ ) {
			if(!isdigit(name[i]))
				return FALSE;
		}
	}
	return dot_count > 0;
}

char * escape_string(const char * input)
{
	GString * temp_result = g_string_sized_new(2048);
	char * result;
	int ipos = 0;
	for(ipos=0;input[ipos];ipos++) {
		if(input[ipos] == '\n')
			g_string_append(temp_result, "\\n");
		else if(input[ipos] == '\r')
			g_string_append(temp_result, "\\r");
		else if(input[ipos] == '\\')
			g_string_append(temp_result, "\\\\");
		else if(input[ipos] == '"')
			g_string_append(temp_result, "\\\"");
		else
			g_string_append_c(temp_result, input[ipos]);
	}

	result = temp_result->str;
	g_string_free(temp_result, FALSE);
	return result;
}
char * unescape_string(const char * input)
{
	char * result = calloc(strlen(input)+1, sizeof(char));
	int ipos=0, opos=0; 
	while(input[ipos]) {
		char c = input[ipos++];
		if(c == '\\') {
			c = input[ipos++];
			switch(c) {
			case 'n':
				result[opos++] = '\n'; break;
			case 'r':	
				result[opos++] = '\r'; break;
			case '\\':	
				result[opos++] = '\\'; break;
			case '\"':	
				result[opos++] = '\"'; break;
			}
		} else
			result[opos++] = c;
	}
	result[opos] = '\0';

	return result;

}
/* this function will be used to determine if a given token is a link */

static int is_link( char * token )
{
	int i;
	int len = strlen(token);
	
	if( token[0] == '<' )
		return TOKEN_NORMAL;

	if(!strncasecmp( token, "http://", 7 ) ) {
		if(is_valid_domain(token+7))
			return TOKEN_HTTP;
		else
			return TOKEN_NORMAL;
	}
	
	if(!strncasecmp( token, "ftp://", 6))
		return TOKEN_FTP;
	if(!strncasecmp( token, "log://", 6))
		return TOKEN_CUSTOM;
	if(!strncasecmp( token, "mailto:", 7))
		return TOKEN_EMAIL;
	if(!strncasecmp( token, "www.", 4) && is_valid_domain(token))
		return TOKEN_HTTP;
	if(!strncasecmp( token, "ftp.", 4) && is_valid_domain(token))
		return TOKEN_FTP;
	if(strstr(token, "://") && !ispunct(token[0]) && !ispunct(token[strlen(token)]) )
		return TOKEN_CUSTOM;

	for(i = 0; i < len; i++ ) {
		if(token[i] == '@' ) {
			if( !ispunct(token[0]) && !ispunct(token[len-1]) ) {
				if(is_valid_domain(token+i+1))
					return TOKEN_EMAIL;
				break;
			}
		}
	}
	
	for(i=len; i >= 0; i-- ) {
		if( token[i] == '.' ) {
			if(!strcasecmp(token+i, ".edu") && is_valid_domain(token))
				return TOKEN_HTTP;
			if(!strcasecmp(token+i, ".com") && is_valid_domain(token))
				return TOKEN_HTTP;
			if(!strcasecmp(token+i, ".net") && is_valid_domain(token))
				return TOKEN_HTTP;
			if(!strcasecmp(token+i, ".org") && is_valid_domain(token))
				return TOKEN_HTTP;
			if(!strcasecmp(token+i, ".gov") && is_valid_domain(token))
				return TOKEN_HTTP;
			break;
		}
	}
	return TOKEN_NORMAL;
}

static GString * get_next_token(char * input)
{
	int i = 0;
	int len = strlen(input);
	GString * string = g_string_sized_new(20);

	if(!strncasecmp(input, "<A", 2)) {
		g_string_assign(string, "<A");
		for( i = 2; i < len; i++ ) {
			g_string_append_c(string, input[i]);
			if( input[i] != '<' )
				continue;
			g_string_append_c(string, input[++i]);
			if( input[i] != '/' )
				continue;
			g_string_append_c(string, input[++i]);
			if( tolower(input[i]) != 'a' )
				continue;
			g_string_append_c(string, input[++i]);
			break;
		}
		return string;
	}
	if(input[0] == '<') {
		for(i=0; i < len; i++) {
			g_string_append_c(string, input[i]);
			if(input[i] == '>')
				break;
		}
		return string;
	}

	if( ispunct(input[0]) ) {
		for( i=0; i < len; i++ ) {
			if( ispunct(input[i]) && input[i] != '<' )
				g_string_append_c(string, input[i]);
			else
				break;
		}
		return string;
	}

	/*
	 * now that we have covered prior html
	 * we can do an (almost) simple word tokenization
	 */

	for( i = 0; i < len; i++ ) {
		if(!isspace(input[i]) && input[i] != '<' ) {
			if(!ispunct(input[i]) || input[i] == '/' )
				g_string_append_c(string, input[i]);
			else {
				int j;
				for(j = i+1; j < len; j++ ) {
					if( isspace(input[j] ) )
						return string;
					if( isalpha( input[j] ) || isdigit(input[j] ) ) 
						break;
				}
				if( j == len )
					return string;
				else
					g_string_append_c(string, input[i]);
			}
		} else
			return string;
	}
	return string;
}
			
static void linkify_token( GString * token )
{
	GString * g;
	GString * g2;
	int type = is_link(token->str);

	if(type == TOKEN_NORMAL)
		return;

	g = g_string_sized_new(token->len);
	g2 = g_string_sized_new(token->len);

	g_string_assign(g, token->str);
	g_string_assign(g2, token->str);
	

		
	if(type == TOKEN_HTTP && strncasecmp(token->str, "http://", 7))
		g_string_prepend(g2, "http://");
	else if( type == TOKEN_FTP && strncasecmp(token->str, "ftp://", 6))
		g_string_prepend(g2, "ftp://");
	else if( type == TOKEN_EMAIL && strncasecmp(token->str, "mailto:", 7))
		g_string_prepend(g2, "mailto:");

	eb_debug(DBG_HTML, "TOKEN: %s\n", token->str);
	/* g_string_sprintf is safe */
	g_string_sprintf(token, "<A HREF=\"%s\">%s</A>", g2->str, g->str);

	g_string_free(g,TRUE);
	g_string_free(g2,TRUE);
}

char * linkify( char * input )
{
	int i = 0;
	int len = strlen(input);
	char * result;
	GString * temp_result;
	GString * temp = NULL;

	temp_result = g_string_sized_new(2048);

	while( i < len ) {
		if( isspace(input[i]) ) {
			g_string_append_c(temp_result, input[i]);
			i++;
		} else {
			temp = get_next_token(input+i);
			eb_debug(DBG_HTML, "%s\t%s\t%d\t%d\n", input, input+i, i, temp->len);
			i += temp->len;
			linkify_token(temp);
			g_string_append(temp_result, temp->str);
			g_string_free(temp, TRUE);
		}
	}

	result = temp_result->str;
	g_string_free(temp_result, FALSE);
	return result;
}

char * convert_eol (char * text) 
{
	char * temp;
	char **data=NULL;
	int i;

	if(!text || !*text)
		return g_strdup("");

	if (strstr (text, "\r\n") != NULL)
		return g_strdup(text);
	
	data = g_strsplit(text,"\n",64);
	temp = g_strdup(data[0]);
	for(i=1; data[i] != NULL; i++) {
		temp = g_strdup_printf("%s\r\n%s",temp,data[i]);
	}
	g_strfreev(data);
	return temp;
}

static grouplist * dummy_group = NULL;

/* Parameters:
	remote - the account to find a local account for
	online - whether the local account needs to be online
*/
eb_local_account * find_local_account_for_remote( eb_account *remote, int online )
{
	static LList * node = NULL;
	static eb_account *last_remote=NULL;

	/* If this is a normal call, start at the top and give the first, otherwise continue where we left off */
	if(remote) {
		node = accounts;
		last_remote=remote;
	} else {
		remote = last_remote;
		if(node)
			node=node->next;
	}
	
	for( ; node; node = node->next ) {
		eb_local_account * ela = node->data;
		
		if (remote->service_id == ela->service_id) {
			if ( (!online || ela->connected ) && 
					( !RUN_SERVICE(ela)->is_suitable || 
					  RUN_SERVICE(ela)->is_suitable(ela, remote) ) )
				return ela;
		}
	}
	
	/*We can't find anything, let's bail*/
	return NULL;
}

int connected_local_accounts(void)
{
	int n = 0;
	LList *node = NULL;
	for( node = accounts; node; node = node->next ) {
		eb_local_account * ela = node->data;
		if (ela->connected || ela->connecting) {
			n++;
			eb_debug(DBG_CORE, "%s: connected=%d, connecting=%d\n",
					ela->handle, ela->connected, ela->connecting);
		}
	}
	return n;
}

eb_local_account * find_suitable_local_account( eb_local_account * first, int second )
{
	LList * node;
	LList * states;
	
	/* The last state in the list of states will be the OFFLINE state
	 * The first state in the list will be the ONLINE states
	 * The rest of the states are the various AWAY states
	 */
	
	states = eb_services[second].sc->get_states();
	
	l_list_free(states);
	
	if( first && ( first->connected || first->connecting) )
		return first;
			
	/*dang, we are out of luck with our first choice, do we have something
	  else that uses the same service? */

	for( node = accounts; node; node = node->next ) {
		eb_local_account * ela = node->data;
		eb_debug(DBG_CORE, "%s %s\n", get_service_name(ela->service_id), ela->handle);
		
		if( ela->service_id == second && (ela->connected || ela->connecting))
			return ela;
		else if( !ela->connected )
			eb_debug(DBG_CORE, "%s is offline!\n", ela->handle );
			
	}
	
	/*We can't find anything, let's bail*/
	return NULL;
}

/* if contact can offline message, return the account, otherwise, return null */

eb_account * can_offline_message( struct contact * con )
{
	LList * node;
	for(node = con->accounts; node; node=node->next) {
		eb_account * ea = node->data;
		
		if( eb_services[ea->service_id].offline_messaging )
			return ea;
	}
	return NULL;
}	
	
eb_account * find_suitable_remote_account( eb_account * first, struct contact * rest )
{
	LList * node;
	eb_account * possibility = NULL;

	if( first && (eb_services[first->service_id].offline_messaging || 
			 RUN_SERVICE(first)->query_connected(first)) )
		return first;
	
	for(node = rest->accounts; node; node=node->next) {
		eb_account * ea = node->data;
		
		if( RUN_SERVICE(ea)->query_connected(ea) ) {
			if(ea->service_id == rest->default_chatb ) 
				return ea;
			else
				possibility = ea;
		}
	}
	return possibility;
}
	
eb_account * find_suitable_file_transfer_account( eb_account * first, struct contact * rest )
{
	LList * node;
	eb_account * possibility = NULL;

	if ( first == NULL )
		return NULL;

	if( first && RUN_SERVICE(first)->query_connected(first)
			&& eb_services[first->service_id].file_transfer )
		return first;
	
	for(node = rest->accounts; node; node=node->next) {
		eb_account * ea = node->data;
		
		if( RUN_SERVICE(ea)->query_connected(ea)
				&& eb_services[first->service_id].file_transfer ) {
			if(ea->service_id == rest->default_chatb )
				return ea;
			else
				possibility = ea;
		}
	}
	return possibility;
}
	
eb_chat_room * find_chat_room_by_id( char * id )
{
	LList * node = chat_rooms;
	for( node= chat_rooms; node; node=node->next) {
		eb_chat_room * ecr = node->data;
		eb_debug(DBG_CORE, "Comparing %s to %s\n", id, ecr->id );
		if(!strcmp(id, ecr->id))
			return ecr;
	}
	return NULL;

}

eb_chat_room * find_chat_room_by_name( char * name, int service_id )
{
	LList * node = chat_rooms;
	for( node= chat_rooms; node && node->data; node=node->next) {
		eb_chat_room * ecr = node->data;
		if((ecr->local_user->service_id == service_id) && !strcmp(name, ecr->room_name) )
			return ecr;
	}
	return NULL;

}

LList * find_chatrooms_with_remote_account(eb_account *remote)
{
	LList * node = NULL;
	LList * result = NULL;
	
	for (node = chat_rooms; node && node->data; node = node->next) {
		eb_chat_room *ecr = node->data;
		if (remote->service_id == ecr->local_user->service_id) {
			LList *others = NULL;
			for (others = ecr->fellows; others && others->data; others = others->next) {
				eb_chat_room_buddy *ecrb = others->data;
				if(!strcmp(remote->handle, ecrb->handle)) {
					result = l_list_append(result, ecr);
					eb_debug(DBG_CORE, "Found %s in %s\n",remote->handle, ecr->room_name);
					break;
				}
			}
		}
	}
	return result;
}

grouplist * find_grouplist_by_name(char * name)
{
	LList * l1;

	if (name == NULL)
		return NULL;

	for(l1 = groups; l1; l1=l1->next )
		if(!strcasecmp(((grouplist *)l1->data)->name, name))
			return l1->data;

	return NULL;
}

grouplist * find_grouplist_by_nick(char * nick)
{
	LList * l1, *l2;

	if (nick == NULL) 
		return NULL;
    
	for(l1 = groups; l1; l1=l1->next )
		for(l2 = ((grouplist*)l1->data)->members; l2; l2=l2->next )
			if(!strcmp(((struct contact*)l2->data)->nick, nick))
				return l1->data;

	return NULL;
	
}

struct contact * find_contact_by_handle( char * handle )
{
	LList *l1, *l2, *l3;

	if (handle == NULL) 
		return NULL;

	for(l1 = groups; l1; l1=l1->next ) {
		for(l2 = ((grouplist*)l1->data)->members; l2; l2=l2->next ) {
			for(l3 = ((struct contact*)l2->data)->accounts; l3; l3=l3->next) {
				eb_account * account = (eb_account*)l3->data;
				if(!strcmp(account->handle, handle))
					return (struct contact*)l2->data;
			}
		}
	}
	return NULL;
}

struct contact * find_contact_by_nick( const char * nick )
{
	LList *l1, *l2;

	if (nick == NULL)
		return NULL;

	for(l1 = groups; l1; l1=l1->next ) {
		for(l2 = ((grouplist*)l1->data)->members; l2; l2=l2->next ) {
			if(!strcasecmp(((struct contact*)l2->data)->nick, nick))
				return (struct contact*)l2->data;
		}
	}
	return NULL;
}

struct contact * find_contact_in_group_by_nick( char * nick , grouplist *gl )
{
	LList * l;

	if (nick == NULL || gl == NULL)
		return NULL;

	for(l = gl->members; l; l=l->next ) {
		if(!strcasecmp(((struct contact*)l->data)->nick, nick))
			return (struct contact*)l->data;
	}

	return NULL;
}

eb_account * find_account_by_handle( const char * handle, int type )
{
	LList *l1, *l2, *l3;

	if (handle == NULL)
		return NULL;

/* go on as long as there's groups in groups or if we have a dummy_group */
	for(l1 = groups; l1 || dummy_group; l1=l1->next ) {
		grouplist *g=NULL;
		/* if groups is over, then check the dummy_group */
		if(l1)
			g = l1->data;
		else
			g = dummy_group;

		for(l2 = g->members; l2; l2=l2->next ) {
			for(l3 = ((struct contact*)l2->data)->accounts; l3; l3=l3->next) {
				eb_account * ea = l3->data;

				if(ea->service_id == type && !strcasecmp(handle, ea->handle))
					return ea;
			}
		}

		/* if groups is over, then we've also finished with dummy */
		if(!l1)
			break;
	}
	return NULL;
}

eb_local_account * find_local_account_by_handle( char * handle, int type)
{
	LList * l1;

	if (handle == NULL)
		return NULL;

	for(l1 = accounts; l1; l1=l1->next ) {
		eb_local_account * account = (eb_local_account*)l1->data;
		if(account->service_id == type && !strcasecmp(account->handle, handle))
			return account;
	}
	return NULL;
}

void strip_html(char * text)
{
	int i, j;
	int visible = 1;
	
	for( i=0, j=0; text[i]; i++ ) {
		if(text[i]=='<') {
			switch(text[i+1]) {
				case 'a':
				case 'A':
					if(isspace(text[i+2]) || text[i+2] == '>')
						visible = 0;
					break;

				case 'i':
				case 'I':
				case 'u':
				case 'U':
				case 'p':
				case 'P':
					if(text[i+2] == '>')
						visible = 0;
					break;
				case 'b':
				case 'B':
					if(text[i+2] == '>' 
					|| !strncasecmp(text+i+1, "body ", 5)
					|| !strncasecmp(text+i+1, "body>", 5))
						visible = 0;
					break;
				case 'h':
				case 'H':
					if(!strncasecmp(text+i+1, "html ", 5)
					|| !strncasecmp(text+i+1, "html>", 5))
						visible = 0;
					break;
								
				case 'F':
				case 'f':
					if(!strncasecmp(text+i+1, "font ", 5)
					|| !strncasecmp(text+i+1, "font>", 5))
						visible = 0;
					break;
				case 's':
					if (!strncasecmp(text+i+2,"miley", 5)) {
						visible = 0;
						text[j++]=' '; /*hack*/
					}
				case '/':
					visible = 0;
					break;
			}
		}
		else if(text[i] == '>')
		{
			if(!visible)
				visible = 1;
				continue;
		}
		if(visible)
			text[j++] = text[i];
	}
	text[j] = '\0';
}


int remove_account( eb_account * a )
{
	struct contact * c = a->account_contact;

	if (!find_suitable_local_account(NULL, a->service_id)) 
		contact_mgmt_queue_add(a, MGMT_DEL, NULL);
  

	buddy_logoff(a);
	remove_account_line(a);
	c->accounts = l_list_remove(c->accounts, a);

	RUN_SERVICE(a)->del_user(a);
	g_free(a);

	if (l_list_empty(c->accounts)) {
		remove_contact(c);
		return 0; /* so if coming from remove_contact 
			don't try again to remove_contact_line() 
			and so on */
	}

	return 1;
}
void remove_contact( struct contact * c )
{
	grouplist * g = c->group;

	if(c->chatwindow)
		gtk_widget_destroy(c->chatwindow->window);

	while(c->accounts && !l_list_empty(c->accounts))
		if (!remove_account(c->accounts->data))
			return;

	remove_contact_line(c);
	g->members = l_list_remove(g->members, c);
	g_free(c);
}

void remove_group( grouplist * g )
{
	LList *node = NULL;
	while(g->members)
	        remove_contact(g->members->data);

	remove_group_line(g);
	groups = l_list_remove(groups,g);

	for( node = accounts; node; node = node->next ) {
		eb_local_account * ela = (eb_local_account *)(node->data);
		if (ela->connected && RUN_SERVICE(ela)->del_group) {
			eb_debug(DBG_CORE, "dropping group %s in %s\n",
				g->name, get_service_name(ela->service_id));
			RUN_SERVICE(ela)->del_group(g->name);
		} else if (RUN_SERVICE(ela)->del_group) {
			group_mgmt_queue_add(ela, g->name, MGMT_GRP_DEL, NULL);
		}
			
	}
	g_free(g);
}

void add_group( char * name )
{
	LList *node = NULL;
	grouplist *eg;
	
	eg = calloc(1, sizeof(grouplist));
	
	strncpy(eg->name, name, sizeof(eg->name));

	groups = l_list_append( groups, eg );
	add_group_line(eg);

	for( node = accounts; node; node = node->next ) {
		eb_local_account * ela = node->data;
		if (RUN_SERVICE(ela)->add_group)
		{
			if (ela->connected) {
				eb_debug(DBG_CORE, "adding group %s in %s\n",
					name, get_service_name(ela->service_id));
				RUN_SERVICE(ela)->add_group(name);
			} else {
				group_mgmt_queue_add(ela, NULL, MGMT_GRP_ADD, name);
			}
		}
	}
	update_contact_list();
	write_contact_list();
}

void rename_group( grouplist *current_group, char * new_name )
{
	LList *node = NULL;
	char oldname[NAME_MAX];

	strncpy (oldname, current_group->name, sizeof(oldname));
	strncpy (current_group->name, new_name, sizeof(current_group->name));
	gtk_label_set_text(GTK_LABEL(current_group->label),
				   current_group->name);
	update_contact_list();
	write_contact_list();

	for (node = current_group->members; node; node = node->next) {
		struct contact *c = (struct contact *)node->data;
		if (c)
			rename_nick_log(oldname, c->nick, new_name, c->nick);
	}
	
	for( node = accounts; node; node = node->next ) {
		eb_local_account * ela = node->data;
		if (RUN_SERVICE(ela)->rename_group)
		{
			if (ela->connected) {
				eb_debug(DBG_CORE, "renaming group %s to %s in %s\n",
					oldname, new_name, get_service_name(ela->service_id));
				RUN_SERVICE(ela)->rename_group(oldname, new_name);
			} else {
				group_mgmt_queue_add(ela, oldname, MGMT_GRP_REN, new_name);
			}
		}
	}
}


/* compares two contact names */
static int contact_cmp(const void * a, const void * b)
{
	const struct contact *ca=a, *cb=b;
	return strcasecmp(ca->nick, cb->nick);
}

struct relocate_account_data {
	eb_account * account;
	struct contact * contact;
};

static void move_account_yn(void * data, int result)
{
	struct relocate_account_data * rad = data;

	if(result)
		move_account(rad->contact, rad->account);
	else if (l_list_empty(rad->contact->accounts))
		remove_contact(rad->contact);

	free(rad);
}

static void add_account_verbose( char * contact, eb_account * account, int verbosity )
{
	struct relocate_account_data * rad; 
	struct contact * c = find_contact_by_nick( contact );
	eb_account * ea = find_account_by_handle(account->handle, account->service_id);
	if(ea) {
		if(!strcasecmp(ea->account_contact->group->name, _("Unknown"))) {
			move_account(c, ea);
		} else {
			char buff[2048];
			snprintf(buff, sizeof(buff), 
					_("The account already exists on your\n"
					"contact list at the following location\n"
					"\tGroup: %s\n"
					"\tContact: %s\n"
					"\nShould I move it?"),
					ea->account_contact->group->name, ea->account_contact->nick );

			/* remove this contact if it was newly created */
			if(verbosity) {
				rad = calloc(1, sizeof(struct relocate_account_data));
				rad->account = ea;
				rad->contact = c;
				eb_do_dialog(buff, _("Error: account exists"), move_account_yn, rad);
			} else if( c && l_list_empty(c->accounts))
				remove_contact(c);
		}
		return;
	}
	if (!find_suitable_local_account(NULL, account->service_id)) {
		contact_mgmt_queue_add(account, MGMT_ADD, c?c->group->name:_("Unknown"));
	}
	if (c) {
		c->accounts = l_list_append( c->accounts, account );
		account->account_contact = c;
		RUN_SERVICE(account)->add_user(account);

		if(!strcmp(c->group->name, _("Ignore")) && 
			RUN_SERVICE(account)->ignore_user)
			RUN_SERVICE(account)->ignore_user(account);
	}
	else 
		add_unknown_with_name(account, contact);

	update_contact_list();
	write_contact_list();
}

void add_account_silent ( char * contact, eb_account * account )
{
	add_account_verbose(contact, account, FALSE);
}
void add_account( char * contact, eb_account * account )
{
	add_account_verbose(contact, account, TRUE);
}

static void add_contact( char * group, struct contact * user )
{
	grouplist * grp = find_grouplist_by_name(group);

	if(!grp) {
		add_group(group);
		grp = find_grouplist_by_name(group);
	}
	if(!grp) {
	    printf("Error adding group :(\n");
	    return;
	}    
	grp->members = l_list_insert_sorted(grp->members, user, contact_cmp);
	user->group = grp;
}

static struct contact * create_contact( const char * con, int type )
{
	struct contact * c = calloc(1, sizeof(struct contact));
	if (con != NULL) 
		strncpy(c->nick, con, sizeof(c->nick));

	c->default_chatb = c->default_filetransb = type;

	return( c );
}

struct contact * add_new_contact( char * group, char * con, int type )
{
	struct contact * c = create_contact(con, type);

	add_contact(group, c);

	return c;
}

/* used to chat with someone without adding him to your buddy list */
struct contact * add_dummy_contact(char * con, eb_account * ea)
{
	struct contact * c = create_contact(con, ea->service_id);

	c->accounts = l_list_prepend(c->accounts, ea);
	ea->account_contact = c;
	ea->icon_handler = ea->status_handler = -1;

	if(!dummy_group) {
		dummy_group = calloc(1, sizeof(grouplist));
		/* don't translate this string */
		snprintf(dummy_group->name, sizeof(dummy_group->name),
				"__Ayttm_Dummy_Group__%d__", rand());
	}

	dummy_group->members = l_list_prepend(dummy_group->members, c);
	c->group = dummy_group;
	return c;
}

void clean_up_dummies()
{
	LList *l, *l2;

	if (!dummy_group) 
		return;
	
	for(l = dummy_group->members; l; l=l->next) {
		struct contact * c = l->data;
		for(l2 = c->accounts; l2; l2=l2->next) {
			eb_account * ea = l2->data;
			if(CAN(ea, free_account_data))
				RUN_SERVICE(ea)->free_account_data(ea);
			free(ea);
		}
		l_list_free(c->accounts);
		free(c);
	}
	l_list_free(dummy_group->members);
	free(dummy_group);
}

void add_unknown_with_name( eb_account * ea, char * name )
{
	struct contact * con = add_new_contact(_("Unknown"), 
			(name && strlen(name))?name:ea->handle, ea->service_id);
	
	con->accounts = l_list_append( con->accounts, ea );
	ea->account_contact = con;
	ea->icon_handler = ea->status_handler = -1;
	if(find_suitable_local_account(NULL, ea->service_id))
		RUN_SERVICE(ea)->add_user(ea);
	else {
		contact_mgmt_queue_add(ea, MGMT_ADD, ea->account_contact->group->name);
	}	
	write_contact_list();
}

void add_unknown( eb_account * ea )
{
	add_unknown_with_name(ea, NULL);
}

static void handle_group_change(eb_account *ea, char *og, char *ng)
{
	/* if the groups are same, do nothing */
	if(!strcasecmp(ng, og))
		return;

	if (!find_suitable_local_account(NULL, ea->service_id)) {
		contact_mgmt_queue_add(ea, MGMT_MOV, ng);
	}
		
	/* adding to ignore */
	if(!strcmp(ng, _("Ignore")) && RUN_SERVICE(ea)->ignore_user)
		RUN_SERVICE(ea)->ignore_user(ea);

	/* remove from ignore */
	else if(!strcmp(og, _("Ignore")) && RUN_SERVICE(ea)->unignore_user)
		RUN_SERVICE(ea)->unignore_user(ea, ng);
		
	/* just your regular group change */
	else if(RUN_SERVICE(ea)->change_group)
		RUN_SERVICE(ea)->change_group(ea, ng);

}

struct contact * move_account (struct contact * con, eb_account *ea)
{
	struct contact *c = ea->account_contact;
	char * new_group = con->group->name;
	char *old_group = c->group->name;

	if (c != con) {

		handle_group_change(ea, old_group, new_group);

		c->accounts = l_list_remove(c->accounts, ea);

		remove_account_line(ea);
		if(l_list_empty(c->accounts)) {
			remove_contact(c);
			c=NULL;
		} else {
			LList *l;
			c->online = 0;
			for(l=c->accounts; l; l=l->next)
				if(((eb_account *)l->data)->online)
					c->online++;
			if(!c->online)
				remove_contact_line(c);
			else
				add_contact_and_accounts(c);
		}

		con->accounts = l_list_append(con->accounts, ea);

		ea->account_contact = con;
		if(ea->online) {
			con->online++;
			add_contact_line(con);
		}
	}
	
	add_contact_and_accounts(con);

	write_contact_list();

	return c;
}

void move_contact (char * group, struct contact * c)
{
	grouplist * g = c->group;
	struct contact *con;
	LList *l = c->accounts;
	
	if(!strcasecmp(g->name, group))
		return;

	g->members = l_list_remove(g->members, c);
	remove_contact_line(c);
	g = find_grouplist_by_name(group);
	
	if(!g) {
		add_group(group);
		g = find_grouplist_by_name(group);
	}
	add_group_line(g);

	rename_nick_log(c->group->name, c->nick, group, c->nick);
	
	for(; l; l=l->next) {
		eb_account *ea = l->data;
		handle_group_change(ea, c->group->name, group);
	}

	con = find_contact_in_group_by_nick(c->nick, g);
	if(con) {
		/* contact already exists in new group : 
		   move accounts for old contact to new one */
		while(c && (l=c->accounts)) {
			eb_account *ea = l->data;
			c=move_account(con,ea);
		}
		return; 
	} else {
		g->members = l_list_insert_sorted(g->members, c, contact_cmp);
		c->group = g;
		add_contact_and_accounts(c);
		add_contact_line(c);
	}
}

void rename_contact( struct contact * c, char *newname) 
{
	LList *l = NULL;
	struct contact *con;
	
	if(!c || !strcmp(c->nick, newname))
		return;

	eb_debug(DBG_CORE,"Renaming %s to %s\n",c->nick, newname);
	con = find_contact_in_group_by_nick(newname, c->group);
	if(con) {
		eb_debug(DBG_CORE,"found existing contact\n");
		rename_nick_log(c->group->name, c->nick, c->group->name, con->nick);
		while(c && (l=c->accounts)) {
			eb_account *ea = l->data;
			c=move_account(con,ea);
		}
		update_contact_list ();
		write_contact_list();
		
	} else {
		eb_debug(DBG_CORE,"no existing contact\n");

        	rename_nick_log(c->group->name, c->nick, c->group->name, newname);
        	strncpy(c->nick, newname, 254);
        	c->nick[254] = '\0';
		if (c->label)
			gtk_label_set_text(GTK_LABEL(c->label), newname);
		l = c->accounts;
		while(l) {
			eb_account *ea = l->data;

			if (RUN_SERVICE(ea)->change_user_name) {
				if (!find_suitable_local_account(NULL, ea->service_id))
					contact_mgmt_queue_add(ea, MGMT_REN, newname);
				else
					RUN_SERVICE(ea)->change_user_name(ea, newname);
			}

			l = l->next;
		}
	}
	update_contact_list ();
	write_contact_list();
}

typedef struct _invite_request
{
	eb_local_account * ela;
	void * id;
} invite_request;

static void process_invite( GtkWidget * widget, gpointer data )
{
	invite_request * invite = data;
	int result = (int)gtk_object_get_user_data( GTK_OBJECT(widget));

	if (result && RUN_SERVICE(invite->ela)->accept_invite)
		RUN_SERVICE(invite->ela)->accept_invite( invite->ela, invite->id );
	else if (RUN_SERVICE(invite->ela)->decline_invite)
		RUN_SERVICE(invite->ela)->decline_invite( invite->ela, invite->id);

	g_free(invite);
}


void invite_dialog( eb_local_account * ela, char * user, char * chat_room,
					void * id )
{
	char * message = g_strdup_printf(
			_("User %s wants to invite you to\n%s\nWould you like to accept?"),
			user, chat_room);
	invite_request * invite = g_new0( invite_request, 1 );

	invite->ela = ela;
	invite->id = id;
	do_dialog( message, _("Chat Invite"), process_invite, invite );
	g_free(message);
}

void make_safe_filename(char *buff, char *name, char *group)  {
	
	/* i'm pretty sure the only illegal character is '/', but maybe 
	 * there are others i'm forgetting */
	char *bad_chars="/";
	char *p;
	char holder[NAME_MAX], gholder[NAME_MAX];

	strncpy(holder, name, NAME_MAX);
	if (group!=NULL)
		strncpy(gholder, group, NAME_MAX);

	for (p=holder; *p; p++) {
		if ( strchr(bad_chars, *p) )
			*p='_';
	}
	if (group != NULL)
		for (p=gholder; *p; p++) {
			if ( strchr(bad_chars, *p) )
				*p='_';
		}

	g_snprintf(buff, NAME_MAX, "%slogs/%s%s%s", 
				config_dir, 
				(group!=NULL ? gholder:""),
				(group!=NULL ? "-":""),
				holder);
	eb_debug(DBG_CORE,"logfile: %s\n",buff);
}
					

int gtk_notebook_get_number_pages(GtkNotebook *notebook)
{
	int i;  
	for (i=0; gtk_notebook_get_nth_page(notebook, i); i++)
		;
	return i;
}

/* looks for lockfile, if found, returns the pid in the file */
/* If not, creates the file, writes our pid, and returns -1 */
pid_t create_lock_file(char* fname)
{
	pid_t ourpid = -1;
#ifndef __MINGW32__
	struct stat sbuff;
	FILE* f;

	if (stat(fname, &sbuff) != 0) { 
		/* file doesn't exist, so we're gonna open it to write out pid to it */
		ourpid = getpid();
		if ((f = fopen(fname, "a")) != NULL) {
			fprintf(f, "%d\n", ourpid);
			fclose(f);
			ourpid = -1;
		} else {
			ourpid = 0;		/* I guess could be considered an error condition */
						/* in that we couldn't create the lock file */
		}           
	} else {
		/* this means that the file exists */
		if ((f = fopen(fname, "r")) != NULL) {
			char data[64], data2[64];
			fscanf(f, "%d", &ourpid);
			fclose(f);
			snprintf(data, sizeof(data), "/proc/%d", ourpid);
			if(stat(data, &sbuff) != 0) {
				fprintf(stderr, _("deleting stale lock file\n"));
				unlink(fname); /*delete lock file and try again :) */
				return create_lock_file(fname);
			} else {
				FILE * fd = NULL;
				snprintf(data2, sizeof(data2), "%s/cmdline", data);
				fd = fopen(data2, "r");
				if(fd==NULL) {
					perror("fopen");
					printf("can't open %s\n",data2);
				}
				else {
					char cmd[1024];
					fgets(cmd, sizeof(cmd), fd);
					printf("registered PID is from %s\n",cmd);
					fclose(fd);
					if(cmd == NULL || strstr(cmd, "ayttm") == NULL) {
						fprintf(stderr, _("deleting stale lock file\n"));
						unlink(fname); /*delete lock file and try again :) */
						return create_lock_file(fname);
					}
				}
			}
		} else {
			/* couldn't open it... bizarre... allow the program to run anyway... heh */
			ourpid = -1;
		}
	}
#endif
	return ourpid;
}

void delete_lock_file(char* fname) 
{
#ifndef __MINGW32__
	unlink(fname);
#endif
}

void eb_generic_menu_function(void *add_button, void * userdata)
{
	menu_item_data *mid=userdata;
	ebmCallbackData *ecd=NULL;

	assert(userdata);
	ecd=mid->data;
	/* Check for valid data type */
	if(!ecd || !IS_ebmCallbackData(ecd)) {
		g_warning(_("Unexpected datatype passed to eb_generic_menu_function, ignoring call!"));
		return;
	}
	if(!mid->callback) {
		g_warning(_("No callback defined in call to eb_generic_menu_function, ignoring call!"));
		return;
	}
	eb_debug(DBG_CORE, "Calling callback\n");
	mid->callback(ecd);
}

/*
 * This function just gets a linked list of groups, this is handy
 * because we can then use it to populate the groups combo widget
 * with this information
 */

LList * get_groups()
{
	LList * node = NULL;
  	LList * newlist = NULL;
	node = groups;
	
	while(node) {
		newlist=l_list_append(newlist, ((grouplist *)node->data)->name);
		node=node->next;
	}
	
	return newlist;
}

void rename_nick_log(char *oldgroup, char *oldnick, char *newgroup, char *newnick)
{
	char oldnicklog[255], newnicklog[255], buff[NAME_MAX];
	FILE *test = NULL;
	make_safe_filename(buff, oldnick, oldgroup);
	strncpy(oldnicklog, buff, 255);

	make_safe_filename(buff, newnick, newgroup);
	strncpy(newnicklog, buff, 255);

	if (!strcmp(oldnicklog, newnicklog))
		return;

	if ((test = fopen(newnicklog,"r")) != NULL && strcmp(oldnicklog,newnicklog)) {
		FILE *oldfile = fopen(oldnicklog,"r");
		char read_buffer[4096];

		fclose(test);
		test = fopen(newnicklog,"a");

		if (oldfile) {
			while (fgets(read_buffer, 4096, oldfile) != NULL)
				fputs(read_buffer,test);

			fclose(oldfile);
		}
		fclose(test);
		unlink(oldnicklog);
		eb_debug(DBG_CORE,"Copied log from %s to %s\n", oldnicklog, newnicklog);
	}

	else if (strcmp(oldnicklog,newnicklog)) {
		rename(oldnicklog, newnicklog);
		eb_debug(DBG_CORE,"Renamed log from %s to %s\n", oldnicklog, newnicklog);
	}
}

eb_account *find_account_for_protocol(struct contact *c, int service) 
{
	LList *l = c->accounts;
	while (l && l->data) {
		eb_account *a = (eb_account *)l->data;
		if (a->service_id == service)
			return a;
		l=l->next;
	}
	return NULL;
}

GList * llist_to_glist(LList * ll, int free_old)
{
	GList * g = NULL;
	LList * l = ll;
	
	for(; l; l=l->next)
		g = g_list_append(g, l->data);

	if(free_old)
		l_list_free(ll);

	return g;
}

LList * glist_to_llist(GList * gl, int free_old)
{
	LList * l = NULL;
	GList * g = gl;
	
	for(; g; g=g->next)
		l = l_list_append(l, g->data);

	if(free_old)
		g_list_free(gl);

	return l;
}

int send_url(const char * url)
{
	LList *node;
	int ret = 0;
	for( node = accounts; node && !ret; node = node->next ) {
		eb_local_account * ela = node->data;
		if (CAN(ela, handle_url)) {
			ret = RUN_SERVICE(ela)->handle_url(url);
			eb_debug(DBG_CORE,"%s: %s handle this url\n",ela->handle,
					ret?"did":"didn't");
		} else {
			eb_debug(DBG_CORE,"%s: no handle of this url\n",ela->handle);
		}
		
	}
	return ret;
}

int eb_send_message (const char *to, const char *msg, int service)
{
  gint pos = 0;
  struct contact *con=NULL;
  eb_account *ac=NULL;
  
  if (service != -1)
	ac = find_account_by_handle(to, service);
  else
	con = find_contact_by_nick(to);
	  
  if (ac) {
	  con = ac->account_contact;
	  eb_chat_window_display_account(ac);
  } else if (con) {
	  eb_chat_window_display_contact(con);
  } else {
	  return 0;
  }
  gtk_editable_insert_text(GTK_EDITABLE(con->chatwindow->entry), msg?msg:"", msg?strlen(msg):0, &pos);
  /* send_message(NULL, con->chatwindow);*/
  return 1;
}
