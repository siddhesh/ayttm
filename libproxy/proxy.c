/*
 * Ayttm
 *
 * Copyright (C) 2003, 2009 the Ayttm team
 * 
 * Ayttm is a derivative of Everybuddy
 * Copyright (C) 1998-1999, Torrey Searle
 * proxy featured by Seb C.
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

#include "libproxy.h"
#include "tcp_util.h"
#include "messages.h"


#ifdef __MINGW32__
#define sleep(a)		Sleep(1000*a)

#define bcopy(a,b,c)	memcpy(b,a,c)	 
#define bzero(a,b)		memset(a,0,b)	 

#define ECONNREFUSED	WSAECONNREFUSED	 
#endif

static ay_proxy_data *default_proxy = NULL;
static GSList *ay_active_connections = NULL;

/* Prototypes */
static char *encode_proxy_auth_str( ay_proxy_data *proxy );
static gpointer _do_connect( gpointer data );


#define debug_print printf


static struct addrinfo *lookup_address(const char *host, int port_num, int family)
{
	struct addrinfo hints;
	struct addrinfo *result=NULL;
	char port[10];

	memset(&hints, 0, sizeof(struct addrinfo));

	hints.ai_family = family;
	hints.ai_socktype = SOCK_STREAM;

	snprintf(port, sizeof(port), "%d", port_num);

	if ( getaddrinfo(host, port, &hints, &result) < 0 ) {
		debug_print("unable to create socket\n");
	}
	else {
		return result;
	}

	return NULL;
}


static int connect_address( const char *host, int port_num )
{
	int fd;
	struct addrinfo hints;
	struct addrinfo *result=NULL, *rp=NULL;
	char port[10];

	memset(&hints, 0, sizeof(struct addrinfo));

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	snprintf(port, sizeof(port), "%d", port_num);

	if ( getaddrinfo(host, port, &hints, &result) < 0 ) {
		debug_print("unable to create socket\n");
	}
	else {
		for( rp = result; rp; rp = rp->ai_next ) {
			fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

			if (fd == -1)
				continue;

			if (connect(fd, rp->ai_addr, rp->ai_addrlen) != -1) {
				sleep(2);  /* Anyone know why we sleep here? */
				
				freeaddrinfo(result);
				return fd;
			}
			else
				perror("connect:" );
		}
	}

	if(result)
		freeaddrinfo(result);

	return -1;
}


/* external function to use to set the proxy settings */
int ay_set_default_proxy( int type, const char *host, int port, char *username, char *password)
{
	if (!default_proxy)
		default_proxy = g_new0(ay_proxy_data, 1);

	default_proxy->type=type;

	if (type == PROXY_NONE) {
		if(default_proxy->host)
			free(default_proxy->host);

		if(default_proxy->username)
			free(default_proxy->username);

		if(default_proxy->password)
			free(default_proxy->password);

		g_free(default_proxy);
		default_proxy = NULL;
	}
	else {
		default_proxy->port=0;
	
		if (host != NULL && host[0])  {
			default_proxy->host=strdup(host);
			default_proxy->port=port;
		}
		if (default_proxy->port == 0)
			default_proxy->port=3128;

		if(username && username[0])
			default_proxy->username = strdup(username);

		if(password && password[0])
			default_proxy->password = strdup(password);

	}
#ifdef __MINGW32__
	{
		WSADATA wsaData;
		WSAStartup(MAKEWORD(2,0),&wsaData);
	}
#endif
	return(0);
}


/* this code is borrowed from cvs 1.10 */
static int	proxy_recv_line( int sock, char **resultp )
{
	int c;
	char *result;
	size_t input_index = 0;
	size_t result_size = 80;

	result = (char *) malloc (result_size);

	while (1)
	{
		char ch;
		if (recv (sock, &ch, 1, 0) < 0) {
			fprintf (stderr, "recv() error from  proxy server\n");
			return -1;
		}
		c = ch;

		if (c == EOF)
		{
			free (result);
        
			/* It's end of file.  */
			fprintf(stderr, "end of file from  server\n");
		}
        
		if (c == '\012')
			break;
        
		result[input_index++] = c;
		while (input_index + 1 >= result_size)
		{
			result_size *= 2;
			result = (char *) realloc (result, result_size);
		}
	}

	if (resultp)
		*resultp = result;

	/* Terminate it just for kicks, but we *can* deal with embedded NULs.  */
	result[input_index] = '\0';

	if (resultp == NULL)
		free (result);

	return input_index;
}


