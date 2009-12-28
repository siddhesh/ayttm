/*
 * Ayttm
 *
 * Copyright (C) 2003,2009 the Ayttm team
 * 
 * Ayttm is derivative of Everybuddy
 * Copyright (C) 1998-1999, Torrey Searle
 * proxy feature by Seb C.
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

#ifndef __NETWORKING_H__
#define __NETWORKING_H__

#include "proxy.h"
#include "net_constants.h"
#include "plugin_api.h"

typedef struct _AyConnectionPrivate AyConnectionPrivate;

/* An Opaque Connection object */
typedef struct {
	AyConnectionPrivate *priv;
} AyConnection;

/* Update connection status in requestor */
typedef void (*AyStatusCallback) (const char *msg, void *callback_data);

/* Callback function to be called once connection completes */
typedef void (*AyConnectCallback) (AyConnection *con, AyConnectionStatus error,
	void *callback_data);

/* Callback when input/output fd is ready */
typedef void (*AyInputCallback) (AyConnection *con, eb_input_condition cond,
	void *data);

/* Callback function to be called to make decisions regarding Certificates.
 * Returning a non-zero value results in validation of certificate */
typedef int (*AyCertVerifyCallback) (const char *message, const char *title);

#define AY_CONNECTION(con) ((AyConnection *)con)

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__MINGW32__) && defined(__IN_PLUGIN__)
#define extern __declspec(dllimport)
#endif

	AyConnection *ay_connection_new(const char *host,
		int port, AyConnectionType type);

	void ay_connection_free(AyConnection *con);

	int ay_connection_connect(AyConnection *con,
		AyConnectCallback callback,
		AyStatusCallback status_callback,
		AyCertVerifyCallback verify_callback, void *cb_data);

#define ay_connection_accept ay_connection_connect

	void ay_connection_disconnect(AyConnection *con);

	void ay_connection_cancel_connect(int tag);

	gboolean ay_connection_is_active(AyConnection *con);

	int ay_connection_read(AyConnection *con, void *buff, int len);

	int ay_connection_write(AyConnection *con, const void *buff, int len);

	int ay_connection_input_add(AyConnection *con, eb_input_condition cond,
		AyInputCallback function, void *data);
	void ay_connection_input_remove(int tag);

	char *ay_connection_get_ip_addr(AyConnection *con);

	void ay_connection_free_no_close(AyConnection *con);
	int ay_connection_get_fd(AyConnection *con);

	int ay_connection_listen(AyConnection *con);

	AyConnectionType ay_connection_get_type(AyConnection *con);

	const char *ay_connection_strerror(int error_num);

#if defined(__MINGW32__) && defined(__IN_PLUGIN__)
#define extern extern
#endif

#ifdef __cplusplus
}
#endif
#endif
