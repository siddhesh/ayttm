
/*
 * IRC protocol Implementation
 *
 * Copyright (C) 2008, Siddhesh Poyarekar and the Ayttm Team
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

#include "ctcp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
 * String data for CTCP data types
 * The CTCPCommandType enumerator elements act as offset to this array
 */
static const CTCPExtData ctcp_command[] = {
	{"ACTION", 6, " allows user to refer to 'perform' action."
			" This is shown by chat clients in 3rd person."},
	{"DCC", 3, " allows direct connection between clients."},
	{"SED", 3, " allows encrypted communication between clients."},
	{"FINGER", 6, " returns user's full name and idle time"},
	{"VERSION", 7, " shows client type, version and environment."},
	{"SOURCE", 6, " shows location from where to download client source code."},
	{"USERINFO", 8, " shows user information"},
	{"CLIENTINFO", 10, " gives information about available CTCP commands."},
	{"ERRMSG", 6, " is used to display and reply to errors."},
	{"PING", 4, " echoes back whatever it receives."},
	{"TIME", 4, " shows the current time on the client system."}
};

#define ctcp_strndup(outstr,str,len) {\
	outstr=(char *)calloc(len+1, sizeof(char)) ; \
	strncpy(outstr,str,len); \
}

static void ctcp_low_encode(char *msg, int size, int mem_size)
{
	int offset, out_offset;
	char *in_msg = NULL;

	ctcp_strndup(in_msg, msg, size);

	for (offset = 0, out_offset = 0; offset <= size; offset++, out_offset++) {
		/* Reallocate if we're falling short of memory... will happen only in rare cases */
		if (out_offset >= mem_size - 1) {
			mem_size += mem_size / 2;
			msg = (char *)realloc(msg, mem_size * sizeof(char));
		}

		if (in_msg[offset] == CTCP_NL) {
			msg[out_offset++] = CTCP_MQUOTE;
			msg[out_offset] = 'n';
		} else if (in_msg[offset] == CTCP_CR) {
			msg[out_offset++] = CTCP_MQUOTE;
			msg[out_offset] = 'r';
		} else {
			msg[out_offset] = in_msg[offset];
		}
	}
}

char *ctcp_encode(const char *msg, int msg_size)
{
	int offset, out_size, out_capacity;
	char *out_msg = NULL;

	out_capacity = msg_size * 2;

	out_msg = (char *)calloc(out_capacity, sizeof(char));

	for (offset = 0, out_size = 0; offset <= msg_size; offset++, out_size++) {

		/* Reallocate if we're falling short of memory... will happen only in rare cases */
		if (out_size >= out_capacity - 1) {
			out_capacity += msg_size / 2;
			out_msg =
				(char *)realloc(out_msg,
				out_capacity * sizeof(char));
		}

		if (msg[offset] == CTCP_TAG) {
			out_msg[out_size++] = CTCP_XQUOTE;
			out_msg[out_size] = 'a';
		} else if (msg[offset] == CTCP_XQUOTE) {
			out_msg[out_size++] = CTCP_XQUOTE;
			out_msg[out_size] = CTCP_XQUOTE;
		} else {
			out_msg[out_size] = msg[offset];
		}
	}

	/* Do low level quoting */
	ctcp_low_encode(out_msg, out_size, out_capacity);

	return out_msg;
}

static void ctcp_low_decode(char *msg, const char *in_msg, int size)
{
	int offset, out_offset;

	for (offset = 0, out_offset = 0; offset <= size; offset++, out_offset++) {
		if (in_msg[offset] == CTCP_MQUOTE) {
			if (in_msg[offset + 1] == '0')
				msg[out_offset] = CTCP_NUL;
			else if (in_msg[offset + 1] == 'n')
				msg[out_offset] = CTCP_NL;
			else if (in_msg[offset + 1] == 'r')
				msg[out_offset] = CTCP_CR;
			else
				msg[out_offset] = in_msg[offset];

			offset++;
		} else {
			msg[out_offset] = in_msg[offset];
		}
	}
}

char *ctcp_decode(const char *msg, int msg_size)
{
	int offset, out_size;
	char *out_msg = NULL;

	out_msg = (char *)calloc(msg_size + 1, sizeof(char));

	/* Do low level quoting */
	ctcp_low_decode(out_msg, msg, msg_size);

	for (offset = 0, out_size = 0; offset <= msg_size; offset++, out_size++) {

		if (msg[offset] == CTCP_XQUOTE) {
			if (msg[offset + 1] == CTCP_XQUOTE)
				out_msg[out_size] = CTCP_XQUOTE;
			else if (msg[offset + 1] == 'a')
				out_msg[out_size] = CTCP_TAG;
			else
				out_msg[out_size] = msg[offset];

			offset++;
		} else {
			out_msg[out_size] = msg[offset];
		}
	}

	return out_msg;
}