/* http://archive.socks.permeo.com/protocol/socks4.protocol */
static int socks4_connect( int sock, struct sockaddr *serv_addr, int addrlen, ay_connection *con )
{
	int i, packetlen;

	unsigned char *packet = NULL;
	struct addrinfo *result = NULL;

	int retval = 0;


	if(con->proxy->username && con->proxy->username[0])
		packetlen = 9 + strlen(con->proxy->username);
	else
		packetlen = 9;

	result = lookup_address(con->host, con->port, AF_UNSPEC);

	if(!result)
	       return AY_HOSTNAME_LOOKUP_FAIL;

	packet = (unsigned char *)calloc(packetlen, sizeof(unsigned char));

	packet[0] = 4;						/* Version */
	packet[1] = 1;						/* CONNECT  */
	packet[2] = (((unsigned short) con->port) >> 8);	/* DESTPORT */
	packet[3] = (((unsigned short) con->port) & 0xff);	/* DESTPORT */

	/* DESTIP */
	bcopy(packet+4, &(((struct sockaddr_in *)result->ai_addr)->sin_addr), 4 );

	freeaddrinfo(result);

	if(con->proxy->username && con->proxy->username[0]) {
	        for (i=0; con->proxy->username[i]; i++) {
	      	  packet[i+8] = (unsigned char)con->proxy->username[i];  /* AUTH	 */
	        }
	}
	packet[packetlen-1] = 0;							  /* END	  */
	debug_print("Sending \"%s\"\n", packet);
	if (write(sock, packet, packetlen) == packetlen)
	{
		 bzero(packet, sizeof(packet));
		 /* Check response - return as SOCKS4 if its valid */
		if (read(sock, packet, 9)>=4)
		{
			if (packet[1] == 90) {
//				if(proxy_auth)
//					g_free(proxy_auth);
				return 0;
			}
			else if(packet[1] == 91)
				retval = AY_SOCKS4_UNKNOWN;
//					ay_do_error( _("Proxy Error"), 
//							_("Socks 4 proxy rejected request for an unknown reason.") );
			else if(packet[1] == 92)
				retval = AY_SOCKS4_IDENTD_FAIL;
//					ay_do_error( _("Proxy Error"), 
//							_("Socks 4 proxy rejected request because it could not connect to our identd.") );
			else if(packet[1] == 93)
				retval = AY_SOCKS4_IDENT_USER_DIFF;
//					ay_do_error( _("Proxy Error"), 
//							_("Socks 4 proxy rejected request because identd returned a different userid.") );
			else {
				retval = AY_SOCKS4_INCOMPATIBLE_ERROR;
//					ay_do_error( _("Proxy Error"), 
//							_("Socks 4 proxy rejected request with an RFC-uncompliant error cod.") );	
				printf("=>>%d\n",packet[1]);
			}			
		} else {
			printf("short read %s\n",packet);
		}	
	}
	close(sock);
	
//	if(proxy_auth)
//		g_free(proxy_auth);

	return retval;
}

/* http://archive.socks.permeo.com/rfc/rfc1928.txt */
/* http://archive.socks.permeo.com/rfc/rfc1929.txt */

/* 
 * Removed support for datagram connections because we're not even using it now. 
 * I'll add it back if/when it is needed or if I feel like being very correct 
 * some time later...
 */
