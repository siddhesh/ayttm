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

#include "externs.h"
#include "plugin_api.h"
#include "prefs.h"
#include "util.h"
#include "tcp_util.h"
#include "messages.h"
#include "platform_defs.h"


/* already declared in dialog.h - but that uses gtk */
void do_list_dialog(char * message, char * title, char **list,
		    void (*action) (char *text, gpointer data),
		    gpointer data);
			
/*******************************************************************************
 *                             Begin Module Code
 ******************************************************************************/
/*  Module defines */
#define plugin_info autotrans_LTX_plugin_info
#define module_version autotrans_LTX_module_version


/* Function Prototypes */
static char *translate_out(const eb_local_account * local, const eb_account * remote,
			    const struct contact *contact, const char * s);
static char *translate_in(const eb_local_account * local, const eb_account * remote,
			   const struct contact *contact, const char * s);

static int trans_init();
static int trans_finish();
static void language_select(ebmCallbackData * data);

static int doTrans = 0;
static char myLanguage[MAX_PREF_LEN] = "en";

static void *tag1;
static void *tag2;

static int ref_count = 0;


/*  Module Exports */
PLUGIN_INFO plugin_info = {
	PLUGIN_FILTER,
	"Auto-translation",
	"Automatic translation of messages using Babelfish",
	"$Revision: 1.9 $",
	"$Date: 2003/05/07 14:25:12 $",
	&ref_count,
	trans_init,
	trans_finish,
	NULL
};
/* End Module Exports */

unsigned int module_version() {return CORE_VERSION;}

static int trans_init()
{
	input_list *il = calloc(1, sizeof(input_list));
	plugin_info.prefs = il;

	il->widget.checkbox.value = &doTrans;
	il->widget.checkbox.name = "doTrans";
	il->widget.checkbox.label = strdup(_("Enable automatic translation"));
	il->type = EB_INPUT_CHECKBOX;

	il->next = calloc(1, sizeof(input_list));
	il = il->next;
	il->widget.entry.value = myLanguage;
	il->widget.entry.name = "myLanguage";
	il->widget.entry.label = strdup(_("My language code:"));
	il->type = EB_INPUT_ENTRY;

	eb_debug(DBG_MOD, "Auto-trans initialised\n");

	outgoing_message_filters = l_list_append(outgoing_message_filters, &translate_out);
	incoming_message_filters = l_list_append(incoming_message_filters, &translate_in);

	/* the following is adapted from notes.c */

	if ((tag1 = eb_add_menu_item(_("Set Language"), EB_CHAT_WINDOW_MENU,
			      language_select, ebmCONTACTDATA, NULL)) == NULL) {
		eb_debug(DBG_MOD, "Error!  Unable to add Language menu to chat window menu\n");
		return (-1);
	}

	if ((tag2 = eb_add_menu_item(_("Set Language"), EB_CONTACT_MENU,
			      language_select, ebmCONTACTDATA, NULL)) == NULL) {
		eb_remove_menu_item(EB_CHAT_WINDOW_MENU, tag1);
		eb_debug(DBG_MOD, "Error!  Unable to add Language menu to contact menu\n");
		return (-1);
	}

	return 0;
}

static int trans_finish()
{
	int result = 0;

	eb_debug(DBG_MOD, "Auto-trans shutting down\n");
	outgoing_message_filters = l_list_remove(outgoing_message_filters, &translate_out);
	incoming_message_filters = l_list_remove(incoming_message_filters, &translate_in);
	
	while(plugin_info.prefs) {
		input_list *il = plugin_info.prefs->next;
		free(plugin_info.prefs);
		plugin_info.prefs = il;
	}

	/* This is also from utility.c */

	result = eb_remove_menu_item(EB_CHAT_WINDOW_MENU, tag1);
	if (result) {
		g_warning("Unable to remove Language menu item from chat window menu!");
		return (-1);
	}
	result = eb_remove_menu_item(EB_CONTACT_MENU, tag2);
	if (result) {
		g_warning("Unable to remove Language menu item from chat window menu!");
		return (-1);
	}

	return 0;
}

/*******************************************************************************
 *                             End Module Code
 ******************************************************************************/
static void language_selected(char *text, gpointer data)
{
	struct contact *cont = (struct contact *) data;

	cont->language[0] = text[0];
	cont->language[1] = text[1];
	cont->language[2] = '\0';

	write_contact_list();

	if (!doTrans) {
		ay_do_warning( _("Auto-Translation Warning"), _("You have just selected a language "
					"with which to talk to a buddy. This will "
					"only affect you if you have the auto-translator"
					"plugin turned on. If you do, beware that it will"
					"hang each time you send or receive a message, for"
					"the time it takes to contact BabelFish. This can"
					"take several seconds.") );
	}
}

