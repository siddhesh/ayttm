/*
 * Autotrans module for Ayttm 
 *
 * Copyright (C) 2003, the Ayttm team
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
#ifdef __MINGW32__
#define __IN_PLUGIN__
#include <winsock2.h>
#endif

#include "intl.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

#include "externs.h"
#include "plugin_api.h"
#include "prefs.h"
#include "util.h"
#include "messages.h"
#include "llist.h"
#include "platform_defs.h"
#include "libproxy/networking.h"
#include "chat_window.h"
#include "status.h"

/* already declared in dialog.h - but that uses gtk */
void do_list_dialog(char *message, char *title, char **list,
	void (*action) (char *text, gpointer data), gpointer data);

/*******************************************************************************
 *                             Begin Module Code
 ******************************************************************************/
/*  Module defines */
#ifndef USE_POSIX_DLOPEN
#define plugin_info autotrans_LTX_plugin_info
#define module_version autotrans_LTX_module_version
#endif

/* Function Prototypes */
static char *translate_out(const eb_local_account *local,
	const eb_account *remote, const struct contact *contact, const char *s);

static int trans_init();
static int trans_finish();
static void language_select(ebmCallbackData *data);

static int doTrans = 0;
static int myLanguage = 0;
static char *languages[11];

static void *tag1;
static void *tag2;

static int ref_count = 0;

/*  Module Exports */
PLUGIN_INFO plugin_info = {
	PLUGIN_FILTER,
	"Auto-translation",
	"Automatic translation of messages using Babelfish",
	"$Revision: 1.17 $",
	"$Date: 2009/09/17 12:04:59 $",
	&ref_count,
	trans_init,
	trans_finish,
	NULL
};

/* End Module Exports */

unsigned int module_version()
{
	return CORE_VERSION;
}

static int trans_init()
{
	input_list *il = calloc(1, sizeof(input_list));
	plugin_info.prefs = il;

	languages[0] = "en (English)";
	languages[1] = "fr (French)";
	languages[2] = "de (German)";
	languages[3] = "it (Italian)";
	languages[4] = "pt (Portuguese)";
	languages[5] = "es (Spanish)";
	languages[6] = "ru (Russian)";
	languages[7] = "ko (Korean)";
	languages[8] = "ja (Japanese)";
	languages[9] = "zh (Chinese)";
	languages[10] = NULL;

	il->widget.checkbox.value = &doTrans;
	il->name = "doTrans";
	il->label = _("Enable automatic translation");
	il->type = EB_INPUT_CHECKBOX;

	il->next = calloc(1, sizeof(input_list));
	il = il->next;
	il->widget.listbox.value = &myLanguage;
	il->name = "myLanguage";
	il->label = _("My language code:");
	{
		LList *l = NULL;
		int i;
		for (i = 0; languages[i]; i++)
			l = l_list_append(l, languages[i]);

		il->widget.listbox.list = l;
	}
	il->type = EB_INPUT_LIST;

	eb_debug(DBG_MOD, "Auto-trans initialised\n");

	outgoing_message_filters =
		l_list_prepend(outgoing_message_filters, &translate_out);
	incoming_message_filters =
		l_list_append(incoming_message_filters, &translate_out);

	/* the following is adapted from notes.c */

	if ((tag1 = eb_add_menu_item(_("Set Language"), EB_CHAT_WINDOW_MENU,
				language_select, ebmCONTACTDATA,
				NULL)) == NULL) {
		eb_debug(DBG_MOD,
			"Error!  Unable to add Language menu to chat window menu\n");
		return (-1);
	}

	if ((tag2 = eb_add_menu_item(_("Set Language"), EB_CONTACT_MENU,
				language_select, ebmCONTACTDATA,
				NULL)) == NULL) {
		eb_remove_menu_item(EB_CHAT_WINDOW_MENU, tag1);
		eb_debug(DBG_MOD,
			"Error!  Unable to add Language menu to contact menu\n");
		return (-1);
	}

	return 0;
}

static int trans_finish()
{
	int result = 0;

	eb_debug(DBG_MOD, "Auto-trans shutting down\n");
	outgoing_message_filters =
		l_list_remove(outgoing_message_filters, &translate_out);
	incoming_message_filters =
		l_list_remove(incoming_message_filters, &translate_out);

	while (plugin_info.prefs) {
		input_list *il = plugin_info.prefs->next;
		if (il && il->type == EB_INPUT_LIST)
			l_list_free(il->widget.listbox.list);
		free(plugin_info.prefs);
		plugin_info.prefs = il;
	}

	/* This is also from utility.c */

	result = eb_remove_menu_item(EB_CHAT_WINDOW_MENU, tag1);
	if (result) {
		g_warning
			("Unable to remove Language menu item from chat window menu!");
		return (-1);
	}
	result = eb_remove_menu_item(EB_CONTACT_MENU, tag2);
	if (result) {
		g_warning
			("Unable to remove Language menu item from chat window menu!");
		return (-1);
	}

	return 0;
}

/*******************************************************************************
 *                             End Module Code
 ******************************************************************************/
static void language_selected(char *text, gpointer data)
{
	struct contact *cont = (struct contact *)data;

	cont->language[0] = text[0];
	cont->language[1] = text[1];
	cont->language[2] = '\0';

	write_contact_list();
}

