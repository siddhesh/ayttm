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

#include <string.h>

#include "msn-connection.h"
#include "msn-util.h"
#include "msn-message.h"
#include "msn-errors.h"
#include "msn-http.h"
#include "msn-ext.h"

#define __DEBUG__ 0

typedef struct {
	int trid;
	MsnCallbackHandler handler;
	void *data;
} MsnCallback;

void msn_connection_destroy(MsnConnection *mc)
{
	if (!mc)
		return;

	if (mc->host)
		free(mc->host);

	if (mc->current_message)
		free(mc->current_message);

	ext_msn_connection_destroyed(mc->ext_data);

	free(mc);
}

void msn_connection_push_callback(MsnConnection *mc, MsnCallbackHandler handler,
	void *data)
{
	MsnCallback *cb = m_new0(MsnCallback, 1);

	cb->trid = mc->trid;
	cb->handler = handler;
	cb->data = data;

	mc->callbacks = l_list_append(mc->callbacks, cb);
}

int msn_connection_pop_callback(MsnConnection *mc)
{
	LList *l = mc->callbacks;
	int cur_trid = 0;

	if (mc->current_message->argc > 1)
		cur_trid = atoi(mc->current_message->argv[1]);

	if (cur_trid == 0)
		return 0;

	while (l) {
		MsnCallback *cb = l->data;
		if (cb->trid == cur_trid) {
			mc->callbacks = l_list_remove(mc->callbacks, cb);
			cb->handler(mc, cb->data);
			return 1;
		}
		l = l_list_next(l);
	}

	return 0;
}

int msn_got_response(MsnConnection *mc, char *response, int len)
{
	int remaining = len;

	if (mc->type == MSN_CONNECTION_HTTP) {
		MsnMessage *msg;
		int curlen;

		if (!mc->current_message)
			mc->current_message = msn_message_new();

		msg = mc->current_message;
		curlen = msg->payload ? strlen(msg->payload) : 0;

		if (len > (msg->capacity - curlen)) {
			char *tmp =
				m_renew(char, msg->payload, len + curlen + 1);

			if (!tmp)
				abort();	/* Out of memory! */

			msg->payload = tmp;
		}

		strcat(msg->payload, response);

		return msn_http_got_response(mc, len);

	} else if (mc->type == MSN_CONNECTION_NS
		|| mc->type == MSN_CONNECTION_SB) {
		MsnAccount *ma = mc->account;

		if (len == 0 && mc->type == MSN_CONNECTION_NS) {
			const MsnError *error =
				msn_strerror(MSN_ERROR_CONNECTION_RESET);
			ext_msn_error(mc, error);
			return 1;
		}
		else if (len == 0)
			return 1;

		do {
			if (!mc->current_message)
				mc->current_message = msn_message_new();

			remaining =
				msn_message_concat(mc->current_message,
				response + len - remaining, remaining);

			/* If the state is non-zero then the complete message has not 
			 * arrived yet. Go back out. */
			if (mc->current_message->state)
				return 0;

			/* Skip processing this message if it is an error */
			if (!msn_message_is_error(mc)) {
				/*
				 * Check if this is a response to a previous message
				 * Otherwise look for the default handler for the message
				 */
				if (msn_connection_pop_callback(mc) == 0)
					msn_message_handle_incoming(mc);

				/* Connection could have been freed earlier */
				if (ma->ns_connection) {
					msn_connection_free_current_message(mc);

					/* Orphaned so that it can be freed */
					if (!mc->account) {
						if (mc->type !=
							MSN_CONNECTION_NS)
							ma->connections =
								l_list_remove
								(ma->
								connections,
								mc);

						msn_connection_free(mc);
						return 1;
					}
				}
			}
		} while (remaining > 0);
	}

	return 0;
}

void msn_connection_send_data(MsnConnection *mc, char *data, int len)
{
	ext_msn_send_data(mc, data, len);
}

MsnConnection *msn_connection_new()
{
	MsnConnection *mc = m_new0(MsnConnection, 1);
#if __DEBUG__
	fprintf(stderr, "Creating %p\n", mc);
#endif

	return mc;
}

void msn_connection_free(MsnConnection *mc)
{
#if __DEBUG__
	fprintf(stderr, "Freeing %p\n", mc);
#endif
	if (!mc)
		return;

	ext_msn_free(mc);
	msn_connection_free_current_message(mc);
	l_list_free(mc->callbacks);
	free(mc->host);
	free(mc);
}
