/*
 * Ayttm
 *
 * Copyright (C) 2003, the Ayttm team
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
 
static int		proxy_inited = 0;
static char		*proxy_realhost = NULL;
static char		debug_buff[256];

static int		proxy_type = PROXY_NONE;
static char		*proxy_host = NULL;
static int		proxy_port = 3128;

static int		proxy_auth_required = 0;
static char		*proxy_auth = NULL;
static char		*proxy_user = NULL;
static char		*proxy_password = NULL;

/* Prototypes */
static char *encode_proxy_auth_str( const char *user, const char *passwd );

static int ap_base64encode( char *encoded, const char *string, int len );


#define debug_print printf

#ifndef HAVE_GETHOSTBYNAME2
#define gethostbyname2(x, y) gethostbyname(x)
#endif

typedef struct _udp_connection
{
	int fd;
	struct sockaddr * host;
	struct _udp_connection * next;
} udp_connection;

static udp_connection	*server_list = NULL;

static struct sockaddr *get_server( int fd )
{
	udp_connection * node = NULL;
	printf("looking for connection matching %d", fd);
	for ( node = server_list; node; node = node->next )
	{
		if(fd == node->fd )
		{
			printf("match found\n");
			return node->host;
		}
	}
	printf("no match found\n");
	return NULL;
}

static int connect_address( unsigned int addy, unsigned short port )
{
    int fd;
    struct sockaddr_in sin;

    sin.sin_addr.s_addr = addy;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);

    fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd > -1)
    {
        if (connect(fd, (struct sockaddr *)&sin, sizeof(sin)) > -1)
        {
		sleep(2);
        	return fd;
        }
		else
		{
			perror("connect:" );
		}

    }
	else
	{
		printf("unable to create socket\n");
	}
    return -1;
}

/* Call this function if you want to free memory when leaving */
static void proxy_freemem( void )
{
    /* dummy function to avoid possibly memory leak */
    if (proxy_host != NULL)
        free(proxy_host);
    if (proxy_realhost != NULL)
        free(proxy_realhost);
	if ( proxy_auth != NULL )
		free( proxy_auth );

    return;
}

/* this function is useless if you do use an external method to set 
   the proxy setting, but it does yet allow to test the code without 
   having to code the interface */
static void proxy_autoinit( void )
{
    if (getenv("HTTPPROXY") != NULL) {
        proxy_host=(char *)strdup(getenv("HTTPPROXY"));
        if (proxy_host != NULL) {
            proxy_port=atoi(getenv("HTTPPROXYPORT"));
	    proxy_type=PROXY_HTTP;
            if (!proxy_port)
                proxy_port=3128;

	    if ( getenv( "HTTPPROXYUSER" ) ) {
		char *proxy_user;
		char *proxy_password;

		proxy_user = getenv( "HTTPPROXYUSER" );
		proxy_password = getenv( "HTTPPROXYPASSWORD" );

		proxy_auth = encode_proxy_auth_str(proxy_user, proxy_password);
	    }
#ifdef HAVE_ATEXIT
	    /* if you do not have atexit, it means that if you don't call
	       proxy_freemem when leaving, you'll have mem leaks */
            atexit(proxy_freemem);
#endif
        }
    }
	else
	{
		proxy_type=PROXY_NONE;
	}
    proxy_inited=1;
#ifdef __MINGW32__
    {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2,0),&wsaData);
    }
#endif
    return;
}

/* external function to use to set the proxy settings */
int proxy_set_proxy( int type, const char *host, int port )
{
    proxy_type=type;
    if (type != PROXY_NONE) {
    proxy_port=0;
    
    if (host != NULL)  {
	proxy_host=(char *)strdup(host);
	proxy_port=port;
    }
    if (proxy_port == 0)
	proxy_port=3128;
#ifdef HAVE_ATEXIT
    /* if you do not have atexit, it means that if you don't call
       proxy_freemem when leaving, you'll have mem leaks */
    atexit(proxy_freemem);
#endif
    }
    proxy_inited=1;
#ifdef __MINGW32__
    {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2,0),&wsaData);
    }
#endif
    return(0);
}