static void language_select(ebmCallbackData * data)
{
	ebmContactData *ecd = (ebmContactData *) data;
	struct contact *cont;
	char buf[1024];

	char **languages;

	/* FIXME: do_list_dialog frees the list passed into it. How ridiculous! */
	languages = (char **) malloc(15 * sizeof(char *));
	languages[0] = strdup("en (English)");
	languages[1] = strdup("fr (French)");
	languages[2] = strdup("de (German)");
	languages[3] = strdup("it (Italian)");
	languages[4] = strdup("pt (Portuguese)");
	languages[5] = strdup("es (Spanish)");
	languages[6] = strdup("ru (Russian)");
	languages[7] = strdup("ko (Korean)");
	languages[8] = strdup("ja (Japanese)");
	languages[9] = strdup("zh (Chinese)");
	languages[10] = NULL;

	cont = find_contact_by_nick(ecd->contact);

	if (cont == NULL) {
		return;
	}

	g_snprintf(buf, 1024, _("Select the code for the language to use when talking to %s"),
		   cont->nick);

	do_list_dialog(buf, _("Select Language"), languages,
		       &language_selected, (gpointer) cont);
}

/*
** Name:    Utf8ToStr
** Purpose: revert UTF-8 string conversion
** Input:   in     - the string to decode
** Output:  a new decoded string
*/
static char *Utf8ToStr(const char *in)
{
	int i = 0;
	unsigned int n;
	char *result = NULL;
	result = (char *) malloc(strlen(in) + 1);

	/* convert a string from UTF-8 Format */
	for (n = 0; n < strlen(in); n++) {
		unsigned char c = in[n];

		if (c < 128) {
			result[i++] = (char) c;
		} else {
			result[i++] = (c << 6) | (in[++n] & 63);
		}
	}
	result[i] = '\0';
	return result;
}

static int isurlchar(unsigned char c)
{
	return (isalnum(c) || '-' == c || '_' == c);
}

static char *trans_urlencode(const char *instr)
{
	int ipos=0, bpos=0;
	char *str = NULL;
	int len = strlen(instr);

	if(!(str = malloc(sizeof(char) * (3*len + 1)) ))
		return strdup("");

	while(instr[ipos]) {
		while(isurlchar(instr[ipos]))
			str[bpos++] = instr[ipos++];
		if(!instr[ipos])
			break;
		
		snprintf(&str[bpos], 4, "%%%.2x", 
				(instr[ipos]=='\r' || instr[ipos]=='\n'?
				 ' ':instr[ipos]));
		bpos+=3;
		ipos++;
	}
	str[bpos]='\0';

	/* free extra alloc'ed mem. */
	len = strlen(str);
	str = realloc(str, sizeof(char) * (len+1));

	return str;
}

static int do_http_post(const char *host, const char *path, const char *enc_data)
{
	char buff[1024];
	int fd;

	fd = ay_socket_new(host, 80);

	if (fd > 0) {
		snprintf(buff, sizeof(buff),
			 "POST %s HTTP/1.1\r\n"
			 "Host: %s\r\n"
			 "User-Agent: Mozilla/4.5 [en] (%s/%s)\r\n"
			 "Content-type: application/x-www-form-urlencoded\r\n"
			 "Content-length: %d\r\n"
			 "\r\n",
			 path, host, PACKAGE, VERSION, strlen(enc_data));

		write(fd, buff, strlen(buff));
		write(fd, enc_data, strlen(enc_data));
	}

	return fd;
}

#define START_POS "<input type=hidden  name=\"q\" value=\""
#define END_POS   "\">"
static char *doTranslate(const char * ostring, const char * from, const char * to)
{
	char buf[2048];
	int fd;
	int offset=0;
	char *string;
	char *result;

	string = trans_urlencode(ostring);
	snprintf(buf, 2048, "tt=urltext&lp=%s_%s&urltext=%s",
		 from, to, string);
	free(string);

	fd = do_http_post("babelfish.altavista.com", "/babelfish/tr", buf);

	while((ay_tcp_readline(buf+offset, sizeof(buf)-offset, fd)) > 0) {
		char *end, *start = strstr(buf, START_POS);
		offset=0;
		if(!start)
			continue;
		
		start += strlen(START_POS);
		end = strstr(start, END_POS);
		if(end) {
			*end='\0';
			string = start;
			break;
		} else {
			/* append next line */
			offset = strlen(buf);
		}
			
	}
	
	eb_debug(DBG_MOD, "Translated %s to %s\n", string, buf);

	result = Utf8ToStr(string);
	eb_debug(DBG_MOD, "%s\n", result);
	return result;
}

static char *translate_out(const eb_local_account * local, const eb_account * remote,
			    const struct contact *contact, const char * s)
{
	char *p;
	if (!doTrans) {
		return strdup(s);
	}

	if (contact->language[0] == '\0') {
		return strdup(s);
	}			// no translation

	if (!strcmp(contact->language, myLanguage)) {
		return strdup(s);
	}			// speak same language

	p = doTranslate(s, myLanguage, contact->language);
	eb_debug(DBG_MOD, "%s translated to %s\n", s, p);
	return p;
}

static char *translate_in(const eb_local_account * local, const eb_account * remote,
			   const struct contact *contact, const char * s)
{
	char *p;
	if (!doTrans) {
		return strdup(s);
	}

	if (contact->language[0] == '\0') {
		return strdup(s);
	}			// no translation

	if (!strcmp(contact->language, myLanguage)) {
		return strdup(s);
	}			// speak same language

	p = doTranslate(s, contact->language, myLanguage);
	return p;
}


