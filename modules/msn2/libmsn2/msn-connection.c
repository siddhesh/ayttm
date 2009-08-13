
#include <string.h>

#include "msn-connection.h"
#include "msn-util.h"
#include "msn-message.h"
#include "msn-errors.h"
#include "msn-http.h"
#include "msn-ext.h"

typedef struct {
	int trid;
	MsnCommandHandler handler;
} MsnCallback;


void msn_connection_destroy(MsnConnection *mc)
{
	if(!mc)
		return;

	if(mc->host)
		free(mc->host);

	if(mc->current_message)
		free(mc->current_message);

	ext_msn_connection_destroyed(mc->ext_data);

	free(mc);
}

void msn_connection_push_callback(MsnConnection *mc, MsnCommandHandler handler)
{
	MsnCallback *cb = m_new0(MsnCallback, 1);

	cb->trid = mc->trid;

	cb->handler = handler;

	mc->callbacks = l_list_append(mc->callbacks, cb);
}


int msn_connection_pop_callback(MsnConnection *mc)
{
	LList *l = mc->callbacks;
	int cur_trid = 0;
	
	if(mc->current_message->argc>1)
		cur_trid = atoi(mc->current_message->argv[1]);

	if(cur_trid == 0)
		return 0;

	while(l) {
		MsnCallback *cb = l->data;
		if ( cb->trid == cur_trid ) {
			mc->callbacks = l_list_remove(mc->callbacks, cb);
			cb->handler(mc);
			return 1;
		}
		l = l_list_next(l);
	}

	return 0;
}


int msn_got_response(MsnConnection *mc, char *response, int len)
{
	int remaining = len;

	if(mc->type == MSN_CONNECTION_HTTP) {
		MsnMessage *msg;
		int curlen;

		if(!mc->current_message)
			mc->current_message = msn_message_new();

		msg = mc->current_message;
		curlen = msg->payload?strlen(msg->payload):0;

		if ( len > (msg->capacity - curlen) ) {
			char *tmp = m_renew(char, msg->payload, len + curlen + 1);

			if(!tmp) abort();	/* Out of memory! */

			msg->payload = tmp;
		}

		strcat(msg->payload, response);

		return msn_http_got_response(mc, len);

	}
	else if (mc->type == MSN_CONNECTION_NS || mc->type == MSN_CONNECTION_SB) {
		if(len == 0) {
			ext_msn_error(mc, MSN_ERROR_NS_CONNECTION_RESET);
			return 1;
		}

		do {
			if(!mc->current_message)
				mc->current_message = msn_message_new();

			remaining = msn_message_concat(mc->current_message, response+len-remaining, remaining);
                
			/* If the state is non-zero then the complete message has not 
			 * arrived yet. Go back out. */
			if(mc->current_message->state)
				return 0;
                
			/*
			 * Check if:
			 * TODO 1) It is an error message OR
			 * 2) It is a response to a previous message
			 *
			 * Otherwise look for the default handler for the message
			 */
			if( msn_connection_pop_callback(mc) == 0 ) 
				msn_message_handle_incoming(mc);

			if( mc->account ) {
				msn_connection_free_current_message(mc);
			}
			else {
				msn_connection_free(mc);
			}
		} while(remaining > 0);
	}

	return 0;
}


void msn_connection_send_data (MsnConnection *mc,char *data,int len) 
{
	ext_msn_send_data(mc,data,len);
}

void msn_connection_free(MsnConnection *mc)
{ 
	ext_msn_free(mc); 
	msn_connection_free_current_message(mc); 
	l_list_free(mc->callbacks); 
	if(mc->host) free(mc->host); 
	free(mc); 
}


