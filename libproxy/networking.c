/*
 * Ayttm
 *
 * Copyright (C) 2009 the Ayttm team
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

/* this is a little piece of code to handle proxy connection */
/* it is intended to : 1st handle http proxy, using the CONNECT command
 , 2nd provide an easy way to add socks support */

#include "intl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef __MINGW32__
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include "networking.h"
#include "net_constants.h"
#include "proxy_private.h"
#include "common.h"
#include "ssl.h"

#include <glib.h>

#ifdef __MINGW32__
#define sleep(a)		Sleep(1000*a)

#define bcopy(a,b,c)	memcpy(b,a,c)	 
#define bzero(a,b)		memset(a,0,b)	 

#endif

/* The connection function for any proxy */
typedef int  ( *AyConnectionHandler )(int sockfd, const char *host, int port, gpointer con );

typedef struct {
	AyInputCallback callback;
	void *cb_data;
	AyConnection *connection;
	int tag;
} AyInputCallbackData;

static GSList *ay_input_callback_data_list = NULL;
static GSList *ay_active_attempts = NULL;

typedef struct {
	int sockfd;
	int listenfd;
#ifdef HAVE_OPENSSL
	SSL *ssl;
#endif
} AySource ;

typedef struct {
	AyConnection *con;
	int id;
} ActiveAttempt;

/* Private structure of an active connection */
struct _AyConnectionPrivate {

	/* Hostname */
	char			*host;		

	/* Port */
	int			 port;

	/* Proxy. NULL if the connection is direct */
	AyProxyData		*proxy;

	/* Connection Type See AyConnectionType for valid values */
	AyConnectionType	 type;

	/* A Source representing the file descriptor or SSL socket */
	AySource		 fd;

	/* Status of connection. See the AyConnectionStatus enum */		
	AyConnectionStatus	 status;
} ;


/* Used to establish a connection */
typedef struct {

	/* The Connection  I will eventually return */
	AyConnection		*connection;

	/* What we should call when connection is completed */
	AyConnectCallback	 callback;

	/* Update status in the requestor */
	AyStatusCallback	 status_callback;

	/* Check if certificate is to be accepted or not */
	AyCertVerifyCallback	 verify_callback;

	/* Return value from verify_callback */
	int 			 ret;

	/* Message for verify_callback */
	const char 		*message;

	/* Message Title for verify_callback */
	const char 		*title;

	/* Points to the appropriate connection function in case of proxy */
	AyConnectionHandler	 connect;

	/* What the requestor wants us to give back once we're done */
	gpointer		 cb_data;

	/* What the main thread uses to keep track of its thread */
	GAsyncQueue		*leash;

	/* So that someone can talk back as well */
	GAsyncQueue		*backleash;

	/* Tag for the polling idle function -- useful as a unique ID */
	guint			 source;

} _AyConnector ;


static gboolean check_connect_complete(gpointer data)
{
	_AyConnector *con = data;

	if( g_async_queue_try_pop(con->leash) ) {
		GSList *l = ay_active_attempts;
        
		if(con->connection->priv->status == AY_UI_REQUEST) {
			GAsyncQueue *backleash = con->backleash;

			con->ret = con->verify_callback(con->message, con->title);

			g_async_queue_ref(backleash);
			g_async_queue_push(backleash, con);
			g_async_queue_unref(backleash);

			return TRUE;
		}
		/* Clean up if there was an error or a cancellation */
		if (con->connection->priv->status != AY_NONE) {
			ay_connection_disconnect(con->connection);

		}

		con->callback(con->connection, con->connection->priv->status, con->cb_data);

		g_async_queue_unref(con->leash);

		g_free(con);

		/* Remove the connection attempt */
		while(l) {
			ActiveAttempt *a = l->data;
        
			if(a->con == con->connection) {
				ay_active_attempts = g_slist_remove(ay_active_attempts, a);
				g_free(a);
				break;
			}

			l = g_slist_next(l);
		}

		return FALSE;
	}
	
	return TRUE;
}


static gboolean ay_check_continue_connecting(_AyConnector *con, GAsyncQueue *leash)
{
	if(con->connection->priv->status == AY_CANCEL_CONNECT) {
		if(con->callback) {
			g_async_queue_push(leash, con);
			g_async_queue_unref(leash);
			g_async_queue_unref(con->backleash);
		}

		return FALSE;
	}

	return TRUE;
}


