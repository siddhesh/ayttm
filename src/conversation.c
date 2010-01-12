/*
 * Ayttm
 *
 * Copyright (C) 2009, 2010 the Ayttm team
 * 
 * Ayttm is derivative of Everybuddy
 * Copyright (C) 1999-2002, Torrey Searle <tsearle@uci.edu>
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
 * conversation.c
 * implementation for the conversation window
 * This is the window where you will be doing most of your talking :)
 */

#include "intl.h"

#include <string.h>
#include <stdlib.h>
#include <gdk/gdkkeysyms.h>
#include <ctype.h>
#include <regex.h>

#include "auto_complete.h"

#ifdef HAVE_ICONV_H
#include <iconv.h>
#include <errno.h>
#endif

#include "util.h"
#include "add_contact_window.h"
#include "sound.h"
#include "dialog.h"
#include "prefs.h"
#include "gtk_globals.h"
#include "status.h"
#include "away_window.h"
#include "message_parse.h"
#include "plugin.h"
#include "contact_actions.h"
#include "smileys.h"
#include "service.h"
#include "action.h"
#include "mem_util.h"
#include "chat_window.h"
#include "conversation.h"

#define BUF_SIZE 1024		/* Maximum message length */
#ifndef NAME_MAX
#define NAME_MAX 4096
#endif

LList *outgoing_message_filters = NULL;
LList *incoming_message_filters = NULL;
LList *outgoing_message_filters_remote = NULL;
LList *outgoing_message_filters_local = NULL;

static LList *conversation_list = NULL;

static const int nb_cr_colors = 9;

void ay_conversation_fellows_append(Conversation *conv, const char *alias,
				    const char *handle)
{
	ConversationFellow *fellow = g_new0(ConversationFellow, 1);

	strncpy(fellow->alias, alias, sizeof(fellow->alias));
	strncpy(fellow->handle, handle, sizeof(fellow->handle));
	fellow->color = conv->num_fellows % nb_cr_colors;

	conv->num_fellows++;

	conv->fellows = l_list_append(conv->fellows, fellow);

	ay_chat_window_fellows_append(conv->window, fellow);
}

void ay_conversation_buddy_arrive(Conversation *conv, const char *alias,
	const char *handle)
{
	char buf[1024];

	ay_conversation_fellows_append(conv, alias, handle);

        if (!strcmp(alias, handle))
		snprintf(buf, sizeof(buf), _("<b>%s has joined the chat.</b>"),
			handle);
	else
		snprintf(buf, sizeof(buf), _("<b>%s (%s) has joined the chat.</b>"),
			alias, handle);

	ay_conversation_display_notification(conv, buf,
		CHAT_NOTIFICATION_JOIN);
}

void ay_conversation_buddy_leave(Conversation *conv, const char *handle)
{
	ay_conversation_buddy_leave_ex(conv, handle, NULL);
}

void ay_conversation_buddy_leave_ex(Conversation *conv, const char *handle,
	const char *message)
{
	char buf[1024];
	LList *l = conv->fellows;
	ConversationFellow *fellow = NULL;

	while (l) {
		fellow = l->data;

		if (!strcmp(fellow->handle, handle)) {
			ay_chat_window_fellows_remove(conv->window, fellow);
			break;
		}
		l = l_list_next(l);
	}

	if (!fellow) {
		eb_debug(DBG_CORE, "Strange, got a nick change for "
			"someone not in our list!\n");
		return;
	}

	snprintf(buf, sizeof(buf), message ? _("<b>%s has left the chat (%s).</b>"):
		_("<b>%s has left the chat.</b>"),
		handle, message);

	ay_conversation_display_notification(conv, buf,
		CHAT_NOTIFICATION_JOIN);
}