static int socks5_connect( int sockfd, ay_connection *con )
{
	int i;
	char buff[530];
	int need_auth = 0;
	struct addrinfo *result = NULL;
	int j;	


	buff[0] = 0x05;  //use socks v5
	if(con->proxy->username && con->proxy->username[0]) {
		buff[1] = 0x02;  //we support (no authentication & username/pass)
		buff[2] = 0x00;  //we support the method type "no authentication"
		buff[3] = 0x02;  //we support the method type "username/passw"
		need_auth=1;
	} else {
		buff[1] = 0x01;  //we support (no authentication)
		buff[2] = 0x00;  //we support the method type "no authentication"
	}	 


	write( sockfd, buff, 3+((con->proxy->username && con->proxy->username[0])?1:0) );
	
	if ( read( sockfd, buff, 2 ) < 0 ) {
		close(sockfd);
		return AY_SOCKS5_CONNECT_FAIL;
	}

//	printf("buff[] %d %d proxy_user %s\n",buff[0],buff[1], con->proxy->username);
	if( buff[1] == 0x00 )
		need_auth = 0;
	else if (buff[1] == 0x02 && con->proxy->username && con->proxy->username[0])
		need_auth = 1;
	else {
		fprintf(stderr, "No Acceptable Methods");
		return AY_SOCKS5_CONNECT_FAIL;
	}
//	printf("need_auth=%d\n",((con->proxy->username && con->proxy->username[0])?1:0));
	if (((con->proxy->username && con->proxy->username[0])?1:0)) {
		/* subneg start */
		buff[0] = 0x01; 		/* subneg version  */
		printf("[%d]",buff[0]);
		buff[1] = strlen(con->proxy->username);	/* username length */
		printf("[%d]",buff[1]);
		for (i=0; con->proxy->username[i] && i<255; i++) {
			buff[i+2] = con->proxy->username[i];  /* AUTH	 */
			printf("%c",buff[i+2]);
		}
		i+=2;
		buff[i] = strlen(con->proxy->password);
		printf("[%d]",buff[i]);
		i++;
		for (j=0; j < con->proxy->password[j] && j<255; j++) {
			buff[i+j] = con->proxy->password[j];  /* AUTH	 */
			printf("%c",buff[i+j]);
		}
		i+=(j);
		buff[i]=0;

		write( sockfd, buff, i );

		if ( read( sockfd, buff, 2 ) < 0 ) {
			close(sockfd);
			return AY_SOCKS5_CONNECT_FAIL;
		}

		if (buff[1] != 0) {
//			ay_do_error( _("Proxy Error"), _("Socks5 proxy refused our authentication.") );
			return AY_PROXY_PERMISSION_DENIED;
		}
	}
	
	buff[0] = 0x05; //use socks5
	buff[1] = 0x01; //connect only SOCK_STREAM for now
	buff[2] = 0x00; //reserved
	buff[3] = 0x01; //ipv4 address

	if ( ( result = lookup_address(con->host, con->port, AF_UNSPEC) ) == NULL )
		return AY_HOSTNAME_LOOKUP_FAIL;

	memcpy(buff+4, &(((struct sockaddr_in *)result->ai_addr)->sin_addr),4 );
	memcpy((buff+8), &(((struct sockaddr_in *)result->ai_addr)->sin_port), 2);

	freeaddrinfo(result);

	write( sockfd, buff, 10 );

	if ( read( sockfd, buff, 10 ) < 0 ) {
		close(sockfd);
		return AY_SOCKS5_CONNECT_FAIL;
	}


	if( buff[1] != 0x00 ) {
		for( i = 0; i < 8; i++ )
			printf("%03d ", buff[i] );

		printf("%d", ntohs(*(unsigned short *)&buff[8]));
		printf("\n");
		fprintf(stderr, "SOCKS error number %d\n", buff[1] );
		close(sockfd);
		return AY_CONNECTION_REFUSED;
	}

	return AY_NONE;
}