/* external function to set the proxy user and proxy pasword */
int proxy_set_auth( const int required, const char *user, const char *passwd )
{
	proxy_auth_required = required;
	if (required)
	{
		proxy_auth = encode_proxy_auth_str(user, passwd);
	}
	else
	{
		if (proxy_auth)
			free(proxy_auth);

		proxy_auth = NULL;
	}

	if (proxy_user)
		free(proxy_user);
	if (proxy_password)
		free(proxy_password);
	proxy_user = strdup(user);
	proxy_password = strdup(passwd);
	return 1;
}

const char *proxy_get_auth( void )
{
	return proxy_auth;
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

int proxy_recv( int s, void * buff, int len, unsigned int flags )
{
#ifdef __MINGW32__
	char type;
#else
	int type;
#endif
	int size;

	
	getsockopt( s, SOL_SOCKET, SO_TYPE, &type, &size );
	if( proxy_type != PROXY_SOCKS5 || type == SOCK_STREAM )
	{
		return recv(s, buff, len, flags );
	}
	else
	{
		const int	buff_len = len + 10;
		char		*tmpbuff = calloc( buff_len, sizeof( char ) );
		int			mylen = recv( s, tmpbuff, buff_len, flags );
		
		memcpy( buff, tmpbuff+10, len);
		free( tmpbuff );

		return( mylen - 9 );
	}
	return( 0 );
}
		


int proxy_send( int s, const void *msg, int len, unsigned int flags )
{
#ifdef __MINGW32__
	char type;
#else
	int type;
#endif
	int size;

	printf("proxy_send\n");
	
	getsockopt( s, SOL_SOCKET, SO_TYPE, &type, &size );
	if( proxy_type != PROXY_SOCKS5 || type == SOCK_STREAM)
	{
		if( type != SOCK_DGRAM )
			printf("type TCP\n");
		else
			printf("type UDP\n");

		return send(s, msg, len, flags);
	}
	else
	{
		const int	buff_len = len + 10;
		char		*buff = calloc( buff_len, sizeof( char ) );
		struct sockaddr * serv_addr = get_server( s );
		int ret;

		printf("type UDP %s %d\n", 
				inet_ntoa((((struct sockaddr_in *)serv_addr)->sin_addr)),
				ntohs(((struct sockaddr_in *)serv_addr)->sin_port) );

		buff[3] = 0x01;
		
		memcpy(buff+4, &(((struct sockaddr_in *)serv_addr)->sin_addr),4 );
		memcpy((buff+8), &(((struct sockaddr_in *)serv_addr)->sin_port), 2);
		memcpy((buff+10), msg, len );

		ret = send( s, buff, buff_len, flags );
		free( buff );
		
		return( ret );
	}
	
	return( 0 );
}

struct hostent *proxy_gethostbyname( const char *host )
{
	if ( !proxy_inited )
		proxy_autoinit();
    
	if ( proxy_type == PROXY_NONE || proxy_type == PROXY_SOCKS5 )
		return( gethostbyname( host ) );

    if ( proxy_realhost != NULL )
		free( proxy_realhost );

    /* we keep the real host name for the Connect command */
    proxy_realhost = strdup( host );
        
    return( gethostbyname( proxy_host ) );
}

struct hostent *proxy_gethostbyname2( const char *host, int type )
{
	if ( !proxy_inited )
		proxy_autoinit();

	if ( proxy_type == PROXY_NONE || proxy_type == PROXY_SOCKS5 )
		return( gethostbyname2( host, type ) );

	if ( proxy_realhost != NULL )
		free( proxy_realhost );

	/* we keep the real host name for the Connect command */
	proxy_realhost = strdup( host );

	return( gethostbyname2( proxy_host, type ) );
}

/* http://archive.socks.permeo.com/protocol/socks4.protocol */
static int socks4_connect( int sock, struct sockaddr *serv_addr, int addrlen )
{
   struct sockaddr_in sa;
   int i, packetlen;
   struct hostent * hp;

   if (!(hp = gethostbyname(proxy_host)))
      return -1;
   
   if(proxy_auth && strlen(proxy_auth)) {
	   packetlen = 9 + strlen(proxy_auth);
   } else {
	   packetlen = 9;
   }
   /* Clear the structure and setup for SOCKS4 open */  
   bzero(&sa, sizeof(sa));
   sa.sin_family = AF_INET;
   sa.sin_port = htons (proxy_port);
   bcopy(hp->h_addr, (char *) &sa.sin_addr, hp->h_length);
   
   /* Connect to the SOCKS4 port and send a connect packet */
   if (connect(sock, (struct sockaddr *) &sa, sizeof (sa))!=-1)
   {
      unsigned short realport;
      unsigned char packet[packetlen];
      if (!(hp = gethostbyname(proxy_realhost)))
      {
         return -1;
      }
      realport = ntohs(((struct sockaddr_in *)serv_addr)->sin_port);
      packet[0] = 4;                                        /* Version */
      packet[1] = 1;                                       /* CONNECT  */
      packet[2] = (((unsigned short) realport) >> 8);      /* DESTPORT */
      packet[3] = (((unsigned short) realport) & 0xff);    /* DESTPORT */
      packet[4] = (unsigned char) (hp->h_addr_list[0])[0]; /* DESTIP   */
      packet[5] = (unsigned char) (hp->h_addr_list[0])[1]; /* DESTIP   */
      packet[6] = (unsigned char) (hp->h_addr_list[0])[2]; /* DESTIP   */
      packet[7] = (unsigned char) (hp->h_addr_list[0])[3]; /* DESTIP   */
      if(proxy_auth && strlen(proxy_auth)) {
	      for (i=0; i < strlen(proxy_user); i++) {
		      packet[i+8] = 
			    (unsigned char)proxy_user[i];  /* AUTH     */
	      }
      }
      packet[packetlen-1] = 0;                              /* END      */
      printf("Sending \"%s\"\n", packet);
      if (write(sock, packet, packetlen) == packetlen)
      {
         bzero(packet, sizeof(packet));
         /* Check response - return as SOCKS4 if its valid */
         if (read(sock, packet, 9)>=4)
         {
	    if (packet[1] == 90)		 
	            return 0;
	    else if(packet[1] == 91)
		    ay_do_error( _("Proxy Error"), _("Socks 4 proxy rejected request for an unknown reason.") );
	    else if(packet[1] == 92)
		    ay_do_error( _("Proxy Error"), _("Socks 4 proxy rejected request because it could not connect to our identd.") );
	    else if(packet[1] == 93)
		    ay_do_error( _("Proxy Error"), _("Socks 4 proxy rejected request because identd returned a different userid.") );
	    else {
		    ay_do_error( _("Proxy Error"), _("Socks 4 proxy rejected request with an RFC-uncompliant error cod.") );   
		    printf("=>>%d\n",packet[1]);
	    }		    
         } else {
		printf("short read %s\n",packet);
	 }	
      }
      close(sock);
   }
   
   return -1;
}

/* http://archive.socks.permeo.com/rfc/rfc1928.txt */
/* http://archive.socks.permeo.com/rfc/rfc1929.txt */
static int socks5_connect( int sockfd, struct sockaddr *serv_addr, int addrlen )
{
   int i;
   int s =0;
#ifdef __MINGW32__
   char type;
#else
   int type;
#endif
   int size = sizeof(int);
   int tcplink = 0;
   struct sockaddr_in bind_addr;
   struct sockaddr_in sin;
   struct hostent * hostinfo;
   char buff[530];
   int need_auth = 0;
   int j;   
   getsockopt( sockfd, SOL_SOCKET, SO_TYPE, &type, &size );
   printf(" %d %d %d \n", SOCK_STREAM, SOCK_DGRAM, type );

   bind_addr.sin_addr.s_addr = INADDR_ANY;
   bind_addr.sin_family = PF_INET;
   bind_addr.sin_port = 0;

   if( type == SOCK_DGRAM )
   {
	   size = sizeof(bind_addr);
	   if(bind(sockfd, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) < 0 )
	   {
		   perror("bind");
	   }
	   getsockname( sockfd, (struct sockaddr*)&bind_addr, &size );
   }
   hostinfo = gethostbyname (proxy_host);
   if (hostinfo == NULL) 
   {
	   fprintf (stderr, "Unknown host %s.\n", proxy_host);
	   return (-1);
   }

   /* connect to the proxy server */
   bcopy(hostinfo->h_addr, (char *)&sin.sin_addr, hostinfo->h_length);
   sin.sin_family = AF_INET;
   sin.sin_port = htons(proxy_port);

   if( type != SOCK_DGRAM )
   {
      s = connect(sockfd, (struct sockaddr *)&sin, sizeof(sin)); 
	   if( s < 0 )
		   return s;
   }
   else
   {
#ifdef __MINGW32__
	   printf("Connecting to %lx on %u\n", inet_addr(proxy_host), proxy_port );
#else
	   printf("Connecting to %x on %u\n", inet_addr(proxy_host), proxy_port );
#endif
	   tcplink = connect_address( inet_addr(proxy_host), proxy_port );
	   printf("We got socket %d\n", tcplink);
   }

   buff[0] = 0x05;  //use socks v5
   if(proxy_user && strlen(proxy_user)) {
	   buff[1] = 0x02;  //we support (no authentication & username/pass)
	   buff[2] = 0x00;  //we support the method type "no authentication"
	   buff[3] = 0x02;  //we support the method type "username/passw"
	   need_auth=1;
   } else {
	   buff[1] = 0x01;  //we support (no authentication)
	   buff[2] = 0x00;  //we support the method type "no authentication"
   }	 


   if( type != SOCK_DGRAM )
   {
	   int l = 0;
	   write( sockfd, buff, 3+need_auth );
	   
	   l = read( sockfd, buff, 2 );
	   printf("read %d\n",l);
   }
   else
   {
	   write( tcplink, buff, 3+need_auth );
	   read( tcplink, buff, 2 );
	   fprintf(stderr, "We got a response back from the SOCKS server\n");
   }

   printf("buff[] %d %d proxy_user %s\n",buff[0],buff[1], proxy_user);
   if( buff[1] == 0x00 )
	   need_auth = 0;
   else if (buff[1] == 0x02 && proxy_user && strlen(proxy_user))
	   need_auth = 1;
   else {
	   fprintf(stderr, "No Acceptable Methods");
	   return -1;
   }
   printf("need_auth=%d\n",need_auth);
   if (need_auth) {
	/* subneg start */
	buff[0] = 0x01; 		/* subneg version  */
	printf("[%d]",buff[0]);
	buff[1] = strlen(proxy_user);   /* username length */
	printf("[%d]",buff[1]);
	for (i=0; i < strlen(proxy_user) && i<255; i++) {
		      buff[i+2] = 
			    proxy_user[i];  /* AUTH     */
		      printf("%c",buff[i+2]);
	}
	i+=2;
	buff[i] = strlen(proxy_password);
	printf("[%d]",buff[i]);
	i++;
	for (j=0; j < strlen(proxy_password) && j<255; j++) {
		      buff[i+j] = 
			    proxy_password[j];  /* AUTH     */
			printf("%c",buff[i+j]);
	}
	i+=(j);
	buff[i]=0;
	if( type != SOCK_DGRAM )
	{
		write( sockfd, buff, i );
		read( sockfd, buff, 2 );
	}
	else
	{
		write( tcplink, buff, i );
		read( tcplink, buff, 2 );
		fprintf(stderr, "We got a response back from the SOCKS server\n");
	}
	if (buff[1] != 0) {
		ay_do_error( _("Proxy Error"), _("Socks5 proxy refused our authentication.") );
		return -1;
	}
   }
   
   buff[0] = 0x05; //use socks5
   buff[1] = ((type == SOCK_STREAM) ? 0x01 : 0x03); //connect
   buff[2] = 0x00; //reserved
   buff[3] = 0x01; //ipv4 address
   if( type != SOCK_DGRAM )
   {
	   memcpy(buff+4, &(((struct sockaddr_in *)serv_addr)->sin_addr),4 );
	   memcpy((buff+8), &(((struct sockaddr_in *)serv_addr)->sin_port), 2);
   }
   else
   {
	   buff[4] = 0x00;
	   buff[5] = 0x00;
	   buff[6] = 0x00;
	   buff[7] = 0x00;
	   memcpy((buff+8), &(((struct sockaddr_in *)&bind_addr)->sin_port), 2);
	   printf("UDP port %s %d\n", inet_ntoa(((struct sockaddr_in *)serv_addr)->sin_addr), 
			   ntohs(((struct sockaddr_in *)serv_addr)->sin_port));
	   for( i = 0; i < 8; i++ )
	   {
		   printf("%03d ", buff[i] );
	   }
	   printf("%d", ntohs(*(unsigned short *)&buff[8]));
	   printf("\n");
   }

   if( type != SOCK_DGRAM )
   {
	   write( sockfd, buff, 10 );
	   read(sockfd, buff, 10);
   }
   else
   {
	   write( tcplink, buff, 10 );
	   read( tcplink, buff, 10);
	   fprintf(stderr, "We got another response from the server!\n");
   }

   //	buff[1] = 0;


   if( buff[1] == 0x00 )
   {
	   for( i = 0; i < 8; i++ )
	   {
		   printf("%03d ", buff[i] );
	   }
	   printf("%d", ntohs(*(unsigned short *)&buff[8]));
	   printf("\n");
	   if( type == SOCK_DGRAM )
	   {
		   udp_connection	*conn = calloc( 1, sizeof(udp_connection) );
		   struct sockaddr	*host = calloc( 1, sizeof(struct sockaddr) );
		   
		   memcpy(host, serv_addr, sizeof(struct sockaddr_in));
		   conn->next = server_list;
		   conn->fd = sockfd;
		   conn->host = host;
		   server_list = conn;
		   printf("adding UDP connection %s %d on fd %d\n", 
			   inet_ntoa((((struct sockaddr_in *)serv_addr)->sin_addr)),
			   ntohs(((struct sockaddr_in *)serv_addr)->sin_port),
			   sockfd );

		   memcpy( &sin.sin_addr, buff+4, 4);
		   memcpy( &sin.sin_port, buff+8, 2);

    	   sin.sin_family = AF_INET;
		   printf( "trying to connect to %s on port %d \n", 
				   inet_ntoa(sin.sin_addr), ntohs(sin.sin_port) );
           s = connect(sockfd, (struct sockaddr *)&sin, sizeof(sin)); 
	   }

	   printf("libproxy; SOCKS5 connection on %d (%d)\n", sockfd,s  );
	   return  s;
   }
   else
   {
	   for( i = 0; i < 8; i++ )
	   {
		   printf("%03d ", buff[i] );
	   }
	   printf("%d", ntohs(*(unsigned short *)&buff[8]));
	   printf("\n");
	   fprintf(stderr, "SOCKS error number %d\n", buff[1] );
	   close(sockfd);
	   return -1;
   }
}

static int http_tunnel_init( int sockfd, struct sockaddr *serv_addr, int addrlen )
{
	/* step two : do  proxy tunneling init */
	char cmd[200];
	char *inputline = NULL;
	unsigned short realport=ntohs(((struct sockaddr_in *)serv_addr)->sin_port);
   
	sprintf(cmd,"CONNECT %s:%d HTTP/1.1\r\n",proxy_realhost,realport);
	if ( proxy_auth != NULL ) {
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
		return(-1);
	if (proxy_recv_line(sockfd,&inputline) < 0)
		return(-1);
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
				ay_do_error( _("Proxy Error"), _("HTTP proxy error: Authentication required.") );
		       return( -2 );
		   }
		   if ( strstr( inputline, "403" ) ) {
		       while(proxy_recv_line(sockfd,&inputline) > 0) {
			       free(inputline);
		       }
				ay_do_error( _("Proxy Error"), _("HTTP proxy error: permission denied.") );
		       return( -2 );
		   }
		   free(inputline);
		   return(-1);
		}

	while (strlen(inputline)>1) {
		free(inputline);
		if (proxy_recv_line(sockfd,&inputline) < 0) {
		   return(-1);
		}
#ifndef DEBUG
		sprintf(debug_buff,"<%s>\n",inputline);
		debug_print(debug_buff);
#endif
	}
	free(inputline);
	        
	return 0;
}

