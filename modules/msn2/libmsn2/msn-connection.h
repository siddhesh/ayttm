/*
 * Ayttm
 *
 * Copyright (C) 2009, the Ayttm team
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
 * The general design is as follows:
 *
 * Since a single MSN account spawns multiple connections over
 * the lifecycle of a chat, the basic block is the MsnConnection.
 * Hence, the MsnConnection is what you will see being passed 
 * around most of the time. This is different from the other protocol
 * implementations.
 *
 * An MsnConnection belongs to a certain account and can be either a 
 * connection to the SwitchBoard, Name Service for an FTP transfer. 
 * Also, it can either be incoming or outgoing. since you can have an
 * incoming FTP connection as well.
 *
 * Each Connection also has an associated trid, which is incremented
 * each time a message is sent.
 */

#ifndef _MSN_CONNECTION_H_
#define _MSN_CONNECTION_H_

#include <stdlib.h>

#include "msn.h"
#include "llist.h"
#include "msn-message.h"
#include "msn-account.h"

typedef enum {
	MSN_CONNECTION_NS = 1,
	MSN_CONNECTION_SB = 2,
	MSN_CONNECTION_FT = 3,
	MSN_CONNECTION_HTTP = 4,
} MsnConnectionType;

struct _MsnConnection {
	char *host;		/* The host */
	int port;		/* The port */
	int use_ssl;		/* Do we use SSL? Dial 1 for yes */
	int incoming;		/* Whether this is an incoming connection. 0 or false by default */
	MsnConnectionType type;	/* Type of Connection */
	MsnMessage *current_message;	/* The current Message (or data in case of FT) */
	void *ext_data;		/* Any external data you want to attach? */
	MsnAccount *account;	/* The account to which this connection belongs */
	int trid;		/* Transaction ID for this connection */
	LList *callbacks;	/* Callbacks to process responses to previously sent requests */

	SBPayload *sbpayload;	/* This is stuff we may need to send on connecting */

	int tag_r;		/* Identifier for read resource */
	int tag_w;		/* Identifier for write resource */
	int tag_c;		/* Connection tag */
};

typedef void (*MsnConnectionCallback) (MsnConnection *mc);

typedef void (*MsnCallbackHandler) (MsnConnection *mc, void *data);

void msn_connection_push_callback(MsnConnection *mc, MsnCallbackHandler handle,
	void *data);
int msn_connection_pop_callback(MsnConnection *mc);

MsnConnection *msn_connection_new();

#define msn_connection_free_current_message(mc) { \
	if (mc->current_message) { \
		msn_message_free(mc->current_message); \
		mc->current_message = NULL; \
	} \
}

#define msn_connection_connect(mc,callback) { \
	ext_msn_connect(mc, callback); \
}

void msn_connection_send_data(MsnConnection *mc, char *data, int len);

void msn_connection_free(MsnConnection *mc);

int msn_got_response(MsnConnection *mc, char *response, int len);

#endif
