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

/* Basic functions like hostname lookup, connect and getting data */

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

#include "common.h"
#include "net_constants.h"


struct addrinfo *lookup_address(const char *host, int port_num, int family)
{
	struct addrinfo hints;
	struct addrinfo *result=NULL;
	char port[10];

	memset(&hints, 0, sizeof(struct addrinfo));

	hints.ai_family = family;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_CANONNAME;

	snprintf(port, sizeof(port), "%d", port_num);

	if ( getaddrinfo(host, port, &hints, &result) < 0 ) {
		fprintf(stderr, "Lookup: unable to create socket for %s:%s\n", host, port);
	}
	else {
		return result;
	}

	return NULL;
}


int connect_address( const char *host, int port_num )
{
	int fd;
	struct addrinfo hints;
	struct addrinfo *result=NULL, *rp=NULL;
	char port[10];
	int ret = -1;

	memset(&hints, 0, sizeof(struct addrinfo));

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	snprintf(port, sizeof(port), "%d", port_num);

	if ( (ret = getaddrinfo(host, port, &hints, &result)) < 0 ) {
		fprintf(stderr, "Connect: unable to create socket for %s:%s(%s)\n", host, port, gai_strerror(ret));
		ret = AY_HOSTNAME_LOOKUP_FAIL;
	}
	else {
		for( rp = result; rp; rp = rp->ai_next ) {
			fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

			if (fd == -1)
				continue;

			if (connect(fd, rp->ai_addr, rp->ai_addrlen) != -1) {
				freeaddrinfo(result);
				return fd;
			}
			else {
				perror("connect:" );
				ret = AY_CONNECT_FAILED;
			}
		}
	}

	if(result)
		freeaddrinfo(result);

	return ret;
}


/* this code is borrowed from cvs 1.10 */
int ay_recv_line( int sock, char **resultp )
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
			fprintf (stderr, "recv() error from  server\n");
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