static int http_connect( int sockfd, struct sockaddr *serv_addr, int addrlen )
{
	/* do the  tunneling */
	/* step one : connect to  proxy */
   struct sockaddr_in name;
   int ret;
	struct hostent *hostinfo;
	unsigned short shortport = proxy_port;

	memset (&name, 0, sizeof (name));
	name.sin_family = AF_INET;
	name.sin_port = htons (shortport);
	hostinfo = gethostbyname (proxy_host);
	if (hostinfo == NULL) {
		fprintf (stderr, "Unknown host %s.\n", proxy_host);
		return (-1);
	}
	name.sin_addr = *(struct in_addr *) hostinfo->h_addr;
#ifndef DEBUG
	sprintf(debug_buff,"Trying to connect ...\n");
	debug_print(debug_buff);
#endif
	if ((ret = connect(sockfd,(struct sockaddr *)&name,sizeof(name)))<0)
	   return(ret);

   return (http_tunnel_init(sockfd, serv_addr, addrlen));       
}

int proxy_connect_host( const char *host, int port, void *cb, void *data, void *scb )
{
	struct hostent *hp;
	struct sockaddr_in sin;
	if ((hp = proxy_gethostbyname((char *)host)) == NULL) 
	{
		ay_do_error( _("Proxy Error"), _("Could not resolve host.") );
		perror("ghbn");
		return -1;
	}
	sin.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	
	return proxy_connect(-1, (struct sockaddr *)&sin, sizeof(sin), cb, data, scb);

}

