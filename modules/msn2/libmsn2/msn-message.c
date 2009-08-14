
/* Here we handle stuff related to messages like receiving and building,
 * sending, etc. */

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "intl.h"

#include "msn-message.h"
#include "msn-connection.h"
#include "msn.h"
#include "msn-util.h"
#include "msn-ext.h"


enum {
	NEW_STATE = -4,
	COMMAND_STATE = -3,
	NEWLINE_STATE = -2,
	PAYLOAD_STATE = -1,
	COMPLETE_STATE = 0
};

/**
 * Ok, now this function accepts message chunks and tries to make an MSN message out of 
 * it. It basically processes strings in various states:
 *
 * => NEW_STATE: This is the beginning of the message. Here's where we allocate memory
 * for the argv[]
 *
 * => COMMAND_STATE: Here we parse MSN commands and put them into the argv[]. argc gives
 * the number of arguments to the command, including the command string itself. One may
 * also identify the command at this stage
 *
 * => NEWLINE_STATE: Here we decide if the command has a payload or not
 *
 * => PAYLOAD_STATE: Here, we fill in the payload that follows some commands
 *
 * => COMPLETE_STATE: Here we get the hell out of this function.
 *
 * When state is 0, the message is complete
 *
 * Return: number of characters remaining to be processed
 **/
int msn_message_concat(MsnMessage *msg, char *input, int len)
{
	int chars_processed = 0;
	char *buf = input;
	char *delimit = NULL;

	while(chars_processed < len && msg->state != COMPLETE_STATE) {
		switch (msg->state) {
		case NEW_STATE:
			if(!msg->argv) {
				msg->argv = m_new0(char *, 10);	/* 10 args ought to be enough for anybody */
				msg->argc = 0;
				msg->state = COMMAND_STATE;
			}
        
			break;

		case COMMAND_STATE:
		{
			char *cmd_end = NULL;

			int snippet_len, arg = 0, old_len;

			delimit = strchr(buf, ' ');
			cmd_end = strstr(buf, "\r\n");

			/* We don't want to accidentally NULL out a space in the payload */
			if ( cmd_end && (!delimit || cmd_end < delimit) ) {
				delimit = cmd_end;
				msg->state = NEWLINE_STATE;
				*delimit = '\0';
				delimit+=2;
				arg++;
			}
			else if( (delimit = strchr(buf, ' ')) ) {
				*delimit = '\0';
				delimit++;
				arg++;
			}

			snippet_len = strlen(buf);
			old_len = msg->argv[msg->argc]?strlen(msg->argv[msg->argc]):0;

			if(!old_len) {
				msg->argv[msg->argc] = m_new0(char, snippet_len+1);
			}
			else {
				msg->argv[msg->argc] = m_renew(char, msg->argv[msg->argc], 
								old_len+snippet_len+1);
			}

			strcat(msg->argv[msg->argc], buf);

			if(arg)
				msg->argc++;

			if(delimit) {
				if (msg->argc == 1)
					msg->command = msn_command_get_from_string(msg->argv[0]);

				buf = delimit;
				chars_processed = (buf - input)/sizeof(char);
			}
			else
				chars_processed = len;

			if(msg->state != NEWLINE_STATE)	/* Continue processing if we have found a newline */
				break;
		}
		case NEWLINE_STATE:
			if( msn_command_set_payload_size(msg) ) {
				msg->state = PAYLOAD_STATE;

				msg->payload_offset = 0;
				msg->payload = m_new0(char, (msg->size+1));
			}
			else
				msg->state = COMPLETE_STATE;

			break ;
		case PAYLOAD_STATE:
		{
			int size = 0;

			if ((msg->size - msg->payload_offset) < (len - chars_processed) )
				size = msg->size - msg->payload_offset;
			else
				size = len - chars_processed;

			memcpy(msg->payload + msg->payload_offset, buf, size);
			msg->payload_offset += size;

			chars_processed += size;

			if ( msg->payload_offset == msg->size ) {
				msn_command_parse_payload(msg);
				msg->state = COMPLETE_STATE;
			}
		}
		}
	}

	return len-chars_processed;
}


/*
 * Send a command to the server:
 * ma: Using this account
 * msg: This should have payload information if I need it
 * cmd: The command
 * ...: The arguments that go into this command. The number of arguments is
 * 	determined by the command.
 *
 * Why don't I build a MsnMessage struct like a good boy and then send it?
 * Because it's faster this way.
 */
