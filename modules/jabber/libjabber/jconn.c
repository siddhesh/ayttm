/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  Jabber
 *  Copyright (C) 1998-1999 The Jabber Team http://jabber.org/
 */

#include "jabber/jabber.h"
#include "tcp_util.h"
#include "libproxy/libproxy.h"
#include <config.h>
#ifdef HAVE_OPENSSL
#include "ssl.h"
#endif
/* local macros for launching event handlers */
#define STATE_EVT(arg) if(j->on_state) { (j->on_state)(j, (arg) ); }

/* prototypes of the local functions */
static void startElement(void *userdata, const char *name, const char **attribs);
static void endElement(void *userdata, const char *name);
static void charData(void *userdata, const char *s, int slen);
void jab_continue (int fd, int error, void *data);

/*
 *  jab_new -- initialize a new jabber connection
 *
 *  parameters
 *      user -- jabber id of the user
 *      pass -- password of the user
 *      serv -- connection server (overrides domain in JID)
 *
 *  results
 *      a pointer to the connection structure
 *      or NULL if allocations failed
 */
jconn jab_new(char *user, char *pass, char *serv)
{
    pool p;
    jconn j;

    if(!user) return(NULL);

    p = pool_new();
    if(!p) return(NULL);
    j = pmalloc_x(p, sizeof(jconn_struct), 0);
    if(!j) return(NULL);
    j->p = p;

    j->user = jid_new(p, user);
    j->pass = pstrdup(p, pass);
    j->serv = pstrdup(p, serv);
    
    j->state = JCONN_STATE_OFF;
    j->fd = -1;

    return j;
}

/*
 *  jab_delete -- free a jabber connection
 *
 *  parameters
 *      j -- connection
 *
 */
void jab_delete(jconn j)
{
    if(!j) return;

    jab_stop(j);
    pool_free(j->p);
}

/*
 *  jab_state_handler -- set callback handler for state change
 *
 *  parameters
 *      j -- connection
 *      h -- name of the handler function
 */
void jab_state_handler(jconn j, jconn_state_h h)
{
    if(!j) return;

    j->on_state = h;
}

/*
 *  jab_packet_handler -- set callback handler for incoming packets
 *
 *  parameters
 *      j -- connection
 *      h -- name of the handler function
 */
void jab_packet_handler(jconn j, jconn_packet_h h)
{
    if(!j) return;

    j->on_packet = h;
}


/*
 *  jab_start -- start connection
 *
 *  parameters
 *      j -- connection
 *
 */
int jab_start(jconn j, int port, int use_ssl)
{
    int tag = 0;
    if(!j || j->state != JCONN_STATE_OFF) return 0;

    j->parser = XML_ParserCreate(NULL);
    XML_SetUserData(j->parser, (void *)j);
    XML_SetElementHandler(j->parser, startElement, endElement);
    XML_SetCharacterDataHandler(j->parser, charData);
#ifdef HAVE_OPENSSL
    j->usessl = use_ssl;
#else
    j->usessl = 0;
#endif    
    
    j->user->port = port;

    if (!j->serv || !strlen(j->serv))
      j->serv = j->user->server;

    if ((tag = ay_connect_host(j->serv, port,
		    	    (ay_socket_callback)jab_continue, j, NULL)) < 0) {
	    STATE_EVT(JCONN_STATE_OFF);
	    return 0;
    }	 
    return tag; 
    
}
void jab_continue (int fd, int error, void *data)
{	
    jconn j = (jconn)data;	
    xmlnode x;
    char *t,*t2;

    j->fd = fd;

    if(j->fd < 0 || error) {
        STATE_EVT(JCONN_STATE_OFF)
        return;
    }
    j->state = JCONN_STATE_CONNECTED;
    STATE_EVT(JCONN_STATE_CONNECTED)

#ifdef HAVE_OPENSSL
    if (j->usessl) {
            j->ssl_sock = (SockInfo*)malloc(sizeof(SockInfo));
	    ssl_init();
	    j->ssl_sock->sock = fd;
	    if (!ssl_init_socket(j->ssl_sock, j->user->server, j->user->port)) {
		  printf("ssl error !\n");  
		  STATE_EVT(JCONN_STATE_OFF)
		  return;
	    }
    }  
#endif

    /* start stream */
    x = jutil_header(NS_CLIENT, j->user->server);
    t = xmlnode2str(x);
    /* this is ugly, we can create the string here instead of jutil_header */
    /* what do you think about it? -madcat */
    t2 = strstr(t,"/>");
    *t2++ = '>';
    *t2 = '\0';
    jab_send_raw(j,"<?xml version='1.0'?>");
    jab_send_raw(j,t);
    xmlnode_free(x);

    j->state = JCONN_STATE_ON;
    STATE_EVT(JCONN_STATE_ON)

}