static gpointer _do_connect( gpointer data )
{
	int retval=0;
	_AyConnector *con = (_AyConnector *)data;
	char message[255];
	GAsyncQueue *leash = NULL;

	/* Tell the main thread that I have begun */
	if(con->callback) {
		g_async_queue_ref(con->leash);
		g_async_queue_ref(con->backleash);
		leash = con->leash;
		g_async_queue_push(con->leash, con);

		if( !ay_check_continue_connecting(con, leash) )
			return NULL;
	}

	if(con->status_callback != NULL) {
		snprintf(message, sizeof(message), _("Connecting to %s"), con->connection->priv->host);
		con->status_callback(message, con->cb_data);
	}

	/* If it's a server connection then just wait for a connection */
	if ( con->connection->priv->type == AY_CONNECTION_TYPE_SERVER ) {
		int newfd = 0;

		if ( (newfd = accept(con->connection->priv->fd.listenfd, NULL, 0)) < 0 ) {
			con->connection->priv->status = errno;
			con->connection->priv->fd.sockfd = -1;
		}
		else {
			con->connection->priv->fd.sockfd = newfd;
		}
	}
	/* Direct Connection */
	else if (!con->connect) {
		con->connection->priv->fd.sockfd = connect_address(con->connection->priv->host, con->connection->priv->port);

		if( !ay_check_continue_connecting(con, leash) )
			return NULL;

		if(con->connection->priv->fd.sockfd<0) {
			con->connection->priv->status = con->connection->priv->fd.sockfd;
			con->connection->priv->fd.sockfd = -1;
		}
	}
	/* Proxy */
	else {
		int proxyfd = 0;

		if(con->status_callback != NULL) {
			snprintf(message, sizeof(message), _("Connecting to proxy %s"), con->connection->priv->proxy->host);
			con->status_callback(message, con->cb_data);
		}

		/* First get a connection to the proxy */
		proxyfd = connect_address(con->connection->priv->proxy->host, con->connection->priv->proxy->port);

		if(proxyfd>=0) {
			if( !ay_check_continue_connecting(con, leash) ) {
				close(proxyfd);
				return NULL;
			}
        
			if(con->status_callback != NULL) {
				snprintf(message, sizeof(message), _("Connecting to %s"), con->connection->priv->proxy->host);
				con->status_callback(message, con->cb_data);
			}

			retval = con->connect(proxyfd,	con->connection->priv->host, 
							con->connection->priv->port, 
							con->connection->priv->proxy);

			if( !ay_check_continue_connecting(con, leash) ) {
				close(proxyfd);
				return NULL;
			}
        
			if(retval<0)
				con->connection->priv->status = retval;
			else
				con->connection->priv->fd.sockfd = proxyfd;
		}
		else {
			con->connection->priv->status = proxyfd;
			con->connection->priv->fd.sockfd = -1;
		}
	}

#ifdef HAVE_OPENSSL

	if(con->connection->priv->type == AY_CONNECTION_TYPE_SSL && con->connection->priv->fd.sockfd >= 0) {
		con->connection->priv->fd.ssl = ssl_get_socket(con->connection->priv->fd.sockfd, 
								con->connection->priv->host,
								con->connection->priv->port,
								con);

		if( con->connection->priv->fd.ssl == NULL)
			con->connection->priv->status = AY_SSL_CERT_REJECT;
	}

#endif

	if(con->callback) {
		fcntl(con->connection->priv->fd.sockfd, F_SETFL, O_NONBLOCK);

		g_async_queue_push(leash, con);
		g_async_queue_unref(leash);
		g_async_queue_unref(con->backleash);
	}

	return NULL;
}


gboolean ay_connection_verify(const char *message, const char *title, void *data)
{
	GAsyncQueue *leash ;
	_AyConnector *con = (_AyConnector *)data;

	int status = con->connection->priv->status;

	con->message = message;
	con->title = title;

	g_async_queue_ref(con->leash);
	leash = con->leash;

	con->connection->priv->status = AY_UI_REQUEST;

	g_async_queue_push(leash, con);
	g_async_queue_pop(con->backleash);
	con->connection->priv->status = status;
	g_async_queue_unref(leash);

	return con->ret;
}