void ctcp_free_extended_data(ctcp_extended_data_list *data_list)
{
	while (data_list) {
		ctcp_extended_data_list *elem = data_list;
		data_list = data_list->next;

		if (elem->ext_data) {
			if (elem->ext_data->data) {
				free(elem->ext_data->data);
			}

			free(elem->ext_data);
		}

		free(elem);
	}
}

ctcp_extended_data_list *ctcp_get_extended_data(const char *in_msg, int size)
{
	int delimiter = 0;

	char *msg_start = NULL;
	char *cmd = NULL;
	char *tag_loc = NULL;
	char *temp = NULL;
	int count = 0;

	char *msg = strdup(in_msg);

	msg_start = msg;

	ctcp_extended_data_list *list_head = NULL;
	ctcp_extended_data_list *data_list = NULL;

	if (!in_msg)
		return NULL;

	while ((msg - msg_start) < size
		&& (tag_loc = strchr(msg, CTCP_TAG)) != NULL) {

		*tag_loc = '\0';

		if (data_list == NULL) {
			data_list =
				(ctcp_extended_data_list *)calloc(1,
				sizeof(ctcp_extended_data_list));
			list_head = data_list;
		} else {
			data_list->next =
				(ctcp_extended_data_list *)calloc(1,
				sizeof(ctcp_extended_data_list));
			data_list = data_list->next;
		}

		delimiter = !delimiter;

		data_list->ext_data =
			(ctcp_extended_data *)calloc(1,
			sizeof(ctcp_extended_data));

		/* We found an odd delimiter. The stuff before it is plain text */
		if (delimiter) {
			data_list->ext_data->type = CTCP_NONE;
		} else {
			cmd = strchr(msg, ' ');

			if (cmd) {
				*cmd = '\0';
				temp = msg;
				msg = cmd + 1;
				cmd = temp;
			} else {
				cmd = msg;
				msg = tag_loc;
			}

			for (count = CTCP_NONE + 1; count < CTCP_DATA_COUNT;
				count++) {
				if (!strcmp(cmd, ctcp_command[count].data)) {
					data_list->ext_data->type = count;
					break;
				}
			}

			if (count == CTCP_DATA_COUNT) {
				data_list->ext_data->type = CTCP_NONE;
			}
		}

		data_list->ext_data->data = strdup(msg);
		msg = tag_loc + 1;
	}

	/* if there is anything remaining in the message then put that into the list as well */
	if ((msg - msg_start) < size) {
		if (data_list == NULL) {
			data_list =
				(ctcp_extended_data_list *)calloc(1,
				sizeof(ctcp_extended_data_list));
			list_head = data_list;
		} else {
			data_list->next =
				(ctcp_extended_data_list *)calloc(1,
				sizeof(ctcp_extended_data_list));
			data_list = data_list->next;
		}

		data_list->ext_data =
			(ctcp_extended_data *)calloc(1,
			sizeof(ctcp_extended_data));
		data_list->ext_data->type = CTCP_NONE;
		data_list->ext_data->data = strdup(msg);
	}

	if (msg_start) {
		free(msg_start);
		msg_start = NULL;
	}

	return list_head;
}

char *ctcp_gen_extended_data_request(int type, const char *msg)
{
	char *out_msg = NULL;
	int out_msg_len;

	out_msg_len =
		ctcp_command[type].length + 2 + (msg ? strlen(msg) + 1 : 0);

	out_msg = (char *)calloc((out_msg_len + 1), sizeof(char));

	out_msg[0] = '\001';
	strcat(out_msg, ctcp_command[type].data);

	/* Needed for CLIENTINFO, ERRMSG and PING */
	if (msg) {
		strcat(out_msg, " ");
		strcat(out_msg, msg);
	}

	out_msg[strlen(out_msg)] = '\001';

	return out_msg;
}

char *ctcp_gen_version_response(char *client_name, char *client_version,
	char *client_env)
{
	char *out_msg = NULL;
	int out_msg_len = ctcp_command[CTCP_VERSION].length
		+ (client_name ? strlen(client_name) : 0)
		+ (client_version ? strlen(client_version) : 0)
		+ (client_env ? strlen(client_env) : 0)
		+ 5;

	out_msg = (char *)calloc((out_msg_len + 1), sizeof(char));

	out_msg[0] = '\001';
	strcat(out_msg, ctcp_command[CTCP_VERSION].data);
	out_msg[ctcp_command[CTCP_VERSION].length + 1] = ' ';

	strcat(out_msg, client_name);
	strcat(out_msg, ":");
	strcat(out_msg, client_version);
	strcat(out_msg, ":");
	strcat(out_msg, client_env);

	out_msg[out_msg_len - 1] = '\001';

	return out_msg;
}