static int conn_ok( int sockfd, void *cb, void *data )
{
	ay_socket_callback callback = (ay_socket_callback)cb;
	if (callback) {
		callback(sockfd, 0, data);
		return 0;
	} else {
		return sockfd;
	}	
}

static int conn_nok( int sockfd, void *cb, void *data )
{
	ay_socket_callback callback = (ay_socket_callback)cb;
	if (callback) {
		callback(-1, ECONNREFUSED, data);
		return 0;
	} else {
		return -1;
	}	
}

int proxy_get_proxy( ) {
	return proxy_type;
}

int proxy_connect( int sockfd, struct sockaddr *serv_addr, int addrlen, void *cb, void *data, void *scb )
{
   int tmp;
   ay_socket_callback callback = (ay_socket_callback)cb;
   
   if (!proxy_inited)
	   proxy_autoinit();

   if (sockfd == -1 && (proxy_type != PROXY_NONE || !callback))
	   sockfd = socket(AF_INET, SOCK_STREAM, 0);
   
   switch (proxy_type) {
      case PROXY_NONE:    /* No proxy */
		{
		struct sockaddr_in *sin = (struct sockaddr_in *)serv_addr;
		if (callback)
			return ay_socket_new_async(
			      inet_ntoa(sin->sin_addr), 
			      ntohs(sin->sin_port), 
			      callback, data, scb);     
		else {
			if (connect(sockfd, serv_addr, addrlen) == 0) {
				return sockfd;
			} else {
				return -1;
			}
		}
		break;
		}
      case PROXY_HTTP:    /* Http proxy */
		if ( (tmp=http_connect(sockfd, serv_addr, addrlen)) >= 0 ) {
			return conn_ok(sockfd, callback, data);
		} else {
			return conn_nok(sockfd, callback, data);
		}
		break;
      case PROXY_SOCKS4:  /* SOCKS4 proxy */
		if ( (tmp=socks4_connect(sockfd, serv_addr, addrlen)) >= 0 ) {
			return conn_ok(sockfd, callback, data);
		} else {
			return conn_nok(sockfd, callback, data);
		}
		break;
      case PROXY_SOCKS5:  /* SOCKS5 proxy */
		if ( (tmp=socks5_connect(sockfd, serv_addr, addrlen)) >= 0 ) {
			return conn_ok(sockfd, callback, data);
		} else {
			return conn_nok(sockfd, callback, data);
		}
		break;
      default:
	      fprintf(stderr,"Unknown proxy type : %d.\n",proxy_type);
	      break;
   }
   return(-1);
}