void ay_conversation_buddy_chnick(Conversation *conv, const char *handle,
	const char *newalias)
{
	char buf[1024];
	LList *l = conv->fellows;
	ConversationFellow *fellow = NULL;
	char *oldalias = NULL;

	while (l) {
		fellow = l->data;

		if (!strcmp(fellow->handle, handle)) {
			oldalias = strdup(fellow->alias);
			strcpy(fellow->alias, newalias);
			ay_chat_window_fellows_rename(conv->window, fellow);
			break;
		}
		l = l_list_next(l);
	}

	if (!fellow) {
		eb_debug(DBG_CORE, "Strange, got a nick change for "
			"someone not in our list!\n");
		return;
	}

	snprintf(buf, sizeof(buf), _("<b>%s is now %s.</b>"),
		oldalias, fellow->alias);

	ay_conversation_display_notification(conv, buf,
		CHAT_NOTIFICATION_JOIN);
	
	free(oldalias);
}

int ay_conversation_buddy_connected(Conversation *conv, const char *alias)
{
	LList *l = conv->fellows;
	while (l) {
		ConversationFellow *fellow = l->data;

		if (!strcmp(alias, fellow->alias))
			return 1;
		l = l_list_next(l);
	}

	return 0;
}

void ay_conversation_send_message(Conversation *conv, char *text)
{
	gchar buff[BUF_SIZE];
	gchar buff2[BUF_SIZE];
	gchar *o_text = NULL;
	gchar *message;
	struct tm *cur_time;
	time_t t;
	LList *filter_walk;
#ifdef __MINGW32__
	char *recoded;
#endif
	int i = 0;

	if (!text || !text[0])
		return;

	/* determine what is the best account to send to */
	if (conv->contact) {
		conv->preferred =
			find_suitable_remote_account(conv->preferred, conv->contact);

		if (!conv->preferred) {
			if ( !conv->contact->send_offline
				|| !(conv->preferred = can_offline_message(conv->contact)) ) {
				ay_conversation_display_notification(conv,
					_("Cannot send message - user is offline\n"),
					CHAT_NOTIFICATION_ERROR);
				return;
			}
		} else if (!conv->preferred->online && !conv->contact->send_offline) {
			ay_conversation_display_notification(conv,
				_("Cannot send message - user is offline\n"),
				CHAT_NOTIFICATION_ERROR);
			return;
		}
        
		if (conv->local_user && conv->local_user != conv->preferred->ela)
			conv->local_user = NULL;
        
		if (conv->local_user && !conv->local_user->connected)
			conv->local_user = NULL;
        
		/* determine what is the best local account to use */
		if (!conv->local_user)
			conv->local_user =
				find_suitable_local_account_for_remote(conv->preferred,
				conv->preferred->ela);
	}

	if (!conv->local_user) {
		ay_conversation_display_notification(conv,
			_("Cannot send message - no local account found\n"),
			CHAT_NOTIFICATION_ERROR);
		return;
	}

	if (conv->this_msg_in_history) {
		LList *node = NULL, *node2 = NULL;

		for (node = conv->history; node; node = node->next)
			node2 = node;
		free(node2->data);
		node2->data = strdup(text);
		conv->this_msg_in_history = 0;
	} else {
		conv->history = l_list_append(conv->history, strdup(text));
		conv->hist_pos = NULL;
	}

	message = strdup(text);

	/* remote filters */
	for (filter_walk = outgoing_message_filters_remote; filter_walk;
		i++, filter_walk = filter_walk->next) {
		char *(*ifilter) (Conversation *, const char *);

		ifilter = filter_walk->data;

		o_text = ifilter(conv, text);
		free(text);
		text = o_text;

		if (!text)
			return;
	}

	eb_debug(DBG_CORE, "Finished %i remote outgoing filters\n", i);
	i = 0;

	/* local filters */
	for (filter_walk = outgoing_message_filters_local; filter_walk;
		i++, filter_walk = filter_walk->next) {
		char *(*ifilter) (Conversation *, const char *);

		ifilter = filter_walk->data;

		o_text = ifilter(conv, message);
		free(message);
		message = o_text;

		if (!message)
			return;
	}

	eb_debug(DBG_CORE, "Finished %i local outgoing filters\n", i);

	/* end outbound filters */

#ifdef __MINGW32__
	recoded = ay_utf8_to_str(text);
	g_free(text);
	text = recoded;
#endif
	/* TODO: Convert to conversation encoding before we send */

	if (conv->preferred)
		RUN_SERVICE(conv->local_user)->send_im(conv->local_user,
			conv->preferred, text);
	else
		RUN_SERVICE(conv->local_user)->send_chat_room_message(
			conv, text);

	serv_touch_idle();

	if (iGetLocalPref("do_convo_timestamp")) {
		time(&t);
		cur_time = localtime(&t);
		g_snprintf(buff2, BUF_SIZE, "%d:%.2d:%.2d %s",
			cur_time->tm_hour, cur_time->tm_min, cur_time->tm_sec,
			conv->local_user->alias);
	} else
		g_snprintf(buff2, BUF_SIZE, "%s", conv->local_user->alias);

	g_snprintf(buff, BUF_SIZE, "<FONT COLOR=\"#0000ff\"><B>%s:</B></FONT> %s<BR>",
		buff2, message);

	ay_chat_window_print_send(conv->window, buff);

	/* 
	 * If an away message had been sent to this person, reset the away message tracker
	 * It's probably faster to just do the assignment all the time--the test
	 * is there for code clarity.
	 */

	if (conv->away_msg_sent)
		conv->away_msg_sent = (time_t) NULL;

	/* Log the message */
	if (iGetLocalPref("do_logging"))
		ay_log_file_message(conv->logfile, buff);

	g_free(message);
}