static void language_select(ebmCallbackData *data)
{
	ebmContactData *ecd = (ebmContactData *)data;
	struct contact *cont;
	char buf[1024];

	cont = find_contact_by_nick(ecd->contact);

	if (cont == NULL) {
		return;
	}

	g_snprintf(buf, 1024,
		_("Select the code for the language to use when talking to %s"),
		cont->nick);

	do_list_dialog(buf, _("Select Language"), languages,
		&language_selected, (gpointer) cont);
}

static int isurlchar(unsigned char c)
{
	return (isalnum(c) || '-' == c || '_' == c);
}

static char *trans_urlencode(const char *instr)
{
	int ipos = 0, bpos = 0;
	char *str = NULL;
	int len = strlen(instr);

	if (!(str = malloc(sizeof(char) * (3 * len + 1))))
		return strdup("");

	while (instr[ipos]) {
		while (isurlchar(instr[ipos]))
			str[bpos++] = instr[ipos++];
		if (!instr[ipos])
			break;

		snprintf(&str[bpos], 4, "%%%.2x",
			(instr[ipos] == '\r' || instr[ipos] == '\n' ?
				' ' : instr[ipos]));
		bpos += 3;
		ipos++;
	}
	str[bpos] = '\0';

	/* free extra alloc'ed mem. */
	len = strlen(str);
	str = realloc(str, sizeof(char) * (len + 1));

	return str;
}

struct http_data {
	int done;
	int error;
	char buf[1024];
	int len;
	int handle;
};

#define START_POS "<div id=result_box dir=\"ltr\">"
#define END_POS   "</div>"

static void trim_start(char *buf, int offset, int len)
{
	int i, j;

	if (!offset)
		return;

	for (i=offset, j=0;i<len; i++, j++)
		buf[j] = buf[i];
	
	buf[j] = '\0';
}

static void receive_translation(AyConnection *fd, eb_input_condition cond,
	void *data)
{
	struct http_data *d = data;
	int len = 0;

	while ((len = ay_connection_read(fd, d->buf+d->len, sizeof(d->buf) - d->len)) > 0) {
		char *end, *start;
		
		start = strstr(d->buf, START_POS);

		if (!start)
			continue;

		d->len += len;

		d->buf[len] = '\0';
		trim_start(d->buf, start - d->buf, d->len);
		d->len -= start - d->buf;

		end = strstr(d->buf, END_POS);
		if (end) {
			*end = '\0';
			d->done = 1;
			trim_start(d->buf, strlen(START_POS), d->len);
			ay_connection_input_remove(d->handle);
			return;
		}
	}

	if (len ==0 || (len < 0 && errno != EAGAIN)) {
		d->error = 1;
		d->done = 1;
		ay_connection_input_remove(d->handle);
	}
}

static void http_connected(AyConnection *fd, int error, void *data)
{
	struct http_data *d = data;

	d->error = error;
	d->done = 1;
}

static AyConnection *do_http_post(const char *host, const char *path,
	struct http_data *d)
{
	char buff[1024];
	AyConnection *fd = ay_connection_new(host, 80,
		AY_CONNECTION_TYPE_PLAIN);

	ay_connection_connect(fd, http_connected, NULL, NULL, d);

	/* Busy wait. Yuck */
	while(!d->done)
		do_events();

	if (!d->error) {
		snprintf(buff, sizeof(buff),
			"GET %s HTTP/1.1\r\n"
			"Host: %s\r\n"
			"User-Agent: Mozilla/5.0 [en] (%s/%s)\r\n"
			"\r\n", path, host, PACKAGE, VERSION);

		ay_connection_write(fd, buff, strlen(buff));
	}
	else {
		ay_connection_free(fd);
		fd = NULL;
	}

	return fd;
}

static char *doTranslate(const char *ostring, const char *from, const char *to)
{
	char buf[2048];
	char *result;
	char *string;
	AyConnection *fd;
	struct http_data *d = g_new0(struct http_data, 1);

	string = trans_urlencode(ostring);
	snprintf(buf, 2048, "/translate_t?hl=%s&js=n&text=%s&sl=%s&tl=%s",
		from, string, from, to);
	free(string);

	fd = do_http_post("translate.google.com", buf, d);

	if (!fd)
		return NULL;

	/* Busy wait again. Yuck yuck! */
	d->done = 0;
	d->handle = ay_connection_input_add(fd, EB_INPUT_READ,
		receive_translation, d);
	while (!d->done)
		do_events();

	ay_connection_free(fd);

	if (d->error)
		return NULL;

	eb_debug(DBG_MOD, "Translated %s to %s\n", ostring, d->buf);

	result = g_strdup(d->buf);
	g_free(d);
	return result;
}

static char *translate_out(const eb_local_account *local,
	const eb_account *remote, const struct contact *contact, const char *s)
{
	char *p;
	char l[3];
	if (!doTrans) {
		return strdup(s);
	}

	if (contact->language[0] == '\0') {
		return strdup(s);
	}			/* no translation */

	strncpy(l, languages[myLanguage], 2);
	l[2] = 0;
	if (!strcmp(contact->language, l)) {
		return strdup(s);
	}			/* speak same language */

	eb_chat_window_display_notification(contact->chatwindow,
		_("translating..."), CHAT_NOTIFICATION_WORKING);

	p = doTranslate(s, l, contact->language);

	if (!p) {
		eb_chat_window_display_notification(contact->chatwindow,
			_("Failed to get a translation"),
			CHAT_NOTIFICATION_ERROR);
	}

	eb_debug(DBG_MOD, "%s translated to %s\n", s, p);
	return p;
}
