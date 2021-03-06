/*
 * Ayttm
 *
 * Copyright (C) 2003, the Ayttm team
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __SSL_CERTIFICATE_H__
#define __SSL_CERTIFICATE_H__

#include "config.h"

#ifdef HAVE_OPENSSL

#include <openssl/ssl.h>
#include <openssl/objects.h>
#include <glib.h>

#include "networking.h"

typedef struct _SSLCertificate SSLCertificate;

struct _SSLCertificate {
	X509 *x509_cert;
	char *host;
	int port;
};

SSLCertificate *ssl_certificate_find(const char *host, int port);
SSLCertificate *ssl_certificate_find_lookup(const char *host, int port,
	int lookup);
int ssl_certificate_check(X509 *x509_cert, const char *host, int port,
	void *data);
char *ssl_certificate_to_string(SSLCertificate *cert);
void ssl_certificate_destroy(SSLCertificate *cert);
void ssl_certificate_delete_from_disk(SSLCertificate *cert);
char *readable_fingerprint(unsigned char *src, int len);
char *ssl_certificate_check_signer(X509 *cert);

gboolean ay_connection_verify(const char *message, const char *title,
	void *data);

#endif				/* USE_OPENSSL */
#endif				/* SSL_CERTIFICATE_H */