void ay_conversation_set_local_account(Conversation *conv, eb_local_account *ela)
{
	conv->local_user = ela;
}

void ay_conversation_send_typing_status(Conversation *conv)
{
	/* typing send code */
	time_t now = time(NULL);

	if (!iGetLocalPref("do_send_typing_notify"))
		return;

	if (now >= conv->next_typing_send) {
		if (!conv->preferred) {
			if (!conv->contact)
				return;
			conv->preferred =
				find_suitable_remote_account(NULL, conv->contact);
			if (!conv->preferred)	/* The remote user is not online */
				return;
		}
		conv->local_user =
			find_suitable_local_account_for_remote(conv->preferred,
			conv->local_user);
		if (conv->local_user == NULL)
			return;

		if (RUN_SERVICE(conv->local_user)->send_typing != NULL)
			conv->next_typing_send =
				now +
				RUN_SERVICE(conv->local_user)->send_typing(conv->
				local_user, conv->preferred);
	}
}

void ay_conversation_display_notification(Conversation *conv, const gchar *message,
	ChatNotificationType type)
{
	char *messagebuf;

	if (!conv || !conv->window)
		return;

	messagebuf = g_strdup_printf("<font color=\"#%06x\">%s</font><br>", type,
		message);

	if (type == CHAT_NOTIFICATION_WORKING)
		ay_chat_window_printr(conv->window, messagebuf);
	else {
		ay_chat_window_print(conv->window, messagebuf);
		if (iGetLocalPref("do_logging"))
			ay_log_file_message(conv->logfile, messagebuf);
	}

	g_free(messagebuf);
}

void eb_chat_window_do_timestamp(struct contact *c, gboolean online)
{
	gchar buff[BUF_SIZE];
	time_t my_time = time(NULL);

	if (!c || !c->conversation || !iGetLocalPref("do_convo_timestamp"))
		return;

	g_snprintf(buff, BUF_SIZE,
		online ?
		_("<b> %s has logged in @ %s.\n</b>"):
		_("<b> %s has logged out @ %s.\n</b>"),
		c->nick, g_strchomp(asctime(localtime(&my_time))));

	ay_conversation_display_notification(c->conversation, buff,
		CHAT_NOTIFICATION_NOTE);
}