ctcp_version *ctcp_got_version(const char *msg)
{
	char *name_offset = NULL;
	char *version_offset = NULL;

	ctcp_version *out = (ctcp_version *)calloc(1, sizeof(ctcp_version));

	if (!msg)
		return NULL;

	name_offset = strchr(msg, ':');

	if (name_offset) {
		ctcp_strndup(out->name, msg, (name_offset - msg));
		version_offset = strchr(name_offset + 1, ':');
	}

	if (version_offset) {
		ctcp_strndup(out->version, (name_offset + 1),
			(version_offset - name_offset - 1));
		out->env = strdup(version_offset + 1);
	}

	return out;
}

char *ctcp_gen_source_response(char *source_host, char *source_path,
	char *source_file)
{
	char *out_msg = NULL;
	int out_msg_len = ctcp_command[CTCP_SOURCE].length
		+ (source_host ? strlen(source_host) : 0)
		+ (source_path ? strlen(source_path) : 0)
		+ (source_file ? strlen(source_file) : 0)
		+ 5;

	out_msg = (char *)calloc((out_msg_len + 1), sizeof(char));

	out_msg[0] = '\001';
	strcat(out_msg, ctcp_command[CTCP_SOURCE].data);
	out_msg[ctcp_command[CTCP_SOURCE].length + 1] = ' ';

	strcat(out_msg, source_host);
	strcat(out_msg, ":");
	strcat(out_msg, source_path);
	strcat(out_msg, ":");
	strcat(out_msg, source_file);

	out_msg[out_msg_len - 1] = '\001';

	return out_msg;
}

ctcp_source *ctcp_got_source(const char *msg)
{
	char *host_offset = NULL;
	char *path_offset = NULL;

	ctcp_source *out = (ctcp_source *)calloc(1, sizeof(ctcp_source));

	if (!msg)
		return NULL;

	host_offset = strchr(msg, ':');

	if (host_offset) {
		ctcp_strndup(out->host, msg, (host_offset - msg));
		path_offset = strchr(host_offset + 1, ':');
	}

	if (path_offset) {
		ctcp_strndup(out->path, (host_offset + 1), (path_offset - msg));
		out->file = strdup(path_offset);
	}

	return out;
}

char *ctcp_gen_time_response(void)
{
	time_t tm;
	char *cur_time = NULL;

	char *out_msg = NULL;
	int out_msg_len;

	tm = time(NULL);
	cur_time = ctime(&tm);

	/* Remove the \n from end of string */
	cur_time[strlen(cur_time) - 1] = '\0';

	out_msg_len =
		ctcp_command[CTCP_TIME].length + 4 +
		(cur_time ? strlen(cur_time) : 0);

	out_msg = (char *)calloc((out_msg_len + 1), sizeof(char));

	out_msg[0] = '\001';
	strcat(out_msg, ctcp_command[CTCP_TIME].data);
	out_msg[ctcp_command[CTCP_TIME].length + 1] = ' ';
	out_msg[ctcp_command[CTCP_TIME].length + 2] = ':';

	strcat(out_msg, cur_time);

	out_msg[out_msg_len - 1] = '\001';

	return out_msg;
}

char *ctcp_gen_clientinfo_response(const char *command)
{
	int count = 0;

	char *out_msg = NULL;

	for (count = CTCP_NONE + 1; count < CTCP_DATA_COUNT; count++) {
		if (!strcmp(command, ctcp_command[count].data)) {

			out_msg = (char *)calloc((ctcp_command[count].length +
					strlen(ctcp_command[count].
						clientinfo_description) + 1)
				, sizeof(char));

			strcpy(out_msg, ctcp_command[count].data);
			strcat(out_msg,
				ctcp_command[count].clientinfo_description);

			break;
		}
	}

	return out_msg;
}

char *ctcp_gen_ping_response(char *timestamp)
{
	char *out_msg = NULL;
	int out_msg_len;

	out_msg_len =
		ctcp_command[CTCP_PING].length + 3 +
		(timestamp ? strlen(timestamp) : 0);

	out_msg = (char *)calloc((out_msg_len + 1), sizeof(char));

	out_msg[0] = '\001';
	strcat(out_msg, ctcp_command[CTCP_PING].data);
	out_msg[ctcp_command[CTCP_PING].length + 1] = ' ';

	strcat(out_msg, timestamp);

	out_msg[out_msg_len - 1] = '\001';

	return out_msg;
}