AyConnection *ay_connection_new(const char *host, int port, AyConnectionType type)
{
	AyConnection *con = g_new0(AyConnection, 1);

	con->priv = g_new0(struct _AyConnectionPrivate, 1);

	con->priv->host = strdup(host);
	con->priv->port = port;
	con->priv->type = type;

	con->priv->fd.sockfd = -1;
	con->priv->fd.listenfd = -1;

	con->priv->proxy = default_proxy;

	return con;
}


void ay_connection_free(AyConnection *con)
{
	ay_connection_disconnect(con);
	GSList *l = ay_active_attempts;

	if(!con || !con->priv) {
		fprintf(stderr, "Something is wrong... trying to free a NULL connection "
				"(con=%p, priv=%p)\n", con, (con?con->priv:0x0));
		return;
	}

	while(l) {
		ActiveAttempt *a = l->data;

		if(a->con == con) {
			a->con->priv->status = AY_CANCEL_CONNECT;
			ay_active_attempts = g_slist_remove(ay_active_attempts, a);
			g_free(a);
			return;
		}

		l = g_slist_next(l);
	}

	if(con->priv->host)
		free(con->priv->host);

	g_free(con->priv);
	free(con);
}


/* Cancel a connection. Lets the thread know that we're no longer interested in its connection */
void ay_connection_cancel_connect(int tag)
{
	GSList *l = ay_active_attempts;

	while(l) {
		ActiveAttempt *a = l->data;

		if(a->id == tag) {
			a->con->priv->status = AY_CANCEL_CONNECT;
			ay_active_attempts = g_slist_remove(ay_active_attempts, a);
			g_free(a);
			return;
		}

		l = g_slist_next(l);
	}

}


int ay_connection_listen(AyConnection *con)
{
	struct sockaddr_in addr;

	con->priv->fd.listenfd = socket(AF_INET, SOCK_STREAM, 0);

	if (con->priv->fd.listenfd == -1 )
		return -1;

	memset (&addr, 0, sizeof(struct sockaddr_in));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(con->priv->port);

	/* Bind and listen to the socket */
	if(bind(   con->priv->fd.listenfd, (struct sockaddr *)(&addr), sizeof(addr)) < 0 
		|| listen(con->priv->fd.listenfd, 1) < 0 )
	{
		close(con->priv->fd.listenfd);
		return -1;
	}

	return 0;
}


int ay_connection_connect( AyConnection *con, AyConnectCallback cb, AyStatusCallback scb, 
				AyCertVerifyCallback vfy_cb, void *data )
{
	_AyConnector *connector = g_new0(_AyConnector, 1);

	connector->connection = con;
	connector->callback = cb;
	connector->cb_data = data;
	connector->status_callback = scb;
	connector->verify_callback = vfy_cb;

	if(!default_proxy || default_proxy->type == PROXY_NONE)
		connector->connect = NULL ;
	else if (default_proxy->type == PROXY_HTTP)
		connector->connect = (AyConnectionHandler)http_connect ;
	else if (default_proxy->type == PROXY_SOCKS4)
		connector->connect = (AyConnectionHandler)socks4_connect ;
	else if (default_proxy->type == PROXY_SOCKS5)
		connector->connect = (AyConnectionHandler)socks5_connect ;
	else {
		printf("Invalid proxy\n");
		return AY_INVALID_CONNECT_DATA;
	}

	if(cb) {
		ActiveAttempt *attempt = g_new0(ActiveAttempt, 1);

		ay_active_attempts = g_slist_append(ay_active_attempts, attempt);

		connector->leash  = g_async_queue_new ();
		connector->backleash = g_async_queue_new ();

		g_thread_create(_do_connect, connector, FALSE, NULL);

		/* Wait till the connector thread starts */
		g_async_queue_pop(connector->leash);

		attempt->con = con;
		attempt->id = connector->source = g_idle_add(check_connect_complete, connector);

		return connector->source;
	}
	else {
		_do_connect(connector);

		if (con->priv->status)
			return con->priv->status;
		else
			return 0;
	}
}