void ay_conversation_got_message(Conversation *conv, const gchar *from, 
				 const gchar *o_message)
{
	struct contact *remote_contact = conv->contact;
	eb_account *remote = NULL;
	gchar buff[BUF_SIZE], buff2[BUF_SIZE];
	struct tm *cur_time;
	time_t t;
	LList *filter_walk;
	gchar *message;
	int i = 0;
	char *outmsg = NULL;

	/* init to false so only play if first msg is one received rather than sent */
	/* gboolean firstmsg = FALSE;*/
#ifdef __MINGW32__
	char *recoded;
#endif

	if (!o_message || !strlen(o_message))
		return;

	/* We need to check do the filters and groups only for individuals */
	if (!conv->is_room) {
		char *group_name;
		remote = find_suitable_remote_account(conv->preferred,
			conv->contact);

		if (remote_contact && remote_contact->group
			&& remote_contact->group->name)
			group_name = remote_contact->group->name;
		else
			group_name = _("Unknown");

		/* don't translate this string */
		if (!strncmp(group_name, "__Ayttm_Dummy_Group__",
				strlen("__Ayttm_Dummy_Group__")))
			group_name = _("Unknown");

		/* do we need to ignore this user? If so, do it BEFORE filters 
		 * so they can't DoS us */
		if ((iGetLocalPref("do_ignore_unknown")
				&& !strcmp(_("Unknown"), group_name))
			|| !strcasecmp(group_name, _("Ignore")))
			return;
	}

	/* Inbound filters here - Meredydd */
	message = strdup(o_message);

	for (filter_walk = incoming_message_filters; filter_walk;
		filter_walk = filter_walk->next) {
		char *(*ofilter) (Conversation *, const char *);
		char *otext = NULL;

		i++;
		ofilter = filter_walk->data;

		otext = ofilter(conv, message);
		free(message);
		message = otext;
		if (!message)
			return;
	}

	eb_debug(DBG_CORE, "Finished %i incoming filters\n", i);

	/* end inbound filters */
	
	if (iGetLocalPref("do_convo_timestamp")) {
		gchar *color;

		if (!strcmp(from, conv->local_user->handle))
			color = "#0000ff";
		else
			color = RUN_SERVICE(conv->local_user)->get_color();	/* note do not free() afterwards, may be static */

		time(&t);
		cur_time = localtime(&t);
		g_snprintf(buff2, BUF_SIZE,
			"<FONT COLOR=\"%s\">%d:%.2d:%.2d</FONT> <FONT COLOR=\"%s\">%s:</FONT>",
			color, cur_time->tm_hour, cur_time->tm_min,
			cur_time->tm_sec, color, from);
	} else
		g_snprintf(buff2, BUF_SIZE, "<FONT COLOR=\"%s\">%s:</FONT>",
			RUN_SERVICE(conv->local_user)->get_color(),
			from);

#ifdef __MINGW32__
	recoded = ay_str_to_utf8(message);
	g_free(message);
	message = recoded;
#endif

	outmsg = g_strdup_printf("<B>%s</B> %s<br>", buff2, message);

	ay_chat_window_print_recv(conv->window, outmsg);

	/* Log the message */
	if (iGetLocalPref("do_logging"))
		ay_log_file_message(conv->logfile, outmsg);

	/* 
	 * If user's away and hasn't yet sent the away message in the last 5 minutes,
	 * send, display, & log his away message.
	 * We might want to give the option of whether or not to always send the message.
	 */

	if (!conv->is_room && is_away 
		&& ((time(NULL) - conv->away_msg_sent) > 300)) {

		char *awaymsg = get_away_message();
		eb_account *ea = find_suitable_remote_account(conv->preferred,
			conv->contact);

		if (!ea)
			return;

		RUN_SERVICE(conv->local_user)->send_im(conv->local_user, ea, awaymsg);
		time(&t);
		cur_time = localtime(&t);
		g_snprintf(buff, BUF_SIZE,
			"<FONT COLOR=\"#0000ff\">"
			"<B>%d:%.2d:%.2d %s:</B> %s</FONT><br>",
			cur_time->tm_hour, cur_time->tm_min, cur_time->tm_sec,
			conv->local_user->alias, awaymsg);

		ay_chat_window_print_send(remote_contact->conversation->window, buff);

		/* Note that the time the last away message has been sent */
		remote_contact->conversation->away_msg_sent = time(NULL);

		/* Log it */
		if (iGetLocalPref("do_logging"))
			ay_log_file_message(remote_contact->conversation->logfile,
				buff);

		g_free(awaymsg);
	}

	g_free(message);
	g_free(outmsg);
}