/*
 *  jab_stop -- stop connection
 *
 *  parameters
 *      j -- connection
 */
void jab_stop(jconn j)
{
    if(!j || j->state == JCONN_STATE_OFF) return;

    j->state = JCONN_STATE_OFF;
    close(j->fd);
    j->fd = -1;
    XML_ParserFree(j->parser);
}

/*
 *  jab_getfd -- get file descriptor of connection socket
 *
 *  parameters
 *      j -- connection
 *
 *  returns
 *      fd of the socket or -1 if socket was not connected
 */
int jab_getfd(jconn j)
{
    if(j)
        return j->fd;
    else
        return -1;
}

/*
 *  jab_getjid -- get jid structure of user
 *
 *  parameters
 *      j -- connection
 */
jid jab_getjid(jconn j)
{
    if(j)
        return(j->user);
    else
        return NULL;
}

/*  jab_getsid -- get stream id
 *  This is the id of server's <stream:stream> tag and used for
 *  digest authorization.
 *
 *  parameters
 *      j -- connection
 */
char *jab_getsid(jconn j)
{
    if(j)
        return(j->sid);
    else
        return NULL;
}

/*
 *  jab_send -- send xml data
 *
 *  parameters
 *      j -- connection
 *      x -- xmlnode structure
 */

void jab_send(jconn j, xmlnode x)
{
    if (j && j->state != JCONN_STATE_OFF)
    {
	    char *buf = xmlnode2str(x);
#ifdef HAVE_OPENSSL
	if(j->usessl && buf)
		ssl_write(j->ssl_sock->ssl, buf, strlen(buf));
	else if (buf)
#endif	
	     write(j->fd, buf, strlen(buf));
#ifdef JDEBUG
	    fprintf(stderr, "jab<%s %s\n", j->user->user, buf);
#endif
    }
}

/*
 *  jab_send_raw -- send a string
 *
 *  parameters
 *      j -- connection
 *      str -- xml string
 */
void jab_send_raw(jconn j, const char *str)
{
    if (j && j->state != JCONN_STATE_OFF) {
#ifdef HAVE_OPENSSL
	if(j->usessl)
		ssl_write(j->ssl_sock->ssl, str, strlen(str));
	else
#endif	
		write(j->fd, str, strlen(str));
    }
#ifdef JDEBUG
    fprintf(stderr, "rjab<%s %s\n", j->user->user, str);
#endif
}

/*
 *  jab_recv -- read and parse incoming data
 *
 *  parameters
 *      j -- connection
 */
void jab_recv(jconn j)
{
    static char buf[4096];
    int len;

    if(!j || j->state == JCONN_STATE_OFF)
        return;

#ifdef HAVE_OPENSSL
    if (j->usessl) 
	    len = ssl_read(j->ssl_sock->ssl, buf, sizeof(buf)-1);
    else
#endif
            len = read(j->fd, buf, sizeof(buf)-1);
    if(len>0)
    {
        buf[len] = '\0';
#ifdef JDEBUG
        fprintf(stderr, "jab>%s %s\n", j->user->user, buf);
#endif
        XML_Parse(j->parser, buf, len, 0);
    }
    else if(len<0)
    {
        STATE_EVT(JCONN_STATE_OFF);
        jab_stop(j);
    }
}
#undef JDEBUG
/*
 *  jab_poll -- check socket for incoming data
 *
 *  parameters
 *      j -- connection
 *      timeout -- poll timeout
 */