static char *encode_proxy_auth_str( const char *user, const char *passwd )
{
	char buff[200];
	char buff1[200];

	if ( user == NULL )
		return NULL;

	strcpy( buff, user );
	strcat( buff, ":" );
	strcat( buff, passwd );

	ap_base64encode( buff1, buff, strlen( buff ) );

	return strdup( buff1 );
}

/* ====================================================================
 * Copyright (c) 1995-1999 The Apache Group.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the Apache Group
 *    for use in the Apache HTTP server project (http://www.apache.org/)."
 *
 * 4. The names "Apache Server" and "Apache Group" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache"
 *    nor may "Apache" appear in their names without prior written
 *    permission of the Apache Group.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the Apache Group
 *    for use in the Apache HTTP server project (http://www.apache.org/)."
 *
 * THIS SOFTWARE IS PROVIDED BY THE APACHE GROUP ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE APACHE GROUP OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Group and was originally based
 * on public domain software written at the National Center for
 * Supercomputing Applications, University of Illinois, Urbana-Champaign.
 * For more information on the Apache Group and the Apache HTTP server
 * project, please see <http://www.apache.org/>.
 *
 */

/* base64 encoder/decoder. Originally part of main/util.c
 * but moved here so that support/ab and ap_sha1.c could
 * use it. This meant removing the ap_palloc()s and adding
 * ugly 'len' functions, which is quite a nasty cost.
 */

