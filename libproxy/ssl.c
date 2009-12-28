/*
 * Ayttm
 *
 * Copyright (C) 2003,2009, the Ayttm team
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
#include "globals.h"
#include "networking.h"

static gboolean ssl_inited = FALSE;

static GMutex **ssl_mutex = NULL;

static void locking_function(int mode, int n, const char *file, int line)
{
	if (mode & CRYPTO_LOCK)
		g_mutex_lock(ssl_mutex[n]);
	else
		g_mutex_unlock(ssl_mutex[n]);
}

static unsigned long id_function(void)
{
	return (unsigned long)g_thread_self();
}

/* Global system initialization */
void ssl_init(void)
{
	int num_locks = 0, i = 0;
	static GStaticMutex ssl_init_lock = G_STATIC_MUTEX_INIT;

	g_static_mutex_lock(&ssl_init_lock);

	if (ssl_inited)
		return;

	SSL_library_init();
	SSL_load_error_strings();

	num_locks = CRYPTO_num_locks();

	ssl_mutex = g_new0(GMutex *, num_locks);

	for (i = 0; i < num_locks; i++)
		ssl_mutex[i] = g_mutex_new();

	CRYPTO_set_locking_callback(locking_function);
	CRYPTO_set_id_callback(id_function);

	ssl_inited = TRUE;

	g_static_mutex_unlock(&ssl_init_lock);
}

SSL *ssl_get_socket(int sock, const char *host, int port, void *data)
{
	SSL_CTX *ssl_ctx = NULL;
	SSL_METHOD *meth;

	if (!ssl_inited)
		ssl_init();

	/* Create our context */
	meth = SSLv23_client_method();
	ssl_ctx = SSL_CTX_new(meth);

	/* Set default certificate paths */
	SSL_CTX_set_default_verify_paths(ssl_ctx);

#if (OPENSSL_VERSION_NUMBER < 0x0090600fL)
	SSL_CTX_set_verify_depth(ssl_ctx, 1);
#endif

	return ssl_init_socket_with_method(sock, host, port, ssl_ctx,
		SSL_METHOD_SSLv23, data);
}

SSL *ssl_init_socket_with_method(int sock, const char *host, int port,
	SSL_CTX *ssl_ctx, SSLMethod method, void *data)
{
	X509 *server_cert;
	SSL *ssl;
	int res = 0;

	ssl = SSL_new(ssl_ctx);

	if (ssl == NULL) {
		eb_debug(DBG_CORE, "Error creating ssl context\n");
		return NULL;
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

	SSL_set_fd(ssl, sock);
	if ((res = SSL_connect(ssl)) == -1) {
		eb_debug(DBG_CORE, ("SSL connect failed (%s)\n"),
			ERR_error_string(SSL_get_error(ssl, res), NULL));
		res = SSL_get_error(ssl, res);
		switch (res) {
		case SSL_ERROR_NONE:
			printf("SSL_ERROR_NONE\n");
			break;
		case SSL_ERROR_WANT_READ:
			printf("SSL_ERROR_WANT_READ\n");
			break;
		case SSL_ERROR_WANT_WRITE:
			printf("SSL_ERROR_WANT_WRITE\n");
			break;
		case SSL_ERROR_ZERO_RETURN:
			printf("SSL_ERROR_ZERO_RETURN\n");
			break;
		default:
			printf("DEFAULT\n");
			break;
		}
		SSL_free(ssl);
		return NULL;
	}

	/* Get the cipher */

	eb_debug(DBG_CORE, "SSL connection using %s\n", SSL_get_cipher(ssl));

	/* Get server's certificate (note: beware of dynamic allocation) */
	if ((server_cert = SSL_get_peer_certificate(ssl)) == NULL) {
		eb_debug(DBG_CORE,
			"server_cert is NULL ! this _should_not_ happen !\n");
		SSL_free(ssl);
		return NULL;
	}

	if (!ssl_certificate_check(server_cert, host, port, data)) {
		X509_free(server_cert);
		SSL_free(ssl);
		return NULL;
	}

	X509_free(server_cert);

	return ssl;
}

void ssl_done_socket(SSL *ssl)
{
	if (ssl)
		SSL_free(ssl);
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