void jab_poll(jconn j, int timeout)
{
    fd_set fds;
    struct timeval tv;

    if (!j || j->state == JCONN_STATE_OFF)
        return;

    FD_ZERO(&fds);
    FD_SET(j->fd, &fds);

    if (timeout < 0)
    {
        if (select(j->fd + 1, &fds, NULL, NULL, NULL) > 0)
            jab_recv(j);
    }
    else
    {
	    tv.tv_sec  = 0;
	    tv.tv_usec = timeout;
	    if (select(j->fd + 1, &fds, NULL, NULL, &tv) > 0)
	        jab_recv(j);
    }
}

/*
 *  jab_auth -- authorize user
 *
 *  parameters
 *      j -- connection
 */
void jab_auth(jconn j)
{
    xmlnode x,y,z;
    char *hash, *user;

    if (!j)
		return(NULL);

    x = jutil_iqnew(JPACKET__SET, NS_AUTH);
    xmlnode_put_attrib(x, "id", "id_auth");
    y = xmlnode_get_tag(x,"query");

    user = j->user->user;

    if (user) {
        z = xmlnode_insert_tag(y, "username");
        xmlnode_insert_cdata(z, user, -1);
    }

    z = xmlnode_insert_tag(y, "resource");
    xmlnode_insert_cdata(z, j->user->resource, -1);

    if (j->sid) {
        z = xmlnode_insert_tag(y, "digest");
        hash = pmalloc(x->p, strlen(j->sid)+strlen(j->pass)+1);
        strcpy(hash, j->sid);
        strcat(hash, j->pass);
        hash = shahash(hash);
        xmlnode_insert_cdata(z, hash, 40);
    }
    else {
		z = xmlnode_insert_tag(y, "password");
		xmlnode_insert_cdata(z, j->pass, -1);
    }

    jab_send(j, x);
    xmlnode_free(x);
}

/*
 *  jab_reg -- register user
 *
 *  parameters
 *      j -- connection
 */
void jab_reg(jconn j)
{
    xmlnode x,y,z;
    char *user;

    if (!j)
		return(NULL);

    x = jutil_iqnew(JPACKET__SET, NS_REGISTER);
    xmlnode_put_attrib(x, "id", "id_reg");
    y = xmlnode_get_tag(x, "query");

    user = j->user->user;

    if (user){
        z = xmlnode_insert_tag(y, "username");
        xmlnode_insert_cdata(z, user, -1);
    }

    z = xmlnode_insert_tag(y, "resource");
    xmlnode_insert_cdata(z, j->user->resource, -1);

    if (j->pass){
		z = xmlnode_insert_tag(y, "password");
		xmlnode_insert_cdata(z, j->pass, -1);
    }

    jab_send(j, x);
    xmlnode_free(x);
    j->state = JCONN_STATE_ON;
    STATE_EVT(JCONN_STATE_ON)
    return;
}


/* local functions */

static void startElement(void *userdata, const char *name, const char **attribs)
{
    xmlnode x;
    jconn j = (jconn)userdata;

    if(j->current)
    {
        /* Append the node to the current one */
        x = xmlnode_insert_tag(j->current, name);
        xmlnode_put_expat_attribs(x, attribs);

        j->current = x;
    }
    else
    {
        x = xmlnode_new_tag(name);
        xmlnode_put_expat_attribs(x, attribs);
        if(strcmp(name, "stream:stream") == 0) {
            /* special case: name == stream:stream */
            /* id attrib of stream is stored for digest auth */
            j->sid = xmlnode_get_attrib(x, "id");
            /* STATE_EVT(JCONN_STATE_AUTH) */
	    xmlnode_free(x);
        } else {
            j->current = x;
        }
    }
}

static void endElement(void *userdata, const char *name)
{
    jconn j = (jconn)userdata;
    xmlnode x;
    jpacket p;

    if(j->current == NULL) {
        /* we got </stream:stream> */
        STATE_EVT(JCONN_STATE_OFF)
        return;
    }

    x = xmlnode_get_parent(j->current);

    if(x == NULL)
    {
        /* it is time to fire the event */
        p = jpacket_new(j->current);

        if(j->on_packet)
            (j->on_packet)(j, p);
        xmlnode_free(j->current);
    }

    j->current = x;
}

static void charData(void *userdata, const char *s, int slen)
{
    jconn j = (jconn)userdata;

    if (j->current)
        xmlnode_insert_cdata(j->current, s, slen);
}