/*#include <string.h>*/

/* aaaack but it's fast and const should make it shared text page. */
static const unsigned char pr2six[256] =
{
#ifndef CHARSET_EBCDIC
    /* ASCII table */
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
    64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
    64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
#else /*CHARSET_EBCDIC*/
    /* EBCDIC table */
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 63, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 64, 64, 64, 64, 64, 64,
    64, 35, 36, 37, 38, 39, 40, 41, 42, 43, 64, 64, 64, 64, 64, 64,
    64, 64, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64,  0,  1,  2,  3,  4,  5,  6,  7,  8, 64, 64, 64, 64, 64, 64,
    64,  9, 10, 11, 12, 13, 14, 15, 16, 17, 64, 64, 64, 64, 64, 64,
    64, 64, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64, 64,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64
#endif /*CHARSET_EBCDIC*/
};

/* Ayttm - We don't use these but they are left in in case we want them later */
#if 0
static int ap_base64decode_binary(unsigned char *bufplain, const char *bufcoded);

static int ap_base64decode_len(const char *bufcoded)
{
    int nbytesdecoded;
    register const unsigned char *bufin;
    register int nprbytes;

    bufin = (const unsigned char *) bufcoded;
    while (pr2six[*(bufin++)] <= 63);

    nprbytes = (bufin - (const unsigned char *) bufcoded) - 1;
    nbytesdecoded = ((nprbytes + 3) / 4) * 3;

    return nbytesdecoded + 1;
}