static void eb_restore_last_conv(gchar *file_name, Conversation *conv)
{
	FILE *fp;
	gchar buff[1024], buff2[1024], *buff3, color[8], name[512], *token;
	long location = -1;
	long lastlocation = -1;
	long beforeget;
	gboolean endreached = FALSE;

	if (!(fp = fopen(file_name, "r")))
		return;

	/* find last conversation */
	while (!feof(fp)) {
		beforeget = ftell(fp);
		fgets(buff, 1024, fp);
		if (feof(fp))
			break;
		g_strchomp(buff);
		if (!strncmp(buff, _("Conversation started"),
				strlen(_("Conversation started")))
			|| !strncmp(buff,
				_
				("<HR WIDTH=\"100%%\"><P ALIGN=\"CENTER\"><B>Conversation started"),
				strlen(_("<HR WIDTH=\"100%%\"><P ALIGN=\"CENTER\"><B>Conversation started")))
			|| !strncmp(buff,
				_("<HR WIDTH=\"100%\"><B>Conversation started"),
				strlen(_("<HR WIDTH=\"100%\"><B>Conversation started")))) {
			lastlocation = location;
			location = beforeget;
		}
	}

	if (lastlocation == -1 && location == -1) {
		fclose(fp);
		return;
	}

	if (lastlocation < location)
		lastlocation = location;

	fseek(fp, lastlocation, SEEK_SET);

	if (conv->logfile) {
		conv->logfile->filepos = lastlocation;
		eb_debug(DBG_CORE, "set conv->logfile->filepos to %lu\n",
			conv->logfile->filepos);
	}

	/* now we display the log */
	while (!feof(fp)) {
		fgets(buff, 1024, fp);
		if (feof(fp))
			break;

		g_strchomp(buff);

		if (buff[0] == '<')	/* this is html */
			ay_chat_window_print(conv->window, buff2);
		else if (!strncmp(buff, _("Conversation started"),
				strlen(_("Conversation started")))) {
			snprintf(buff2, 1024,
				"<body bgcolor=#F9E589 width=*><b> %s</b></body>",
				buff);
			ay_chat_window_print(conv->window, buff2);
		} else if (!strncmp(buff, _("Conversation ended"),
				strlen(_("Conversation ended")))) {
			snprintf(buff2, 1024,
				"<body bgcolor=#F9E589 width=*><b> %s</b></body>",
				buff);
			ay_chat_window_print(conv->window, buff2);
			endreached = TRUE;
			break;
		} else {
			int has_space = 0, i = 0;
			strip_html(buff);	/* better safe than sorry */
			strncpy(buff2, buff, sizeof(buff2));

			token = strtok(buff2, ":");
			while (token && token[i]) {
				if (isspace(token[i])) {
					has_space = 1;
					break;
				}
				i++;
			}

			if (token && !has_space && strcmp(buff, token)) {
				/* not happy with this if statement at all! */
				if ((strlen(token) == 3
						&& isdigit((int)token[1])
						&& isdigit(token[2]))
					|| (strlen(token) == 2
						&& isdigit((int)token[1]))) {
					/* we must have time stamps */
					/* allready have hours */
					token = strtok(NULL, ":");	/* minutes */
					if (!token)
						break;	/* need to test this */

					if (!(token = strtok(NULL, ":")))	/* seconds + name */
						break;	/* we were wrong, this isn't a time stamp */

					buff3 = token + strlen(token) + 1;	/* should be the end
										   of the screen name */
					token += 3;
				} else {
					/* no time stamps */
					buff3 = buff2 + strlen(token) + 1;
					token++;
				}
				if (!strncmp(token, conv->contact->nick,
						strlen(conv->contact->nick)))
					strcpy(color, "#ff0000");
				else
					strcpy(color, "#0000ff");

				strncpy(name, buff, buff3 - buff2);
				name[buff3 - buff2] = '\0';
				g_snprintf(buff, BUF_SIZE,
					"<FONT COLOR=\"%s\"><B>%s: </B>%s</FONT><br>",
					color, name, buff3);

				ay_chat_window_print(conv->window, buff);
			} else {
				/* hmm, no ':' must be a non start/blank line */
				ay_conversation_display_notification(conv, buff,
					CHAT_NOTIFICATION_NOTE);
			}
		}
	}
	if (!endreached)
		ay_conversation_display_notification(conv,
			_("Conversation Ended"), CHAT_NOTIFICATION_NOTE);

	fclose(fp);
}