static int http_connect( int sockfd, ay_connection *con )
{
	/* step two : do  proxy tunneling init */
	char cmd[200];
	char *inputline = NULL;
	char *proxy_auth = NULL;
	char debug_buff[255];
	
	sprintf(cmd,"CONNECT %s:%d HTTP/1.1\r\n",con->host,con->port);
	if ( con->proxy->username && con->proxy->username[0] ) {
		proxy_auth = encode_proxy_auth_str(con->proxy);

		strcat( cmd, "Proxy-Authorization: Basic " );
		strcat( cmd, proxy_auth );
		strcat( cmd, "\r\n" );
	}
	strcat( cmd, "\r\n" );
#ifndef DEBUG
	sprintf(debug_buff,"<%s>\n",cmd);
	debug_print(debug_buff);
#endif
	if (send(sockfd,cmd,strlen(cmd),0)<0)
		return AY_CONNECTION_REFUSED;
	if (proxy_recv_line(sockfd,&inputline) < 0)
		return AY_CONNECTION_REFUSED;
#ifndef DEBUG
	sprintf(debug_buff,"<%s>\n",inputline);
	debug_print(debug_buff);
#endif
	if (!strstr(inputline, "200")) {
		/* Check if proxy authorization needed */
		if ( strstr( inputline, "407" ) ) {
			while(proxy_recv_line(sockfd,&inputline) > 0) {
				free(inputline);
			}
//			ay_do_error( _("Proxy Error"), _("HTTP proxy error: Authentication required.") );
			return AY_PROXY_AUTH_REQUIRED;
		}
		if ( strstr( inputline, "403" ) ) {
			while(proxy_recv_line(sockfd,&inputline) > 0) {
				free(inputline);
			}
//			ay_do_error( _("Proxy Error"), _("HTTP proxy error: permission denied.") );
			return AY_PROXY_PERMISSION_DENIED;
		}
		free(inputline);
		return AY_CONNECTION_REFUSED;
	}

	while (strlen(inputline)>1) {
		free(inputline);
		if (proxy_recv_line(sockfd,&inputline) < 0) {
			return AY_CONNECTION_REFUSED;
		}
#ifndef DEBUG
		sprintf(debug_buff,"<%s>\n",inputline);
		debug_print(debug_buff);
#endif
	}
	free(inputline);

	g_free(proxy_auth);
			
	return 0;
}


static gboolean check_connect_complete(gpointer data)
{
	ay_connection *con = data;

	if( g_async_queue_try_pop(con->leash) ) {
		/* Execute callback only if the connection was not cancelled */
		if (con->status != AY_CANCEL_CONNECT) {
			if (con->status != AY_NONE)
				con->sockfd = -1;

			con->callback(con->sockfd, con->status, con->cb_data);
		}
		else if ( con->status == AY_CANCEL_CONNECT && con->sockfd >= 0 )
			close(con->sockfd);

		ay_active_connections = g_slist_remove(ay_active_connections, con);

		g_async_queue_unref(con->leash);
		free(con->host);

		g_free(con);

		return FALSE;
	}
	
	return TRUE;
}


static int ay_compare_connections(ay_connection *con1, ay_connection *con2)
{
	if( con1->source == con2->source )
		return 0;

	return 1;
}


/* Cancel a connection. Lets the thread know that we're no longer interested in its connection */
void ay_cancel_connect(guint source)
{
	GSList *element = NULL;
	ay_connection con;
	char message [255];

	con.source = source;

	element = g_slist_find_custom(ay_active_connections, &con, (GCompareFunc)ay_compare_connections);

	if(element !=NULL) {
		((ay_connection *)element->data)->status = AY_CANCEL_CONNECT;

		if( ((ay_connection *)element->data)->status_callback != NULL ) {
			snprintf(message, sizeof(message), _("Cancelling connection"));
			((ay_connection *)element->data)->status_callback(message, ((ay_connection *)element->data)->cb_data);
		}
	}
}


