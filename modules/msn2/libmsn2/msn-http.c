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

#include <stdio.h>
#include <string.h>

#include "msn-http.h"
#include "msn-connection.h"
#include "msn-util.h"
#include "msn-ext.h"

#include "llist.h"

typedef void (*MsnHttpConnectCallback) (MsnConnection *mc);

typedef struct {
	MsnConnection *mc;
	MsnHttpCallback callback;
	MsnHttpConnectCallback connect_callback;
	char *path;
	char *params;
	char *request;
	char *soap_action;
	int cleaned;
	void *cb_data;
} HttpData;

static LList *http_connection_list = NULL;


static int http_mc_compare(const void *data, const void *comparison)
{
	if( ((HttpData *)data)->mc == comparison )
		return 0;
	
	return 1;
}


int msn_http_got_response(MsnConnection *mc, int len)
{
	char *offset;

	LList *l = l_list_find_custom(http_connection_list, mc, http_mc_compare);

	HttpData *data = (HttpData *)l->data;

	if(mc->current_message->size == 0 && (offset = strstr(mc->current_message->payload, "Content-Length: "))) {
		char *end;

		offset += 16;

		end = strchr(offset, '\r');
		*end = '\0';

		mc->current_message->size = atoi(offset);
		*end = '\r';
	}

	/* The data has started, so adjust the current capacity and message content accordingly */
	if(!data->cleaned && (offset = strstr(mc->current_message->payload, "\r\n\r\n"))) {
		char *temp = strdup(offset+4);

		mc->current_message->capacity = strlen(temp)+1;
		free(mc->current_message->payload);
		mc->current_message->payload = temp;

		data->cleaned = 1;
	}

	/* We're not done yet */
	if(len >0 && (!data->cleaned || mc->current_message->size > strlen(mc->current_message->payload))) {
		return 0;
	}

	data->callback(mc->account, mc->current_message->payload, 
			mc->current_message->size?mc->current_message->size:strlen(mc->current_message->payload), data->cb_data);

	mc->account->connections = l_list_remove(mc->account->connections, mc);

	http_connection_list = l_list_remove(http_connection_list, data);

	free(data->params);
	free(data->soap_action);
	free(data->request);
	free(data->path);

	free(data);

	msn_connection_free(mc);

	return 1;
}


static void http_post_connected(MsnConnection *mc)
{
	char buf[4096];
	char soap_action[64];

	LList *l = l_list_find_custom(http_connection_list, mc, http_mc_compare);

	HttpData *data = (HttpData *)l->data;


	if(data->soap_action) {
		sprintf(soap_action, "SOAPAction: %s\r\n", data->soap_action);
	}
	else {
		soap_action[0] = '\0';
	}

	snprintf(buf, sizeof(buf), 
		"POST %s HTTP/1.1\r\n"
		"%s"
/*		"Accept: text/ *\r\n"*/
		"Content-Type: text/xml; charset=utf-8\r\n"
		"User-Agent: Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1)\r\n"
		"Host: %s\r\n"
		"Content-Length: %d\r\n"
		"%s"
		"\r\n"
		"%s",
		data->path,
		soap_action,
		mc->host,
		(int)(strlen(data->request)),
		data->params,
		data->request
		);

	msn_connection_send_data(mc, buf, strlen(buf));
}

static void http_get_connected(MsnConnection *mc)
{
	char buf[4096];
	char soap_action[64];

	LList *l = l_list_find_custom(http_connection_list, mc, http_mc_compare);

	HttpData *data = (HttpData *)l->data;

	if(data->soap_action) {
		sprintf(soap_action, "SOAPAction: %s\r\n", data->soap_action);
	}
	else {
		soap_action[0] = '\0';
	}

	snprintf(buf, sizeof(buf), 
		"GET %s HTTP/1.1\r\n"
		"%s"
		"Accept: text/*\r\n"
		"User-Agent: Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1)\r\n"
		"Host: %s\r\n"
		"%s"
		"\r\n",
		data->path,
		soap_action,
		mc->host,
		data->params
		);
	
	msn_connection_send_data(mc, buf, strlen(buf));
}

static void http_connect(MsnAccount *ma, char *host, int port, int use_ssl, HttpData *data)
{
	MsnConnection *mc = msn_connection_new() ;

	mc->host = host; /* So that it is freed when mc is freed. Sneaky ain't I ;) */
	mc->port = port;
	mc->use_ssl = use_ssl;

	mc->type = MSN_CONNECTION_HTTP;
	mc->account = ma;
	ma->connections = l_list_append(ma->connections, mc);

	data->mc = mc;

	http_connection_list = l_list_prepend(http_connection_list, data);

	msn_connection_connect(mc, data->connect_callback);
}


void msn_http_request(MsnAccount *ma, MsnRequestType type, char *soap_action, char *url, 
			char *request, MsnHttpCallback callback, char *params, void *cb_data)
{
	char *host, *path;
	int port;
	int use_ssl=0;

	char *curiter = NULL, *saveiter = NULL;

	HttpData *data = m_new0(HttpData, 1);

	curiter = strstr(url, "http");

	if(*(curiter+4) == 's') {
		port = 443;
		use_ssl = 1;
	}
	else {
		port = 80;
	}

	curiter = strstr(curiter, "//");
	curiter+=2;
	saveiter = curiter;

	curiter = strchr(curiter, '/');

	if(curiter) {
		*curiter = '\0';
		host = strdup(saveiter);

		*curiter = '/';
	
		path = strdup(curiter);
	}
	else {
		host = strdup(saveiter);
		path = strdup("/");
	}

	if( (saveiter = strchr(host, ':')) ) {
		port = atoi(saveiter+1);
		*saveiter = '\0';
	}

	data->callback = callback;
	data->path = path;
	data->params = (params?strdup(params):strdup(""));
	data->soap_action = (soap_action?strdup(soap_action):NULL);
	data->cb_data = cb_data;

	if(type == MSN_HTTP_GET) {
		data->connect_callback = http_get_connected;
	}
	else {
		data->request = (request?strdup(request):strdup(""));
		data->connect_callback = http_post_connected;
	}

	http_connect(ma, host, port, use_ssl, data);
}