void ay_conversation_chat_with_contact(struct contact *remote_contact)
{
	eb_account *remote_account =
		find_suitable_remote_account(NULL, remote_contact);
	eb_local_account *account = NULL;

	if (!remote_contact)
		return;

	if (remote_account)
		account = find_suitable_local_account_for_remote(remote_account,
			NULL);

	if (!remote_contact->conversation) {
		remote_contact->conversation=
			ay_conversation_new(account, remote_contact,
				remote_contact->nick, 0, 0);

			if (iGetLocalPref("do_restore_last_conv")) {
				gchar buff[NAME_MAX];
				make_safe_filename(buff, remote_contact->nick,
					remote_contact->group->name);
				eb_restore_last_conv(buff,
					remote_contact->conversation);
			}
			conversation_list =
				l_list_append(conversation_list,
				remote_contact->conversation);
			/* init preferred if no choice */
			if (l_list_length(remote_contact->accounts) == 1
				&& remote_account) {
				remote_contact->conversation->preferred =
					remote_account;
			}
	} else
		ay_chat_window_raise(remote_contact->conversation->window, NULL);
}

void ay_conversation_chat_with_account(eb_account *remote_account)
{
	struct contact *remote_contact = remote_account->account_contact;

	if (!remote_account || !remote_contact)
		return;

	ay_conversation_chat_with_contact(remote_contact);

	remote_contact->conversation->local_user = 
		find_suitable_local_account_for_remote(remote_account,
			NULL);

	remote_contact->conversation->preferred = remote_account;
}

void ay_conversation_log_status_changed(eb_account *ea, const gchar *status)
{
	char buff[BUF_SIZE];
	time_t my_time = time(NULL);

	if (!ea || !ea->account_contact
		|| !ea->account_contact->conversation
		|| !ea->account_contact->conversation->logfile
		/* only current account */
		|| ea->account_contact->conversation->preferred != ea
		/* only if this is the correct local account */
		|| ea->ela != ea->account_contact->conversation->local_user)
		return;

	g_snprintf(buff, BUF_SIZE,
		_("<b> %s changed status to %s @ %s.</b>"),
		ea->account_contact->nick, ((status
				&& status[0]) ? status : _("(Online)")),
		g_strchomp(asctime(localtime(&my_time))));

	ay_log_file_message(ea->account_contact->conversation->logfile, buff);
}

void ay_conversation_display_status(eb_account *remote, gchar *message)
{
	struct contact *remote_contact = remote->account_contact;
	char *tmp = NULL;

	if (!remote_contact)
		remote_contact = find_contact_by_handle(remote->handle);

	/* trim @.*part for User is typing */
	if (!remote_contact || !remote_contact->conversation) {
		gchar **tmp_ct = NULL;
		if (strchr(remote->handle, '@')) {
			tmp_ct = g_strsplit(remote->handle, "@", 2);
			remote_contact = find_contact_by_handle(tmp_ct[0]);
			g_strfreev(tmp_ct);
		}
	}

	if (!remote_contact || !remote_contact->conversation)
		return;

	if (message && strlen(message) > 0)
		tmp = g_strdup_printf("%s", message);
	else
		tmp = g_strdup_printf(" ");

	ay_conversation_display_notification(remote_contact->conversation, tmp,
		CHAT_NOTIFICATION_WORKING);

	g_free(tmp);
}

static int conv_counter = 0;

#define gen_conversation_name(buf) { \
		snprintf(buf, sizeof(buf), _("Conversation #%d"), ++conv_counter); \
}

Conversation *ay_conversation_clone_as_room(Conversation *conv)
{
	Conversation *ret;
	char name [255];
	eb_account *ea = find_suitable_remote_account(conv->preferred, conv->contact);
	
	gen_conversation_name(name);

	ret = RUN_SERVICE(conv->local_user)->make_chat_room(name, conv->local_user, 0);

	ay_conversation_invite_fellow(ret, conv->preferred->handle, NULL);

	return ret;
}

#undef gen_conversation_name