void msn_message_send(MsnConnection *mc, const char *payload, MsnCommand cmd, ...)
{
	int count = 0;
	int remaining;
	va_list ap;
	int num_args = 0;

	char buf[8192];

	memset(buf, 0, sizeof(buf));

	remaining = sizeof(buf) -1;

	snprintf( buf, sizeof(buf), "%s %d ", msn_command_get_name(cmd), ++mc->trid );

	remaining -= strlen(buf);

	va_start(ap, cmd);

	num_args = msn_command_get_num_args(cmd) - 1 ;

	/* -1 implies variable args. So the user has supplied it as first arg */
	if(num_args<0)
		num_args = va_arg(ap, int);

	while ( count < num_args ) {
		char *arg = va_arg(ap, char *);

		strncat( buf, arg, remaining );

		if(count < num_args - 1)
			strcat(buf, " ");

		remaining -= strlen(arg) + 1;

		count++;
	}

	strncat( buf, "\r\n", remaining );

	/* Put in the payload if it is not NULL */
	if ( payload )
		strncat( buf, payload, remaining - 2 );

	msn_connection_send_data(mc, buf, -1);

	va_end(ap);
}


void msn_message_handle_incoming(MsnConnection *mc)
{
	MsnMessage *msg = mc->current_message;

	if ( !(msn_command_handle(mc)) ) {
		char buf[255];
		snprintf(buf, sizeof buf, _("Unable to handle message: %s"), msg->argv[0]);
		ext_show_error(mc, buf);
	}
}


MsnMessage *msn_message_new(void)
{
	MsnMessage *out = m_new0(MsnMessage, 1);
	out->payload = m_new0(char, MAX_PAYLOAD_SIZE+1);
	out->capacity = MAX_PAYLOAD_SIZE;
	out->state = NEW_STATE;

	return out;
}


void msn_message_free(MsnMessage *msg)
{
	int i;

	for(i=0; i< msg->argc; i++)
		free(msg->argv[i]);
	
	free(msg->argv);
	free(msg->payload);
	free(msg);
}


void msn_message_check_ack(MsnConnection *mc)
{
	if(!strcmp(mc->current_message->argv[0], "NACK"))
		ext_do_warning(mc->account, MSN_MESSAGE_DELIVERY_FAILED);
}


/* TODO Rich text messages */
void msn_send_IM_to_sb(MsnConnection *sb, MsnIM *im)
{
	char buf[2048];
	char bufsize[5];
	int len;

	if(im->typing)
		len = snprintf(buf, sizeof(buf), "MIME-Version: 1.0\r\n"
					"Content-Type: text/x-msmsgscontrol\r\n"
					"TypingUser: %s\r\n"
					"\r\n\r\n", sb->account->passport);
       else 
		len = snprintf(buf, sizeof(buf), "MIME-Version: 1.0\r\n"
					"Content-Type: text/plain; charset=UTF-8\r\n"
					"\r\n"
					"%s", im->body);

	sprintf(bufsize, "%d", len);

	msn_message_send(sb, buf, MSN_COMMAND_MSG, 2, im->typing?MSN_MSG_NO_ACK:MSN_MSG_ACK_ALL, bufsize);
	msn_connection_push_callback(sb, msn_message_check_ack);
	free(im);
}


static void msn_send_sb_IM(MsnConnection *sb, int error, void *data)
{
	MsnBuddy *buddy = data;
	LList *l;

	if(error) {
		ext_show_warning(sb->account, MSN_MESSAGE_DELIVERY_FAILED);
		return;
	}

	buddy->sb = sb;
	buddy->connecting = 0;

	ext_got_IM_sb(sb, buddy);

	for(l = buddy->mq; l; l = l_list_remove(l, l->data) )
		msn_send_IM_to_sb(sb, (MsnIM *)l->data);

	buddy->mq = NULL;
}


void msn_send_IM(MsnAccount *ma, MsnBuddy *buddy)
{
	LList *l = buddy->mq;
	int typing = 1;

	while(l) {
		MsnIM *im = l->data;

		if(!im->typing) {
			typing = 0;
			break;
		}

		l = l_list_next(l);
	}

	/* We don't want to initiate a call only for a typing message. 
	 * Only let it be queued */

	if(buddy->sb)
		msn_send_sb_IM(buddy->sb, 0, buddy);
	else if(!buddy->connecting && !typing) {
		msn_get_sb(ma, buddy->passport, buddy, msn_send_sb_IM);
		buddy->connecting = 1;
	}
}


int msn_message_is_error(MsnConnection *mc)
{
	MsnMessage *msg = mc->current_message;
	int errnum;

	if((errnum = atoi(msg->argv[0]))) {
		const MsnError *error = msn_strerror(errnum);
		ext_msn_error(mc, error);

		/* The connection and hence the message will have been
		 * freed if this is fatal */
		if(!error->fatal && !error->nsfatal)
			msn_connection_free_current_message(mc);

		return 1;
	}

	return 0;
}


