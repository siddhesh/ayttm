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
 * tcp_util.c
 * tcp/ip utilities
 *
 */

#include <string.h>
#ifndef __MINGW32__
#include <netdb.h>
#endif
#include <sys/types.h>
#ifdef __MINGW32__
#include <winsock2.h>
#include <limits.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

#include "globals.h"
#include "plugin_api.h"


/* so the compiler tells us about mismatches */
#include "tcp_util.h"


/**
 * ay_socket_new
 * @host: The host to connect to
 * @port: The port to connect on
 *
 * Description: Connect a socket to the given host and port and
 * return a file descriptor to it.
 *
 * Returns:  File descriptor for the socket; -1 on error - errno is set
 **/
int ay_socket_new(const char * host, int port)
{
	struct sockaddr_in serv_addr;
	static struct hostent *server;
	static char last_host[256];
	int servfd;
	char **p;

	if(last_host[0] || strcasecmp(last_host, host)!=0) {
		if(!(server = gethostbyname(host))) {
			errno=h_errno;
			eb_debug(DBG_CORE, "failed to look up server (%s:%d)\n%d: %s", 
						host, port, errno, strerror(errno));
			return -1;
		}
		strncpy(last_host, host, 255);
	}

	if((servfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		eb_debug(DBG_CORE, "Socket create error (%d): %s", errno, strerror(errno));
		return -1;
	}

	eb_debug(DBG_CORE, "connecting to %s:%d\n", host, port);

	for (p = server->h_addr_list; *p; p++)
	{
		memset(&serv_addr, 0, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		memcpy(&serv_addr.sin_addr.s_addr, *p, server->h_length);
		serv_addr.sin_port = htons(port);

		eb_debug(DBG_CORE, "trying %s", *p);
		if(connect(servfd, (struct sockaddr *) &serv_addr, 
					sizeof(serv_addr)) == -1) {
#ifdef __MINGW32__
			int err = WSAGetLastError();
			if(err!=WSAENETUNREACH && err != WSAECONNREFUSED && 
				err != WSAETIMEDOUT)
#else
			if(errno!=ECONNREFUSED && errno!=ETIMEDOUT && 
					errno!=ENETUNREACH) 
#endif
			{
				break;
			}
		} else {
			eb_debug(DBG_CORE, "connected");
			return servfd;
		}
	}

	eb_debug(DBG_CORE, "Could not connect to %s:%d\n%d:%s", host, port, errno, strerror(errno));
	close(servfd);
	return -1;
}

/**
 * ay_tcp_readline
 * @buff: A buffer to read bytes into
 * @maxlen: Maximum number of bytes to read
 * @fd: File descriptor to read from
 *
 * Description: Read a single line or as many bytes as were available
 * on the given file descriptor.
 * 
 * Returns: The number of bytes read; 0 on End of Stream;
 * -1 on error - errno is set
 **/
int ay_tcp_readline(char *buff, int maxlen, int fd)
{
	int n, rc;
	char c;

	for (n = 1; n < maxlen; n++) {

		do {
			rc = read(fd, &c, 1);
		} while(rc == -1 && errno == EINTR);

		if (rc == 1) {
			if(c == '\r')			/* get rid of \r */
				continue;
			*buff = c;
			if (c == '\n')
				break;
			buff++;
		} else if (rc == 0) {
			if (n == 1)
				return (0);		/* EOF, no data */
			else
				break;			/* EOF, w/ data */
		} else {
			return -1;
		}
	}

	*buff = 0;
	return (n);
}

/**
 * ay_tcp_writeline
 * @buff: Data to be written
 * @nbytes: Number of bytes to be written
 * @fd: File descriptor to write to
 *
 * Description: Writes nbytes of data from buff to fd terminating
 * with a CRLF pair.
 * 
 * Returns: The number of bytes written
 **/
int ay_tcp_writeline(const char *buff, int nbytes, int fd)
{
	int	n=0;
	int	count=0;
	char eoln[2] = "\015\012";

	while((n=write(fd, buff+count, nbytes)) < nbytes) {
		if(n<=0) {
			if (errno == EINTR)
				continue;
			return n;
		}
		nbytes-=n;
		count+=n;
	}

	count += write(fd, eoln, 2);

	return count;
}

struct connect_callback_data {
	ay_socket_callback callback;
	void * callback_data;

	int ebi_tag;
	int tag;
};

static LList * pending_connects=NULL;
static int tag_pool=0;

static void destroy_pending_connect(struct connect_callback_data * ccd)
{
	eb_input_remove(ccd->ebi_tag);
	if(ccd->tag == tag_pool)
		tag_pool--;
	free(ccd);
}

static void connect_complete(void *data, int source, eb_input_condition condition)
{
	struct connect_callback_data *ccd = data;
	int error, err_size = sizeof(error);
	const ay_socket_callback callback = ccd->callback;
	void * callback_data = ccd->callback_data;

	pending_connects = l_list_remove(pending_connects, ccd);
	destroy_pending_connect(ccd);

	getsockopt(source, SOL_SOCKET, SO_ERROR, &error, &err_size);

	if(error) {
		close(source);
		source = -1;
	}

	callback(source, error, callback_data);
}

/**
 * ay_socket_cancel_async
 * @tag: a tag returned by ay_socket_new_async
 *
 * Description: cancels a pending connect.  Does nothing if the
 * connect already completed or was never requested.
 **/
void ay_socket_cancel_async(int tag)
{
	LList * l;

	for(l = pending_connects; l; l=l->next) {
		struct connect_callback_data * ccd = l->data;
		if(ccd->tag == tag) {
			pending_connects = l_list_remove_link(pending_connects, l);
			destroy_pending_connect(ccd);
			free(l);
			return;
		}
	}
}

/**
 * ay_socket_new_async
 * @host: The host to connect to
 * @port: The port to connect on
 * @callback: The callback function to call after connect completes
 * @callback_data: Callback data to pass to the callback function
 *
 * Description: Asynchronously connect a socket to the given host and 
 * port.  NOTE: The callback function may get called before this
 * function returns.  Call ay_socket_cancel_async to cancel a connect
 * that hasn't yet called back.  Connects that callback do not need
 * to be cancelled.
 *
 * Returns:  0 if connect succeeded immediately (callback is already called);
 * -1 if connect failed immediately (callback is already called with errno);
 * a tag that identifies the pending connect and can be used to cancel it.
 **/
int ay_socket_new_async(const char * host, int port, ay_socket_callback callback, void * callback_data)
{
	struct sockaddr_in serv_addr;
	static struct hostent *server;
	int servfd;
	struct connect_callback_data * ccd;
	int error,err;

	if(tag_pool >= INT_MAX)
		return -1;

	if(!(server = gethostbyname(host))) {
		errno=h_errno;
		eb_debug(DBG_CORE, "failed to look up server (%s:%d)\n%d: %s", 
					host, port, errno, strerror(errno));
		return -1;
	}

	if((servfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		eb_debug(DBG_CORE, "Socket create error (%d): %s", errno, strerror(errno));
		return -1;
	}

#ifndef __MINGW32__
	fcntl(servfd, F_SETFL, O_NONBLOCK);
#endif
	
	eb_debug(DBG_CORE, "connecting to %s:%d\n", host, port);

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	memcpy(&serv_addr.sin_addr.s_addr, *server->h_addr_list, server->h_length);
	serv_addr.sin_port = htons(port);

	error = connect(servfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
#ifdef __MINGW32__
	err = WSAGetLastError();
#define EINPROGRESS WSAEINPROGRESS
#else
	err = errno;
#endif

	if(!error) {
		callback(servfd, 0, callback_data);
		return 0;
	} else if(error == -1 && err == EINPROGRESS) {
		ccd = calloc(1, sizeof(struct connect_callback_data));
		ccd->callback = callback;
		ccd->callback_data = callback_data;
		ccd->tag = ++tag_pool;

		pending_connects = l_list_append(pending_connects, ccd);
		ccd->ebi_tag = eb_input_add(servfd, EB_INPUT_WRITE, connect_complete, ccd);
		return ccd->tag;
	} else {
		close(servfd);
		callback(-1, errno, callback_data);
		return -1;
	}
}