Conversation *ay_conversation_new(eb_local_account *local, struct contact *remote,
				  const char *name, int is_room, int is_public)
{
	gchar buff[NAME_MAX];
	Conversation *ret;

	if (iGetLocalPref("do_ignore_unknown")
		&& !strcmp(_("Unknown"), remote->group->name))
		return NULL;

	/* first we allocate room for the new chat window */
	ret = g_new0(Conversation, 1);
	ret->contact = remote;
	ret->away_msg_sent = (time_t) NULL;
	ret->away_warn_displayed = (time_t) NULL;
	ret->preferred = NULL;
	ret->is_public = is_public;
	ret->is_room = is_room;
	ret->local_user = local;

	ret->name = strdup(name);

	if (remote)
		make_safe_filename(buff, ret->name, remote->group->name);
	else
		make_safe_filename(buff, ret->name, GET_SERVICE(local).name);

	ret->logfile = ay_log_file_create(buff);
	ay_log_file_open(ret->logfile, "a");

	ret->window = ay_chat_window_new(ret);

	return ret;
}

void ay_conversation_rename(Conversation *conv, char *new_name)
{
	free(conv->name);
	conv->name = strdup(new_name);

	ay_chat_window_set_name(conv->window);
}

void ay_conversation_end(Conversation *conv)
{
	LList *node, *node2;

	/* 
	 * Some protocols like MSN and jabber require that something
	 * needs to be done when you stop a conversation
	 * for every protocol that requires it we call their terminate
	 * method
	 */

	if (conv->contact) {
		for (node = conv->contact->accounts; node; node = node->next) {
			eb_account *ea = (eb_account *)(node->data);

			if (eb_services[ea->service_id].sc->terminate_chat)
				RUN_SERVICE(ea)->terminate_chat(ea);
		}

		conv->contact->conversation = NULL;
	}
	else
		RUN_SERVICE(conv->local_user)->leave_chat_room(conv);

	for (node2 = conv->history; node2; node2 = node2->next) {
		free(node2->data);
		node2->data = NULL;
	}

	l_list_free(conv->history);

	/* 
	 * if we are logging conversations, time stamp when the conversation
	 * has ended
	 */

	ay_log_file_close(conv->logfile);

	conv->window->conv = NULL;

	ay_chat_window_free(conv->window);
	g_free(conv->name);
	g_free(conv);
}

gchar *ay_chat_convert_outgoing(Conversation *conv, const char *msg)
{
	gchar *encoded;
	GError *error = NULL;

	if (!conv->encoding) {
		return g_strdup(msg);
	}

	/* Blindly convert from UTF-8 to user specified locale */
	encoded =
		g_convert_with_fallback(msg, -1, conv->encoding, "UTF-8", NULL,
		NULL, NULL, NULL);

	if (!encoded) {
		encoded =
			g_strdup_printf
			("Message Conversion Error Code %d: %s\n", error->code,
			error->message);
	}

	return encoded;
}

gchar *ay_chat_convert_incoming(Conversation *conv, const char *msg)
{
	gchar *encoded;
	GError *error = NULL;

	if (!conv->encoding) {
		return g_strdup(msg);
	}

	/* Blindly convert from user specified locale to UTF-8 */
	encoded =
		g_convert_with_fallback(msg, -1, "UTF-8", conv->encoding, NULL,
		NULL, NULL, NULL);

	if (!encoded) {
		encoded =
			g_strdup_printf
			("Message Conversion Error Code %d: %s\n", error->code,
			error->message);
	}

	return encoded;
}

void ay_conversation_set_encoding(const char *value, void *data)
{
	Conversation *conv = data;

	if (conv->encoding) {
		g_free(conv->encoding);
		conv->encoding = NULL;
	}

	if (value && value[0]) {
		conv->encoding = g_strdup(value);
	}
}

Conversation *ay_conversation_find_by_name(eb_local_account *ela, const char *name)
{
	LList *l = chat_window_list;

	while (l) {
		chat_window *cw = l->data;

		if (cw->conv->local_user == ela && !strcmp(cw->conv->name, name))
			return cw->conv;

		l = l_list_next(l);
	}

	return NULL;
}

void ay_conversation_invite_fellow(Conversation *conv, const char *fellow,
	const char *message)
{
	RUN_SERVICE(conv->local_user)->send_invite(conv->local_user, conv,
		fellow, message);
}