static int ap_base64decode(char *bufplain, const char *bufcoded)
{
#ifdef CHARSET_EBCDIC
    int i;
#endif				/* CHARSET_EBCDIC */
    int len;
    
    len = ap_base64decode_binary((unsigned char *) bufplain, bufcoded);
#ifdef CHARSET_EBCDIC
    for (i = 0; i < len; i++)
	bufplain[i] = os_toebcdic[bufplain[i]];
#endif				/* CHARSET_EBCDIC */
    bufplain[len] = '\0';
    return len;
}

/* This is the same as ap_base64udecode() except on EBCDIC machines, where
 * the conversion of the output to ebcdic is left out.
 */
static int ap_base64decode_binary(unsigned char *bufplain,
				   const char *bufcoded)
{
    int nbytesdecoded;
    register const unsigned char *bufin;
    register unsigned char *bufout;
    register int nprbytes;

    bufin = (const unsigned char *) bufcoded;
    while (pr2six[*(bufin++)] <= 63);
    nprbytes = (bufin - (const unsigned char *) bufcoded) - 1;
    nbytesdecoded = ((nprbytes + 3) / 4) * 3;

    bufout = (unsigned char *) bufplain;
    bufin = (const unsigned char *) bufcoded;

    while (nprbytes > 4) {
	*(bufout++) =
	    (unsigned char) (pr2six[*bufin] << 2 | pr2six[bufin[1]] >> 4);
	*(bufout++) =
	    (unsigned char) (pr2six[bufin[1]] << 4 | pr2six[bufin[2]] >> 2);
	*(bufout++) =
	    (unsigned char) (pr2six[bufin[2]] << 6 | pr2six[bufin[3]]);
	bufin += 4;
	nprbytes -= 4;
    }

    /* Note: (nprbytes == 1) would be an error, so just ingore that case */
    if (nprbytes > 1) {
	*(bufout++) =
	    (unsigned char) (pr2six[*bufin] << 2 | pr2six[bufin[1]] >> 4);
    }
    if (nprbytes > 2) {
	*(bufout++) =
	    (unsigned char) (pr2six[bufin[1]] << 4 | pr2six[bufin[2]] >> 2);
    }
    if (nprbytes > 3) {
	*(bufout++) =
	    (unsigned char) (pr2six[bufin[2]] << 6 | pr2six[bufin[3]]);
    }

    nbytesdecoded -= (4 - nprbytes) & 3;
    return nbytesdecoded;
}
#endif