void ay_connection_disconnect(AyConnection *con)
{
	if (!con || !con->priv)
		return;

#ifdef HAVE_OPENSSL
	if ( con->priv->fd.ssl ) {
		ssl_done_socket(con->priv->fd.ssl);
		con->priv->fd.ssl = NULL;
	}
#endif

	if ( con->priv->fd.sockfd >= 0 ) {
		close(con->priv->fd.sockfd);
		con->priv->fd.sockfd = -1;
	}

	if ( con->priv->fd.listenfd >= 0 ) {
		close(con->priv->fd.listenfd);
		con->priv->fd.listenfd = -1;
	}
}


int ay_connection_read(AyConnection *con, void *buf, int len)
{
	if (!con || con->priv->fd.sockfd < 0)
		return -1;

#ifdef HAVE_OPENSSL
	if ( con->priv->type == AY_CONNECTION_TYPE_SSL )
		return ssl_read(con->priv->fd.ssl, buf, len);
	else
#endif
		return read(con->priv->fd.sockfd, buf, len);
}


int ay_connection_write(AyConnection *con, const void *buf, int len)
{
	if (!con || con->priv->fd.sockfd < 0)
		return -1;

#ifdef HAVE_OPENSSL
	if ( con->priv->type == AY_CONNECTION_TYPE_SSL )
		return ssl_write(con->priv->fd.ssl, buf, len);
	else
#endif
		return write(con->priv->fd.sockfd, buf, len);
}


gboolean ay_connection_is_active(AyConnection *con)
{
	if(con && con->priv && con->priv->fd.sockfd >=0)
		return TRUE;
	else
		return FALSE;
}


static void ay_connection_ready(void *data, int fd, eb_input_condition cond)
{
	AyInputCallbackData *cb_data = data;

	cb_data->callback(cb_data->connection, cond, cb_data->cb_data);
}


int ay_connection_input_add(AyConnection *con, eb_input_condition cond, AyInputCallback func, void *cb_data)
{
	AyInputCallbackData *data = g_new0(AyInputCallbackData, 1);

	if(!con || !con->priv || con->priv->fd.sockfd == -1)
		return -1;

	data->callback = func;
	data->connection = con;
	data->cb_data = cb_data;
	
	if( con->priv->type == AY_CONNECTION_TYPE_SERVER && con->priv->fd.sockfd < 0 )
		data->tag = eb_input_add(con->priv->fd.listenfd, cond, ay_connection_ready, data);
	else
		data->tag = eb_input_add(con->priv->fd.sockfd, cond, ay_connection_ready, data);

	ay_input_callback_data_list = g_slist_append(ay_input_callback_data_list, data);

	return data->tag;
}


void ay_connection_input_remove(int tag)
{
	GSList *input = ay_input_callback_data_list;

	while(input) {
		if (((AyInputCallbackData *)input->data)->tag == tag) {
			eb_input_remove(tag);
			void *tmp = input->data;
			ay_input_callback_data_list = g_slist_remove(ay_input_callback_data_list, tmp);
			g_free(tmp);

			return;
		}

		input = g_slist_next(input);
	}
}


/* Wow!! A new deprecated function! */
void ay_connection_free_no_close(AyConnection *con)
{
	if(!con || !con->priv) {
		fprintf(stderr, "Something is wrong... trying to free a NULL connection "
				"(con=%p, priv=%p)\n", con, (con?con->priv:0x0));
		return;
	}

	if(con->priv->host)
		free(con->priv->host);

	g_free(con->priv);
	free(con);
}


/* ... and another one! */

int ay_connection_get_fd(AyConnection *con)
{
	return con->priv->fd.sockfd;
}


AyConnectionType ay_connection_get_type(AyConnection *con)
{
	return con->priv->type;
}


static char *errors[] = 
	{
		"Success",
		"Connection Refused",
		"Hostname Lookup failed",
		"Permission denied",
		"Proxy requires Authentication",
		"Socks4: Disconnected due to an unknown error",
		"Socks4: Authentication failed",
		"Socks4: Invalid username",
		"Socks4: Incompatible",
		"Socks5: Connection failed",
		"Invalid Connection information",
		"Connection cancelled",
		"Certificate Rejected",
		"Interrupt. You should not be getting this request. File a bug report",
		"Connection failed"
	};


const char *ay_connection_strerror(int error)
{
	if(error <=0 && error > AY_NUM_ERRORS)
		return errors[0-error];

	return _("Unknown Error");
}


