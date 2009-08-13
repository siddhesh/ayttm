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

#ifndef __SSL_H_
#define __SSL_H_
#include "config.h"

#ifdef HAVE_OPENSSL
/* We de not want KRB5 used as the includes are messed up for that */
#define OPENSSL_NO_KRB5 1
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/evp.h>

#include "networking.h"

#ifdef __cplusplus
extern "C" 
{
#endif


typedef enum {
        SSL_METHOD_SSLv23,
        SSL_METHOD_TLSv1
} SSLMethod;


int ssl_read(SSL *ssl, char *buf, int len);
int ssl_write(SSL *ssl, const char *buf, int len);
SSL *ssl_get_socket(int sock, const char *host, int port, void *data);
SSL *ssl_init_socket_with_method( int sock, const char *host, int port, SSL_CTX *ctx, SSLMethod method, void *data );
void ssl_done_socket(SSL *ssl);

#ifdef __cplusplus
}
#endif
#endif
#endif