int ay_connect_host( const char *host, int port, void *cb, void *data, void *scb )
{
	ay_connection *con = g_new0(ay_connection, 1);

	if (cb)
		ay_active_connections = g_slist_append(ay_active_connections, con);

	con->host = strdup(host);
	con->port = port;
	con->callback = cb;
	con->cb_data = data;
	con->status_callback = scb;

	/* This could be extended later to have separate proxies for different connections */
	con->proxy = default_proxy;

	if(!default_proxy || default_proxy->type == PROXY_NONE)
		con->connect = NULL ;
	else if (default_proxy->type == PROXY_HTTP)
		con->connect = (ay_connect_handler)http_connect ;
	else if (default_proxy->type == PROXY_SOCKS4)
		con->connect = (ay_connect_handler)socks4_connect ;
	else if (default_proxy->type == PROXY_SOCKS5)
		con->connect = (ay_connect_handler)socks5_connect ;
	else {
		debug_print("Invalid proxy\n");
		return AY_INVALID_CONNECT_DATA;
	}

	if(cb) {
		con->leash  = g_async_queue_new ();

		g_thread_create(_do_connect, con, FALSE, NULL);

		/* Wait till the connector thread starts */
		g_async_queue_pop(con->leash);

		con->source = g_idle_add(check_connect_complete, con);

		return con->source;
	}
	else {
		int sockfd;

		con = _do_connect(con);

		sockfd = con->sockfd;

		ay_active_connections = g_slist_remove(ay_active_connections, con);
		free(con->host);
		g_free(con);

		return sockfd;
	}
}


static gboolean ay_check_continue_connecting(ay_connection *con, GAsyncQueue *leash)
{
	if(con->status == AY_CANCEL_CONNECT) {
		if(con->sockfd)
			close(con->sockfd);

		con->sockfd = -1;

		if(con->callback) {
			g_async_queue_push(leash, con);
			g_async_queue_unref(leash);
		}

		return FALSE;
	}

	return TRUE;
}


static gpointer _do_connect( gpointer data )
{
	int retval=0;
	ay_connection *con = (ay_connection *)data;
	char message[255];
	GAsyncQueue *leash = NULL;

	/* Tell the main thread that I have begun */
	if(con->callback) {
		g_async_queue_ref(con->leash);
		leash = con->leash;
		g_async_queue_push(con->leash, con);
	}

	if( !ay_check_continue_connecting(con, leash) )
		return con;

	if(con->status_callback != NULL) {
		snprintf(message, sizeof(message), _("Connecting to %s"), con->host);
		con->status_callback(message, con->cb_data);
	}

	if (!con->connect) {
		con->sockfd = connect_address(con->host, con->port);

		if( !ay_check_continue_connecting(con, leash) )
			return con;

		if(con->sockfd<0)
			con->status = errno;
	}
	else {
		int proxyfd = 0;

		if(con->status_callback != NULL) {
			snprintf(message, sizeof(message), _("Connecting to proxy %s"), con->proxy->host);
			con->status_callback(message, con->cb_data);
		}

		/* First get a connection to the proxy */
		proxyfd = connect_address(con->proxy->host, con->proxy->port);

		if(proxyfd>=0) {
			if( !ay_check_continue_connecting(con, leash) ) {
				close(proxyfd);
				return con;
			}
        
			if(con->status_callback != NULL) {
				snprintf(message, sizeof(message), _("Connecting to %s"), con->proxy->host);
				con->status_callback(message, con->cb_data);
			}

			retval = con->connect(proxyfd, con);

			if( !ay_check_continue_connecting(con, leash) ) {
				close(proxyfd);
				return con;
			}
        
			if(retval<0)
				con->status = retval;
			else
				con->sockfd = proxyfd;
		}
		else {
			con->status = errno;
		}
	}

	if(con->callback) {
		fcntl(con->sockfd, F_SETFL, O_NONBLOCK);

		g_async_queue_push(leash, con);
		g_async_queue_unref(leash);
	}

	return con;
}


static char *encode_proxy_auth_str( ay_proxy_data *proxy )
{
	char buff[200];

	if ( proxy->username == NULL )
		return NULL;

	strcpy( buff, proxy->username );
	strcat( buff, ":" );
	strcat( buff, proxy->password );

	return g_base64_encode( (unsigned char *)buff, strlen( buff ) );
}