static const char basis_64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* Ayttm - We don't use these but they are left in in case we want them later */
#if 0
static int ap_base64encode_len(int len)
{
    return ((len + 2) / 3 * 4) + 1;
}
#endif

/* This is the same as ap_base64encode() except on EBCDIC machines, where
 * the conversion of the input to ascii is left out.
 */
static int ap_base64encode_binary(char *encoded,
                                       const unsigned char *string, int len)
{
    int i;
    char *p;

    p = encoded;
    for (i = 0; i < len - 2; i += 3) {
	*p++ = basis_64[(string[i] >> 2) & 0x3F];
	*p++ = basis_64[((string[i] & 0x3) << 4) |
	                ((int) (string[i + 1] & 0xF0) >> 4)];
	*p++ = basis_64[((string[i + 1] & 0xF) << 2) |
	                ((int) (string[i + 2] & 0xC0) >> 6)];
	*p++ = basis_64[string[i + 2] & 0x3F];
    }
    if (i < len) {
	*p++ = basis_64[(string[i] >> 2) & 0x3F];
	if (i == (len - 1)) {
	    *p++ = basis_64[((string[i] & 0x3) << 4)];
	    *p++ = '=';
	}
	else {
	    *p++ = basis_64[((string[i] & 0x3) << 4) |
	                    ((int) (string[i + 1] & 0xF0) >> 4)];
	    *p++ = basis_64[((string[i + 1] & 0xF) << 2)];
	}
	*p++ = '=';
    }

    *p++ = '\0';
    return p - encoded;
}

static int ap_base64encode(char *encoded, const char *string, int len)
{
#ifndef CHARSET_EBCDIC
    return ap_base64encode_binary(encoded, (const unsigned char *) string, len);
#else				/* CHARSET_EBCDIC */
    int i;
    char *p;

    p = encoded;
    for (i = 0; i < len - 2; i += 3) {
	*p++ = basis_64[(os_toascii[string[i]] >> 2) & 0x3F];
	*p++ = basis_64[((os_toascii[string[i]] & 0x3) << 4) |
	                ((int) (os_toascii[string[i + 1]] & 0xF0) >> 4)];
	*p++ = basis_64[((os_toascii[string[i + 1]] & 0xF) << 2) |
	                ((int) (os_toascii[string[i + 2]] & 0xC0) >> 6)];
	*p++ = basis_64[os_toascii[string[i + 2]] & 0x3F];
    }
    if (i < len) {
	*p++ = basis_64[(os_toascii[string[i]] >> 2) & 0x3F];
	if (i == (len - 1)) {
	    *p++ = basis_64[((os_toascii[string[i]] & 0x3) << 4)];
	    *p++ = '=';
	}
	else {
	    *p++ = basis_64[((os_toascii[string[i]] & 0x3) << 4) |
	                    ((int) (os_toascii[string[i + 1]] & 0xF0) >> 4)];
	    *p++ = basis_64[((os_toascii[string[i + 1]] & 0xF) << 2)];
	}
	*p++ = '=';
    }

    *p++ = '\0';
    return p - encoded;
#endif				/* CHARSET_EBCDIC */
}

#if 0
int
main( )
{
	char *str = "praveen:itisme";
	char buff[1000];
	char buff1[1000];

	ap_base64encode( buff, str, strlen(str) );
	printf( "Buff is %s\n", buff );
	ap_base64decode( buff1, buff );
	return 1;
}
#endif /* 0 */

