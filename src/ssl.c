/*
 * Ayttm
 *
 * Copyright (C) 2003, the Ayttm team
 *
 * this code is derivative of Sylpheed-claws
 * Copyright (C) 1999-2003, Hiroyuki Yamamoto
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

#include "config.h"

#ifdef HAVE_OPENSSL
#include "ssl.h"
#include "ssl_certificate.h"

#include "debug.h"
#include <string.h>
#include <glib.h>
#include <fcntl.h>
#include "globals.h"

static SSL_CTX *ssl_ctx = NULL;

void ssl_init(void)
{
	SSL_METHOD *meth;

	if (ssl_ctx)
		return;
	
	/* Global system initialization*/
	SSL_library_init();
	SSL_load_error_strings();
	
	/* Create our context*/
	meth = SSLv23_client_method();
	ssl_ctx = SSL_CTX_new(meth);

	/* Set default certificate paths */
	SSL_CTX_set_default_verify_paths(ssl_ctx);
	
#if (OPENSSL_VERSION_NUMBER < 0x0090600fL)
	SSL_CTX_set_verify_depth(ctx,1);
#endif
}

void ssl_done(void)
{
	if (!ssl_ctx)
		return;
	
	SSL_CTX_free(ssl_ctx);
	ssl_ctx = NULL;
}

int ssl_init_socket(SockInfo *sockinfo, char *hostname, unsigned short port)
{
	if (sockinfo == NULL) {
		eb_debug(DBG_CORE, "sockinfo == NULL!\n");
		return -1;
	}
	sockinfo->hostname = strdup(hostname);
	sockinfo->port = port;
	return ssl_init_socket_with_method(sockinfo, SSL_METHOD_SSLv23);
}

static int set_block(int fd)
{
#ifndef __MINGW32__
  int flags = fcntl(fd, F_GETFL, 0);
  if(flags == -1) return -1;
  return fcntl(fd, F_SETFL, flags ^ O_NONBLOCK);
#endif
}

static int set_nonblock(int fd) 
{
#ifndef __MINGW32__
  int flags = fcntl(fd, F_GETFL, 0);
  if(flags == -1) return -1;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif
}

int ssl_set_nonblock(SockInfo *sockinfo) 
{
	return set_nonblock(sockinfo->sock);
}

int ssl_set_block(SockInfo *sockinfo) 
{
	return set_block(sockinfo->sock);
}

int ssl_init_socket_with_method(SockInfo *sockinfo, SSLMethod method)
{
	X509 *server_cert;
	SSL *ssl;
	int res = 0;
	
	ssl = SSL_new(ssl_ctx);
	if (ssl == NULL) {
		eb_debug(DBG_CORE, "Error creating ssl context\n");
		return FALSE;
	}

	switch (method) {
	case SSL_METHOD_SSLv23:
		eb_debug(DBG_CORE, "Setting SSLv23 client method\n");
		SSL_set_ssl_method(ssl, SSLv23_client_method());
		break;
	case SSL_METHOD_TLSv1:
		eb_debug(DBG_CORE, "Setting TLSv1 client method\n");
		SSL_set_ssl_method(ssl, TLSv1_client_method());
		break;
	default:
		break;
	}

	/* nonblocking ssl socks are too painful */
	set_block(sockinfo->sock);
	
	SSL_set_fd(ssl, sockinfo->sock);
	if ((res=SSL_connect(ssl)) == -1) {
		printf(("SSL connect failed (%s)\n"),
			    ERR_error_string(SSL_get_error(ssl, res), NULL));
		res = SSL_get_error(ssl, res);
		switch (res) {
			case SSL_ERROR_NONE:
				printf("SSL_ERROR_NONE\n");break;
			case SSL_ERROR_WANT_READ:
				printf("SSL_ERROR_WANT_READ\n");break;
			case SSL_ERROR_WANT_WRITE:
				printf("SSL_ERROR_WANT_WRITE\n");break;
			case SSL_ERROR_ZERO_RETURN:
				printf("SSL_ERROR_ZERO_RETURN\n");break;
			default:
				printf("DEFAULT\n");break;
		}
		SSL_free(ssl);
		return FALSE;
	}

	/* Get the cipher */

	eb_debug(DBG_CORE, "SSL connection using %s\n",
		    SSL_get_cipher(ssl));

	/* Get server's certificate (note: beware of dynamic allocation) */
	if ((server_cert = SSL_get_peer_certificate(ssl)) == NULL) {
		eb_debug(DBG_CORE, "server_cert is NULL ! this _should_not_ happen !\n");
		SSL_free(ssl);
		return FALSE;
	}

	if (!ssl_certificate_check(server_cert, sockinfo->hostname, sockinfo->port)) {
		X509_free(server_cert);
		SSL_free(ssl);
		return FALSE;
	}

	X509_free(server_cert);
	sockinfo->ssl = ssl;

	return TRUE;
}

void ssl_done_socket(SockInfo *sockinfo)
{
	if (sockinfo->ssl) {
		SSL_free(sockinfo->ssl);
	}
}

int ssl_read(SSL *ssl, char *buf, int len)
{
	return SSL_read(ssl, buf, len);
}

int ssl_write(SSL *ssl, const char *buf, int len)
{
	return SSL_write(ssl, buf, len);
}
#endif
