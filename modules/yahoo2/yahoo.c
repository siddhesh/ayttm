/*
 * Yahoo2 Extension for everybuddy 
 *
 * Some code copyright (C) 2002, Philip Tellis <philip.tellis@iname.com>
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

/*
 * yahoo.c
 */

#ifdef __MINGW32__
# include <winsock2.h>
# include <sys/timeb.h>  /* <--  added to allow compilation on Win32 ( see get_time() ) : plasmoidia */
# define EINPROGRESS WSAEINPROGRESS
#else
# include <netdb.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
#endif
#include "intl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#ifdef __MINGW32__
# define __IN_PLUGIN__
#endif

#include "contact.h"
#include "account.h"
#include "service.h"
#include "activity_bar.h"
#include "status.h"
#include "util.h"
#include "message_parse.h"
#include "value_pair.h"
#include "browser.h"
#include "input_list.h"
#include "plugin_api.h"
#include "file_select.h"
#include "smileys.h"
#include "add_contact_window.h"
#include "image_window.h"
#include "tcp_util.h"
#include "messages.h"
#include "dialog.h"
#include "mem_util.h"
#include "video.h"

#include "yahoo2.h"
#include "yahoo2_callbacks.h"
#include "yahoo_util.h"
#include "globals.h"

#include "libproxy/libproxy.h"

#include "pixmaps/yahoo_online.xpm"
#include "pixmaps/yahoo_away.xpm"
#include "pixmaps/yahoo_sms.xpm"

#if defined(HAVE_GLIB)
# include <glib.h>
#endif

/*******************************************************************************
 *                             Begin Module Code
 ******************************************************************************/
/*  Module defines */
#define plugin_info yahoo2_LTX_plugin_info
#define SERVICE_INFO yahoo2_LTX_SERVICE_INFO
#define module_version yahoo2_LTX_module_version


/* Function Prototypes */
static int plugin_init();
static int plugin_finish();
static int reload_prefs();
static void register_callbacks();
static void register_menuentries();
static void unregister_menuentries();

/* called from plugin.c */
struct service_callbacks * query_callbacks();

static int ref_count = 0;
static int is_setting_state = 0;
static int do_yahoo_debug = 0;
static int do_prompt_save_file = 1;
static int do_guess_away = 0;
static int do_show_away_time = 0;

static char pager_host[MAX_PREF_LEN]="scs.msg.yahoo.com";
static char pager_port[MAX_PREF_LEN]="5050";
static char filetransfer_host[MAX_PREF_LEN]="filetransfer.msg.yahoo.com";
static char filetransfer_port[MAX_PREF_LEN]="80";
static char webcam_host[MAX_PREF_LEN]="webcam.yahoo.com";
static char webcam_description[MAX_PREF_LEN]="D-Link USB Digital Video Camera";
static char webcam_port[MAX_PREF_LEN]="5100";
static int conn_type=0;

static void * mywebcam_chat_menu_tag=0;
static void * mywebcam_contact_menu_tag=0;
static void * webcam_chat_menu_tag=0;
static void * webcam_contact_menu_tag=0;

/*  Module Exports */
PLUGIN_INFO plugin_info =
{
	PLUGIN_SERVICE,
	"Yahoo",
	"Provides Yahoo Instant Messenger support",
	"$Revision: 1.94 $",
	"$Date: 2005/11/25 08:02:43 $",
	&ref_count,
	plugin_init,
	plugin_finish,
	reload_prefs,
	NULL
};
struct service SERVICE_INFO = {"Yahoo", -1, 
	SERVICE_CAN_OFFLINEMSG | SERVICE_CAN_GROUPCHAT | SERVICE_CAN_FILETRANSFER | SERVICE_CAN_ICONVERT | SERVICE_CAN_MULTIACCOUNT, 
	NULL};
/* End Module Exports */

unsigned int module_version() {return CORE_VERSION;}

static int plugin_init()
{
	input_list *il = g_new0(input_list, 1);

	eb_debug(DBG_MOD, "yahoo2\n");

	ref_count = 0;
	plugin_info.prefs = il;
	il->widget.entry.value = pager_host;
	il->name = "pager_host";
	il->label= _("Pager Server:");
	il->type = EB_INPUT_ENTRY;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.entry.value = pager_port;
	il->name = "pager_port";
	il->label= _("Pager Port:");
	il->type = EB_INPUT_ENTRY;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.entry.value = filetransfer_host;
	il->name = "filetransfer_host";
	il->label= _("File Transfer Host:");
	il->type = EB_INPUT_ENTRY;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.entry.value = filetransfer_port;
	il->name = "filetransfer_port";
	il->label= _("File Transfer Port:");
	il->type = EB_INPUT_ENTRY;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.entry.value = webcam_host;
	il->name = "webcam_host";
	il->label= _("Webcam Host:");
	il->type = EB_INPUT_ENTRY;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.entry.value = webcam_port;
	il->name = "webcam_port";
	il->label= _("Webcam Port:");
	il->type = EB_INPUT_ENTRY;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.listbox.value = &conn_type;
	il->name = "conn_type";
	il->label= _("Connection type:");
	{
		LList *l=NULL;
		l = l_list_append(l, _("Dialup"));
		l = l_list_append(l, _("DSL/Cable"));
		l = l_list_append(l, _("T1/Lan"));
		il->widget.listbox.list=l;
	}
	il->type = EB_INPUT_LIST;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.checkbox.value = &do_prompt_save_file;
	il->name = "do_prompt_save_file";
	il->label= _("Prompt for transferred filename");
	il->type = EB_INPUT_CHECKBOX;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.checkbox.value = &do_guess_away;
	il->name = "do_guess_away";
	il->label= _("Guess status from Away messages");
	il->type = EB_INPUT_CHECKBOX;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.checkbox.value = &do_show_away_time;
	il->name = "do_show_away_time";
	il->label= _("Show how long contact has been away");
	il->type = EB_INPUT_CHECKBOX;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.checkbox.value = &do_yahoo_debug;
	il->name = "do_yahoo_debug";
	il->label= _("Enable debugging");
	il->type = EB_INPUT_CHECKBOX;

	register_callbacks();
	register_menuentries();
	eb_debug(DBG_MOD, "returning 0\n");
	return (0);
}

static int plugin_finish()
{
	unregister_menuentries();
	while(plugin_info.prefs) {
		input_list *il = plugin_info.prefs->next;
		if(il && il->type == EB_INPUT_LIST) {
			l_list_free(il->widget.listbox.list);
		}
		g_free(plugin_info.prefs);
		plugin_info.prefs = il;
	}
	eb_debug(DBG_MOD, "Returning the ref_count: %i\n", ref_count);
	return (ref_count);
}

static int reload_prefs()
{
	return 0;
}

/*******************************************************************************
 *                             End Module Code
 ******************************************************************************/

#define YAHOO_FONT_SIZE "size=\""

#ifndef FREE
#define FREE(x)	if(x) { g_free((void *)x); x=(void *)NULL; }
#endif

typedef struct {
	int status;
	int away;
	char *status_message;
	int typing_timeout_tag;
} eb_yahoo_account_data;

static void free_yahoo_account(eb_yahoo_account_data * yad)
{
	if(!yad)
		return;
	if(yad->status_message)
		free(yad->status_message);

	free(yad);
}

static void eb_yahoo_free_account_data(eb_account * account)
{
	free_yahoo_account(account->protocol_account_data);
}

typedef struct {
	char password[MAX_PREF_LEN];
	int prompt_password;
	char *act_id;
	int do_mail_notify;
	int login_invisible;
	int initial_state;
	int ignore_system;
	int fd;
	int id;

	int input;
	int timeout_tag;
	int connect_progress_tag;
	int connect_tag;

	int webcam_timeout;
	unsigned int webcam_start;
	int viewers;

	int status;
	char *status_message;
	int away;
	YList *webcams;
} eb_yahoo_local_account_data;

struct webcam_feed {
	int id;
	char *who;
	unsigned timestamp;
	unsigned image_size;
	unsigned recd_size;
	int image_window_tag;

	unsigned char *buff;
	long buff_len;
};

static void _image_window_closed(int, void *);

static struct webcam_feed *find_webcam_feed(eb_yahoo_local_account_data *yla, const char *who)
{
	YList *l;
	struct webcam_feed *wf;
	for(l = yla->webcams; l; l = y_list_next(l)) {
		wf = l->data;
		if((wf->who == who) || (who && wf->who && !strcmp(who, wf->who)))
			return wf;
	}
	return NULL;
}

static void free_yahoo_local_account(eb_yahoo_local_account_data * yla, int free_all)
{
	if(yla->status_message)
		FREE(yla->status_message);

	while(yla->webcams) {
		struct webcam_feed *wf = yla->webcams->data;
		YList *tmp = yla->webcams;
		FREE(wf->who);
		FREE(wf->buff);
		if(wf->image_window_tag) {
			ay_image_window_close(wf->image_window_tag);
			_image_window_closed(wf->image_window_tag, wf);
		}
		FREE(wf);

		yla->webcams = y_list_remove_link(yla->webcams, yla->webcams);
		y_list_free_1(tmp);
	}

	if(free_all)
		free(yla);
}

typedef struct {
	int id;
	eb_account *ea;
} eb_ext_yahoo_typing_notify_data;

typedef struct {
	int id;
	char *label;
} yahoo_idlabel;

struct yahoo_authorize_data {
	int id;
	char *who;
};

typedef struct {
	int id;
	char *host;
	char *room;
	YList *members;
	int connected;
} eb_yahoo_chat_room_data;

typedef struct {
	int id;
	char *from;
	char *url;
	char *fname;
	unsigned long fsize;
	unsigned long transferred;
	long expires;
	int  file;
	int  input;
	int  progress;
} eb_yahoo_file_transfer_data;

enum {
    EB_DISPLAY_YAHOO_ONLINE=0,
    EB_DISPLAY_YAHOO_BRB=1,
    EB_DISPLAY_YAHOO_BUSY=2,
    EB_DISPLAY_YAHOO_NOTATHOME=3,
    EB_DISPLAY_YAHOO_NOTATDESK=4,
    EB_DISPLAY_YAHOO_NOTINOFFICE=5,
    EB_DISPLAY_YAHOO_ONPHONE=6,
    EB_DISPLAY_YAHOO_ONVACATION=7,
    EB_DISPLAY_YAHOO_OUTTOLUNCH=8,
    EB_DISPLAY_YAHOO_STEPPEDOUT=9,
    EB_DISPLAY_YAHOO_INVISIBLE=10,
    EB_DISPLAY_YAHOO_IDLE=11,
    EB_DISPLAY_YAHOO_OFFLINE=12,
    EB_DISPLAY_YAHOO_CUSTOM=13
};

static yahoo_idlabel eb_yahoo_status_codes[] = {
	{YAHOO_STATUS_AVAILABLE, ""},
	{YAHOO_STATUS_BRB, "BRB"},
	{YAHOO_STATUS_BUSY, "Busy"},
	{YAHOO_STATUS_NOTATHOME, "Not Home"},
	{YAHOO_STATUS_NOTATDESK, "Not at Desk"},
	{YAHOO_STATUS_NOTINOFFICE, "Not in Office"},
	{YAHOO_STATUS_ONPHONE, "On Phone"},
	{YAHOO_STATUS_ONVACATION, "On Vacation"},
	{YAHOO_STATUS_OUTTOLUNCH, "Out to Lunch"},
	{YAHOO_STATUS_STEPPEDOUT, "Stepped Out"},
	{YAHOO_STATUS_INVISIBLE, "Invisible"},
	{YAHOO_STATUS_IDLE, "Idle"},
	{YAHOO_STATUS_OFFLINE, "Offline"},
	{YAHOO_STATUS_CUSTOM, "[Custom]"},
	{0, NULL}
};

static int eb_to_yahoo_state_translation[] = {
	YAHOO_STATUS_AVAILABLE,
	YAHOO_STATUS_BRB,
	YAHOO_STATUS_BUSY,
	YAHOO_STATUS_NOTATHOME,
	YAHOO_STATUS_NOTATDESK,
	YAHOO_STATUS_NOTINOFFICE,
	YAHOO_STATUS_ONPHONE,
	YAHOO_STATUS_ONVACATION,
	YAHOO_STATUS_OUTTOLUNCH,
	YAHOO_STATUS_STEPPEDOUT,
	YAHOO_STATUS_INVISIBLE,
	YAHOO_STATUS_IDLE,
	YAHOO_STATUS_OFFLINE,
	YAHOO_STATUS_CUSTOM
};

static int yahoo_to_eb_state_translation(int state)
{
	int i;
	for (i = 0; i <= EB_DISPLAY_YAHOO_CUSTOM; i++)
		if (eb_to_yahoo_state_translation[i] == state)
			return i;
	return EB_DISPLAY_YAHOO_OFFLINE;
}

#define YAHOO_DEBUGLOG ext_yahoo_log

static int ext_yahoo_log(char *fmt,...)
{
	if(do_yahoo_debug) {
		va_list ap;

		va_start(ap, fmt);

		vfprintf(stderr, fmt, ap);
		fflush(stderr);
		va_end(ap);
	}
	return 0;
}


#define LOG(x) if(do_yahoo_debug) { YAHOO_DEBUGLOG("%s:%d: ", __FILE__, __LINE__); \
	YAHOO_DEBUGLOG x; \
	YAHOO_DEBUGLOG("\n"); }

#define WARNING(x) if(do_yahoo_debug) { YAHOO_DEBUGLOG("%s:%d: warning: ", __FILE__, __LINE__); \
	YAHOO_DEBUGLOG x; \
	YAHOO_DEBUGLOG("\n"); }


/* Forward Declarations */

static void eb_yahoo_add_user(eb_account * ea);
static void eb_yahoo_logout(eb_local_account * ela);
static eb_account *eb_yahoo_new_account(eb_local_account *ela, const char * account);


static int eb_yahoo_ping_timeout_callback(gpointer data)
{
	eb_yahoo_local_account_data *ylad = data;
	LOG(("ping"));
	yahoo_keepalive(ylad->id);
	return 1;
}

static struct yahoo_buddy * yahoo_find_buddy_by_handle(int id, char * handle)
{
	const YList * buddies = yahoo_get_buddylist(id);

	for( ; buddies; buddies=buddies->next) {
		struct yahoo_buddy * bud = buddies->data;
		if(!strcmp(bud->id, handle))
			return bud;
	}

	return NULL;
}

static eb_local_account * eb_yahoo_find_active_local_account(eb_account * ea)
{
	LList * node;

	if(ea->ela)
		return ea->ela;

	for (node = accounts; node; node = node->next) {
		eb_local_account *ela = node->data;
		if (ela->connected && ela->service_id == SERVICE_INFO.protocol_id)
			return ela;
	}

	/*WARNING(("No active yahoo accounts"));*/
	return NULL;
}

static eb_local_account *yahoo_find_local_account_by_id(int id)
{
	LList * node;
	for (node = accounts; node; node = node->next) {
		eb_local_account *ela = node->data;
		if (ela && ela->service_id == SERVICE_INFO.protocol_id) {
			eb_yahoo_local_account_data *ylad = ela->protocol_local_account_data;
			if (ylad->id == id)
				return ela;
		}
	}

	WARNING(("Couldn't locate id.  This is a bad thing."));
	return NULL;
}

static void eb_yahoo_decode_yahoo_colors(char *buffer, const char *msg)
{
	char *yahoo_colors[] =
	{
		YAHOO_COLOR_ANY,
		YAHOO_COLOR_BLACK,
		YAHOO_COLOR_BLUE,
		YAHOO_COLOR_LIGHTBLUE,
		YAHOO_COLOR_GRAY,
		YAHOO_COLOR_GREEN,
		YAHOO_COLOR_PINK,
		YAHOO_COLOR_PURPLE,
		YAHOO_COLOR_ORANGE,
		YAHOO_COLOR_RED,
		YAHOO_COLOR_OLIVE
	};
	char *html_colors[] = {
	 "<FONT COLOR=\"",		/* Any */
	 "<FONT COLOR=\"#000000\">",	/* 30 - Black */
	 "<FONT COLOR=\"#000080\">",	/* 31 - Blue */
	 "<FONT COLOR=\"#0000C0\">",	/* 32 - Light Blue */
	 "<FONT COLOR=\"#808080\">",	/* 33 - Gray */
	 "<FONT COLOR=\"#008000\">",	/* 34 - Green */
	 "<FONT COLOR=\"#C000C0\">",	/* 35 - Pink */
	 "<FONT COLOR=\"#800080\">",	/* 36 - Purple */
	 "<FONT COLOR=\"#F95002\">",	/* 37 - Orange */
	 "<FONT COLOR=\"#800000\">",	/* 38 - Red */
	 "<FONT COLOR=\"#00C000\">"	/* 39 - Olive */
	};
	int num_colors = sizeof(yahoo_colors) / sizeof(char *);

	char *yahoo_styles[] =
	{
		YAHOO_STYLE_ITALICON,
		YAHOO_STYLE_ITALICOFF,
		YAHOO_STYLE_BOLDON,
		YAHOO_STYLE_BOLDOFF,
		YAHOO_STYLE_UNDERLINEON,
		YAHOO_STYLE_UNDERLINEOFF
	};
	char *html_styles[] =
	{
		"<I>",
		"</I>",
		"<B>",
		"</B>",
		"<U>",
		"</U>"
	};
	int num_styles = sizeof(yahoo_styles) / sizeof(char *);

	int string_index;
	int color_index;
	int style_index;
	int in_color = 0;
	int match = 0;
	char tmp[2];

	tmp[1] = '\0';
	buffer[0] = '\0';

	for (string_index = 0; msg[string_index]; string_index++) {
		match = 0;
		for (color_index = 0; color_index < num_colors; color_index++) {
			if (!strncmp(yahoo_colors[color_index],
				     &msg[string_index],
				     strlen(yahoo_colors[color_index]))) {
				if (in_color) {
					strcat(buffer, "</FONT>");
				}
				strcat(buffer, html_colors[color_index]);
				string_index += (strlen(
						 yahoo_colors[color_index]) - 1);
				if ( ! color_index ) {
				  char *tbuf = buffer + strlen(buffer);
				  const char *tmsg = msg + string_index;

				  for(  ; *tmsg != 'm' ; string_index ++ ) {
				    * tbuf++ = * tmsg++;
				  }
				  strcpy( tbuf, "\">");
				}
				in_color = 1;
				match = 1;
			}
		}
		for (style_index = 0; style_index < num_styles; style_index++) {
			if (!strncmp(yahoo_styles[style_index],
				     &msg[string_index],
				     strlen(yahoo_styles[style_index]))) {
				strcat(buffer, html_styles[style_index]);
				string_index += (strlen(
					 yahoo_styles[style_index]) - 1);
				match = 1;
			}
		}
		if (!strncmp(YAHOO_STYLE_URLON, &msg[string_index],
			     strlen(YAHOO_STYLE_URLON))) {
			char *end_of_url;
			int length_of_url;

			string_index += (strlen(YAHOO_STYLE_URLON));
			end_of_url = strstr(&msg[string_index],
					    YAHOO_STYLE_URLOFF);
			length_of_url = end_of_url - &msg[string_index];
			if (end_of_url != NULL) {
				strcat(buffer, "<A HREF=\"");
				strncat(buffer, &msg[string_index],
					length_of_url);
				strcat(buffer, "\">");
				match = 1;
			}
			string_index--;
		}
		if (!strncmp(YAHOO_STYLE_URLOFF, &msg[string_index],
			     strlen(YAHOO_STYLE_URLOFF))) {
			strcat(buffer, "</A>");
			string_index += (strlen(YAHOO_STYLE_URLOFF) - 1);
			match = 1;
		}
		if (!strncmp(YAHOO_FONT_SIZE, &msg[string_index],
			     strlen(YAHOO_FONT_SIZE))) {
			strcat(buffer, "PTSIZE=\"");
			string_index += (strlen(YAHOO_FONT_SIZE) - 1);
			match = 1;
		}
		if (!match) {
			tmp[0] = msg[string_index];
			strcat(buffer, tmp);
		}
	}
	if (in_color) {
		strcat(buffer, "</FONT>");
	}
	LOG(("post-color buffer: %s", buffer));
}

static void ext_yahoo_status_changed(int id, char *who, int stat, char *msg, int away)
{
	eb_account *ea;
	eb_yahoo_account_data *yad;
	eb_local_account *ela = yahoo_find_local_account_by_id(id);
	
	ea = find_account_with_ela(who, ela);
	if(!ea) {
		WARNING(("Server set status for unknown: %s\n", who));
		return;
	}

	yad = ea->protocol_account_data;
	FREE(yad->status_message);
	yad->status = stat;
	yad->away = away;

	if(stat == YAHOO_STATUS_OFFLINE) {
		buddy_logoff(ea);
	} else {
		buddy_login(ea);
	}

	if(msg) {
		yad->status_message = g_new(char, strlen(msg) + 1);
		strcpy(yad->status_message, msg);
	}

	buddy_update_status_and_log(ea);
}

struct act_identity {
	int id;
	char * identity;
	void * tag;
};
static YList * identities = NULL;
static void eb_yahoo_set_profile(ebmCallbackData * data)
{
	struct act_identity * i = (void *)data;
	eb_local_account * ela = yahoo_find_local_account_by_id(i->id);
	eb_yahoo_local_account_data * ylad = ela->protocol_local_account_data;

	ylad->act_id = i->identity;
}

static void ext_yahoo_got_identities(int id, YList * ids)
{
	eb_local_account * ela = yahoo_find_local_account_by_id(id);
	eb_yahoo_local_account_data * ylad;
	struct act_identity * i;
	char buff[1024];
	if(!ela)
		return;

	ylad = ela->protocol_local_account_data;
	LOG(("got identities"));
	for(; ids; ids = ids->next) {
		i = g_new0(struct act_identity, 1);
		i->id = id;
		i->identity = strdup((char *)ids->data);
		if(!ylad->act_id)
			ylad->act_id = i->identity;
		LOG(("got %s", i->identity));
		snprintf(buff, sizeof(buff), "%s [Yahoo]", i->identity);
		i->tag = eb_add_menu_item(strdup(buff), EB_PROFILE_MENU, eb_yahoo_set_profile, ebmPROFILEDATA, i);
		identities = y_list_append(identities, i);
	}
}

static void ext_yahoo_got_buddies(int id, YList * buds)
{
	eb_local_account *ela = yahoo_find_local_account_by_id(id);
	eb_yahoo_local_account_data *yla;
	eb_account *ea = NULL;
	int changed = 0;

	for(; buds; buds=buds->next) {
		struct yahoo_buddy *bud = buds->data;
		char *contact_name;
		struct contact *con;
		grouplist *g;

		/*
		if(find_contact_by_handle(bud->id))
			continue;
		*/

		ea = find_account_with_ela(bud->id, ela);

		contact_name = (bud->real_name?bud->real_name:bud->id);

		if(ea) {
			/* nick diff from real_name, but same as handle ==> not changed */
			if(strcmp(ea->account_contact->nick, contact_name) 
				       && !strcmp(ea->account_contact->nick, ea->handle))
			       rename_contact(ea->account_contact, contact_name);
			continue;
		}

		g = find_grouplist_by_name(bud->group);
		con = find_contact_in_group_by_nick(contact_name, g);
		if(!con)
			con = find_contact_in_group_by_nick(bud->id, g);
		if(!con)
			con = find_contact_by_nick(contact_name);
		if(!con)
			con = find_contact_by_nick(bud->id);
		if(!con) {
			changed = 1;
			con=add_new_contact(bud->group, contact_name, SERVICE_INFO.protocol_id);
		}
		ea = eb_yahoo_new_account(ela, bud->id);
		add_account(con->nick, ea);
	}

	if(changed) {
		update_contact_list();
		write_contact_list();
	}

	if(ela) {
		yla = ela->protocol_local_account_data;
		if(yla->connect_progress_tag) {
			ay_activity_bar_remove(yla->connect_progress_tag);
			yla->connect_progress_tag=0;
		}
	}
}

static void ext_yahoo_got_ignore(int id, YList * ign)
{
	eb_local_account *ela = yahoo_find_local_account_by_id(id);
	eb_account *ea = NULL;
	int changed = 0;

	for(; ign; ign = ign->next) {
		struct yahoo_buddy *bud = ign->data;
		char *contact_name;
		struct contact *con;

		/*
		if(find_contact_by_handle(bud->id))
			continue;
		*/

		ea = find_account_with_ela(bud->id, ela);

		if(ea) {
			if(!strcasecmp(ea->account_contact->group->name, bud->group))
				continue;

			
			/* if this person isn't really in the Ignore group, 
			 * we need to change that here */
			continue;
		}

		contact_name = (bud->real_name?bud->real_name:bud->id);

		con = find_contact_by_nick(contact_name);
		if(!con)
			con = find_contact_by_nick(bud->id);
		if(!con) {
			changed = 1;
			if(!find_grouplist_by_name(bud->group)) {
				add_group(bud->group);
			}
			con=add_new_contact(bud->group, contact_name, SERVICE_INFO.protocol_id);
		}
		ea = eb_yahoo_new_account(ela, bud->id);
		add_account(con->nick, ea);
	}

	if(changed) {
		update_contact_list();
		write_contact_list();
	}
}

static void ext_yahoo_got_cookies(int id)
{
	yahoo_get_yab(id);
}

static void ext_yahoo_got_im(int id, char *who, char *msg, long tm, int stat, int utf8)
{
	if(stat == 2) {
		LOG(("Error sending message to %s", who));
		return;
	}

	if(msg) {
		eb_account *sender = NULL;
		eb_local_account *receiver = NULL;
		char buff[2048];

		char *umsg = msg;
		if(utf8)
			umsg = y_utf8_to_str(msg);

		receiver = yahoo_find_local_account_by_id(id);
		sender = find_account_with_ela(who, receiver);
		if (sender == NULL) {
			sender = eb_yahoo_new_account(receiver, who);
			add_dummy_contact(who, sender);
		}

		if(tm) {
			char newmessage[2048];
			char timestr[256];

			strncpy(timestr, ctime(&tm), sizeof(timestr));
			timestr[strlen(timestr) - 1] = '\0';

			snprintf(newmessage, sizeof(newmessage), 
					_("<FONT COLOR=\"#0000FF\">[Offline message at %s]</FONT><BR>%s"),
					timestr, umsg);

			LOG(("<incoming offline message: %s: %s>", who, umsg));
			eb_yahoo_decode_yahoo_colors(buff, newmessage);

		} else {

			LOG(("<incoming message: %s: %s>", who, umsg));
			eb_yahoo_decode_yahoo_colors(buff, umsg);

		}
		eb_parse_incoming_message(receiver, sender, buff);

		if(utf8)
			FREE(umsg);
	}
}

/*************************************
 * File transfer code starts here
 */
static void eb_yahoo_save_file_callback(gpointer data, int fd, eb_input_condition cond)
{
	eb_yahoo_file_transfer_data *yftd = data;
	int file = yftd->file;
	unsigned long count, c;
	char buffer[1024];

	count = read(fd, buffer, sizeof(buffer));

	if(count == 0) {
		eb_input_remove(yftd->input);
		close(file);
		close(fd);

		ay_activity_bar_remove(yftd->progress);

		FREE(yftd->from);
		FREE(yftd->url);
		FREE(yftd->fname);
		FREE(yftd);

		return;
	}

	yftd->transferred += count;
	LOG(("total size: %ld, transferred: %ld\n", yftd->fsize, yftd->transferred));
	ay_progress_bar_update_progress(yftd->progress, yftd->transferred);
	while(count > 0 && (c=write(file, buffer, count)) < count) 
		count -= c;
}

static void eb_yahoo_got_url_handle(int id, int fd, int error, 
		const char *filename, unsigned long size, void *data)
{
	eb_yahoo_file_transfer_data *yftd = data;
	char label[1024]="     ";	/* 5 spaces so that label[5]==NULL */

	if(error || fd<=0) {
		WARNING(("yahoo_get_url_handle returned (%d) %s", error, strerror(error)));
		FREE(yftd->from);
		FREE(yftd->url);
		FREE(yftd->fname);
		FREE(yftd);
		return;
	}

	LOG(("Passed in filename: %s user chosen filename: %s\n", filename, yftd->fname));

	if(!filename)
		filename = yftd->fname;
	if(!yftd->fsize)
		yftd->fsize = size;

	yftd->file = open(filename, O_WRONLY|O_CREAT|O_TRUNC, 
			S_IRUSR|S_IWUSR);

	if(yftd->file <= 0) {
		WARNING(("Could not create file: %s, %s", filename, strerror(errno)));
		close(fd);
		FREE(yftd->from);
		FREE(yftd->url);
		FREE(yftd->fname);
		FREE(yftd);
		return;
	}

	snprintf(label,1024,"Receiving %s...", filename);
	yftd->progress = ay_progress_bar_add(label, yftd->fsize, NULL, NULL);
	yftd->input = eb_input_add(fd, EB_INPUT_READ, eb_yahoo_save_file_callback, yftd);
}

static void eb_yahoo_save_file(const char *filename, gpointer data)
{
	eb_yahoo_file_transfer_data *yftd = data;

	if(!filename) {
		FREE(yftd->from);
		FREE(yftd->url);
		FREE(yftd->fname);
		FREE(yftd);

		return;
	}

	FREE(yftd->fname);
	yftd->fname = g_strdup(filename);

	LOG(("Got filename: %s\n", filename));

	yahoo_get_url_handle(yftd->id, yftd->url, eb_yahoo_got_url_handle, yftd);
}

static void eb_yahoo_accept_file(gpointer data, int result)
{
	eb_yahoo_file_transfer_data *yftd = data;
	if(result) {
		char *filepath, *tmp;

		if(yftd->fname)
			filepath = strdup(yftd->fname);
		else
			filepath = yahoo_urldecode(strchr(yftd->url, '/')+1);

		if(strchr(filepath, '?'))
			*(strchr(filepath, '?')) = '\0';
		tmp = strrchr(filepath, '/');
		if(tmp) {
			tmp = strdup(tmp+1);
			free(filepath);
			filepath=tmp;
		}

		LOG(("yahoo told us file is: %s\n", filepath));
		if(do_prompt_save_file)
			ay_do_file_selection(filepath, _("Save file as"), eb_yahoo_save_file, yftd);
		else
			eb_yahoo_save_file(filepath, yftd);

		FREE(filepath);
	} else {

		FREE(yftd->from);
		FREE(yftd->url);
		FREE(yftd->fname);
		FREE(yftd);
	}
}

static void ext_yahoo_got_file(int id, char *from, char *url, long expires, char *msg, char *fname, unsigned long fsize)
{
	char buff[1024];
	eb_yahoo_file_transfer_data *yftd = g_new0(eb_yahoo_file_transfer_data, 1);
	eb_local_account * ela = yahoo_find_local_account_by_id(id);

	snprintf(buff, sizeof(buff), 
			_("%s, the yahoo user %s has sent you a file%s%s, Do you want to accept it?"),
			ela->handle, from, 
			(msg && *msg ? _(" with the message ") : ""), 
			(msg && *msg ? msg : ""));

	yftd->id = id;
	yftd->from = g_strdup(from);
	yftd->url = g_strdup(url);
	if(fname)
		yftd->fname = g_strdup(fname);
	yftd->fsize = fsize;
	yftd->expires = expires;
	eb_do_dialog(buff, _("Yahoo File Transfer"), eb_yahoo_accept_file, yftd);
}

static void eb_yahoo_send_file_callback(gpointer data, int fd, 
		eb_input_condition cond)
{
	eb_yahoo_file_transfer_data *yftd = data;
	int file = yftd->file;
	unsigned long count, c;
	char buffer[1024];

	LOG(("eb_yahoo_send_file_callback: %d", fd));
	count = read(file, buffer, sizeof(buffer));

	if(count == 0) {
		LOG(("end of file"));
		goto done_sending;
	}

	yftd->transferred += count;
	ay_progress_bar_update_progress(yftd->progress, yftd->transferred);
	while(count > 0 && (c=write(fd, buffer, count)) < count) 
		count -= c;

	if(yftd->transferred >= yftd->fsize) {
		LOG(("transferred >= size"));
done_sending:
		eb_input_remove(yftd->input);
		close(file);
		close(fd);

		ay_activity_bar_remove(yftd->progress);

		FREE(yftd->from);
		FREE(yftd->url);
		FREE(yftd->fname);
		FREE(yftd);
	}
}

static void eb_yahoo_got_fd(int id, int fd, int error, void *data)
{
	eb_yahoo_file_transfer_data *yftd = data;
	char label[1024];

	if(fd <= 0) {
		WARNING(("yahoo_send_file returned (%d) %s", error, strerror(error)));
		FREE(yftd->fname);
		FREE(yftd);
		return;
	}

	snprintf(label,1024,"Sending %s...", yftd->fname);
	yftd->progress = ay_progress_bar_add(label, yftd->fsize, NULL, NULL);	

	yftd->input = eb_input_add(fd, EB_INPUT_WRITE, eb_yahoo_send_file_callback, yftd);
}

static void eb_yahoo_send_file(eb_local_account *from, eb_account *to, char *file)
{
	struct stat stats;
	int in;
	
	eb_yahoo_local_account_data *ylad = from->protocol_local_account_data;
	eb_yahoo_file_transfer_data *yftd;

	if(stat(file, &stats) < 0) {
		WARNING(("Error reading file: %s", strerror(errno)));
		return;
	}
	in = open(file, O_RDONLY);

	yftd = g_new0(eb_yahoo_file_transfer_data, 1);
	/* don't use stat to get the filename because that
	 * may not work with autoconversion file systems.
	 * eg: reading from a DOS partition where the fs driver
	 * automatically does CRLF -> LF conversion causing the
	 * file size to change
	 */
	yftd->fsize = lseek(in, 0, SEEK_END);
	lseek(in, 0, SEEK_SET); /* go back to start of file */
	/* if lseek failed, then use stat instead */
	if(yftd->fsize < 0)
		yftd->fsize = stats.st_size;
	yftd->file = in;
	yftd->fname = strdup(file);

	yahoo_send_file(ylad->id, to->handle, "", file, yftd->fsize,
			eb_yahoo_got_fd, yftd);

}

/*
 * File transfer code ends here
 ********************************/

/********************************
 * Conference code starts here
 */
static void ext_yahoo_conf_userjoin(int id, char *who, char *room)
{
	eb_chat_room *chat_room = NULL;
	eb_local_account *ela = yahoo_find_local_account_by_id(id);
	eb_yahoo_local_account_data * ylad = ela->protocol_local_account_data;
	eb_yahoo_chat_room_data *ycrd;
	eb_account *ea = find_account_with_ela(who, ela);
/*	char buff[1024];*/
	YList * l;

	if(!strcmp(who, ylad->act_id))
		return;

	chat_room = find_chat_room_by_id(room);

	if(!chat_room)
		return;

	eb_chat_room_buddy_arrive(chat_room, (ea?ea->account_contact->nick:who),
			who);

	ycrd = chat_room->protocol_local_chat_room_data;

/*	snprintf(buff, sizeof(buff), _("%s has joined the conference."), who);
	eb_chat_room_show_message(chat_room, _("Yahoo! Messenger"), buff);*/

	for(l = ycrd->members; l; l=l->next) {
		char * handle = l->data;
		if(!strcmp(handle, who))
			return;
	}
	ycrd->members = y_list_append(ycrd->members, strdup(who));
}

static void ext_yahoo_conf_userleave(int id, char *who, char *room)
{
	eb_chat_room *chat_room = find_chat_room_by_id(room);
	eb_yahoo_chat_room_data *ycrd;
/*	char buff[1024]; */
	const YList * l;

	if(!chat_room)
		return;

	ycrd = chat_room->protocol_local_chat_room_data;
	eb_chat_room_buddy_leave(chat_room, who);

/*	snprintf(buff, sizeof(buff), _("%s has left the conference."), who);
	eb_chat_room_show_message(chat_room, _("Yahoo! Messenger"), buff);*/

	
	for(l = ycrd->members; l; l=l->next) {
		char * handle = l->data;
		if(!strcmp(handle, who)) {
			ycrd->members = y_list_remove_link(ycrd->members, l);
			FREE(handle);
			FREE((YList *)l);
			break;
		}
	}
	/* if no more users, then destroy? */
}

static void ext_yahoo_conf_userdecline(int id, char *who, char *room, char *msg)
{
	eb_local_account *ela = yahoo_find_local_account_by_id(id);
	eb_yahoo_local_account_data * ylad = ela->protocol_local_account_data;
	char buff[1024];

	if(!strcmp(ylad->act_id, who))
		return;

	snprintf(buff, sizeof(buff), _("The yahoo user %s declined your invitation to join conference %s, with the message: %s"),
			who, room, msg);

	ay_do_warning( _("Yahoo Error"), buff );
}

#if LOCAL_CHAT_CALLBACKS
static void eb_yahoo_accept_conference(gpointer data, int result)
{
	eb_yahoo_chat_room_data *ycrd = data;
	eb_local_account *ela = yahoo_find_local_account_by_id(ycrd->id);

	if(result) {
		eb_chat_room *chat_room = g_new0(eb_chat_room, 1);
		eb_account *ea = find_account_with_ela(ycrd->host, ela);

		strcpy(chat_room->id, ycrd->room);
		strcpy(chat_room->room_name, ycrd->room);

		chat_room->protocol_local_chat_room_data = ycrd;
		chat_room->fellows = NULL;
		chat_room->connected = FALSE;

		chat_room->local_user = ela;

		eb_join_chat_room(chat_room);
		eb_chat_room_buddy_arrive(chat_room, 
				(ea?ea->account_contact->nick:ycrd->host), 
				ycrd->host);
		eb_chat_room_buddy_arrive(chat_room, ela->alias, ela->handle);

	} else {
		yahoo_conference_decline(ycrd->id, ylad->act_id, ycrd->members, ycrd->room, _("Thanks, but no thanks"));
	}
}

static void ext_yahoo_got_conf_invite(int id, char *who, char *room, char *msg, YList *members)
{
	char buff[1024];
	eb_yahoo_chat_room_data *ycrd = g_new0(eb_yahoo_chat_room_data, 1);
	eb_yahoo_local_account_data *ela = yahoo_find_local_account_by_id(id);

	if(!who || !room)
		return;

	snprintf(buff, sizeof(buff), _("%s, the yahoo user %s has invited you to a conference - %s, the topic is %s.\n")
			_("Do you want to join?"), ela->handle, who, room, msg);

	ycrd->id = id;
	ycrd->host = strdup(who);
	ycrd->room = strdup(room);
	ycrd->members = members;
	eb_do_dialog(buff, _("Yahoo Join Conference"), eb_yahoo_accept_conference, ycrd);
}

static void eb_yahoo_accept_invite(eb_local_account *ela, void * data)
{
}

static void eb_yahoo_decline_invite(eb_local_account *ela, void * data)
{
}

#else
static void eb_yahoo_accept_invite(eb_local_account *ela, void * data)
{
	eb_yahoo_chat_room_data *ycrd = data;
	
	eb_chat_room *chat_room = g_new0(eb_chat_room, 1);
	eb_account *ea;
	YList * l;
	eb_yahoo_local_account_data * ylad = ela->protocol_local_account_data;
	int done_myself=0;

	strcpy(chat_room->id, ycrd->room);
	strcpy(chat_room->room_name, ycrd->room);

	chat_room->protocol_local_chat_room_data = ycrd;
	chat_room->fellows = NULL;
	chat_room->connected = FALSE;

	chat_room->local_user = ela;

	eb_join_chat_room(chat_room);
/*	eb_chat_room_buddy_arrive(chat_room, 
			(ea?ea->account_contact->nick:ycrd->host), 
			ycrd->host);
*/
	for(l = ycrd->members; l; l=l->next) {
		char * handle = l->data;
		if(!strcmp(ylad->act_id, handle)) {
			eb_chat_room_buddy_arrive(chat_room, ela->alias, ylad->act_id);
			done_myself=1;
		} else {
			ea = find_account_with_ela(handle, ela);
			eb_chat_room_buddy_arrive(chat_room, 
					(ea?ea->account_contact->nick:handle), 
					handle);
		}
	}
	if(!done_myself)
		eb_chat_room_buddy_arrive(chat_room, ela->alias, ylad->act_id);
}

static void eb_yahoo_decline_invite(eb_local_account *ela, void * data)
{
	eb_yahoo_chat_room_data *ycrd = data;
	eb_yahoo_local_account_data *ylad = ela->protocol_local_account_data;
	yahoo_conference_decline(ycrd->id, ylad->act_id, ycrd->members, ycrd->room, _("Thanks, but no thanks"));
}

static void ext_yahoo_got_conf_invite(int id, char *who, char *room, char *msg, YList *members)
{
	eb_chat_room * chat_room = NULL;
	eb_yahoo_chat_room_data *ycrd = NULL;
	eb_local_account *ela = yahoo_find_local_account_by_id(id);
	if(!ela)
		return;

	chat_room = find_chat_room_by_id(room);
	if(!chat_room) {
		ycrd = g_new0(eb_yahoo_chat_room_data, 1);
		ycrd->id = id;
		ycrd->host = strdup(who);
		ycrd->room = strdup(room);
		ycrd->members = members;
	} else {
		YList * l;
		ycrd = chat_room->protocol_local_chat_room_data;
		for(l = ycrd->members; l->next; l=l->next)
			;
		l->next = members;
		members->prev = l;
	}

	invite_dialog(ela, who, room, ycrd);
}
#endif

static void ext_yahoo_conf_message(int id, char *who, char *room, char *msg, int utf8)
{
	int i=0, j=0;
	unsigned char * umsg = (unsigned char *)msg;
	eb_chat_room *chat_room = find_chat_room_by_id(room);

	if(!chat_room)
		return;

	while(umsg[i]) {
		if(umsg[i] < (unsigned char)0x80) {	/* 7 bit ASCII */
			umsg[j] = umsg[i];
			i++; 
		} else if(umsg[i] < (unsigned char)0xC4) {/* ISOLatin1 */
			umsg[j] = (umsg[i]<<6) | (umsg[i+1] & 0x3F);
			i+=2;
		} else if(umsg[i] < (unsigned char)0xE0) {
			umsg[j] = '.';
			i+=3;
		} else if(umsg[i] < (unsigned char)0xF0) {
			umsg[j] = '.';
			i+=4;
		}
		j++;
	}
	umsg[j]='\0';

	eb_chat_room_show_message(chat_room, who, umsg);
}

static void eb_yahoo_send_chat_room_message(eb_chat_room * room, char * message)
{
	eb_yahoo_chat_room_data *ycrd;
	eb_yahoo_local_account_data *ylad;
	char * encoded = y_str_to_utf8(message);
		
	if(!room) {
		WARNING(("room is null"));
		return;
	}

	if(!message)
		return;

	ylad = room->local_user->protocol_local_account_data;
	
	ycrd = room->protocol_local_chat_room_data;
	yahoo_conference_message(ycrd->id, ylad->act_id, ycrd->members, ycrd->room, encoded, 1);
	FREE(encoded);
}

static void eb_yahoo_join_chat_room(eb_chat_room * room)
{
	eb_yahoo_chat_room_data *ycrd;
	eb_local_account *ela;
	eb_yahoo_local_account_data *ylad;
	YList * l;

	if(!room) {
		WARNING(("room is null"));
		return;
	}

	ycrd = room->protocol_local_chat_room_data;
	ela = room->local_user;
	ylad = ela->protocol_local_account_data;
	if(!ycrd || !ylad)
		return;
	if(!strcmp(ycrd->host, ylad->act_id))	/* I'm the host */
		return;
	yahoo_conference_logon(ycrd->id, ylad->act_id, ycrd->members, ycrd->room);
	
	for(l = ycrd->members; l; l=l->next) {
		char * handle = l->data;
		if(!strcmp(handle, ylad->act_id))
			return;
	}
	ycrd->members = y_list_append(ycrd->members, strdup(ylad->act_id));
}

static void eb_yahoo_leave_chat_room(eb_chat_room * room)
{
	eb_yahoo_chat_room_data *ycrd;
	eb_yahoo_local_account_data *ylad;

	if(!room) {
		WARNING(("room is null"));
		return;
	}

	ycrd = room->protocol_local_chat_room_data;
	ylad = room->local_user->protocol_local_account_data;
	yahoo_conference_logoff(ycrd->id, ylad->act_id, ycrd->members, ycrd->room);
}

static void eb_yahoo_send_invite(eb_local_account * ela, eb_chat_room * ecr, char *who, char *message)
{
	eb_yahoo_chat_room_data *ycrd;
	eb_yahoo_local_account_data *ylad;

	if(!who || !*who) {
		WARNING(("no one to invite"));
		return;
	}

	ycrd = ecr->protocol_local_chat_room_data;
	ylad = ela->protocol_local_account_data;

	if(!message || !*message)
		message = _("Join my conference");
	if(ycrd->members->next)		/* already someone in here */
		yahoo_conference_addinvite(ylad->id, ylad->act_id, who, ycrd->room, ycrd->members, message);
	else {				/* first time */
		YList * members = NULL;
		members = y_list_append(members, who);
		yahoo_conference_invite(ylad->id, ylad->act_id, members, ycrd->room, message);
		y_list_free(members);
	}
}

static eb_chat_room *eb_yahoo_make_chat_room(char *name, eb_local_account * ela, int is_public)
{
	eb_chat_room *ecr = g_new0(eb_chat_room, 1);
	eb_yahoo_chat_room_data *ycrd = g_new0(eb_yahoo_chat_room_data, 1);
	eb_yahoo_local_account_data *ylad;
	YList *members = NULL;

	if(!ela) {
		WARNING(("ela is null"));
		return NULL;
	}

	ylad = ela->protocol_local_account_data;

	members = y_list_append(members, g_strdup(ylad->act_id));

	if(name && *name)
		strcpy(ecr->room_name, name);
	else
		sprintf(ecr->room_name, "%s-%d", ylad->act_id, ylad->id);

	strcpy(ecr->id, ecr->room_name);
	ecr->fellows = NULL;
	ecr->connected = FALSE;
	ecr->local_user = ela;
	ecr->protocol_local_chat_room_data = ycrd;
	
	ycrd->id = ylad->id;
	ycrd->host = g_strdup(ylad->act_id);
	ycrd->room = g_strdup(ecr->room_name);
	ycrd->members = members;
	ycrd->connected = FALSE;
	
	eb_join_chat_room(ecr);
	eb_chat_room_buddy_arrive(ecr, ela->alias, ylad->act_id);
	return ecr;
}
/*
 * Conference code ends here
 ******************************/

/******************************
 * Yahoo Chat code starts here
 */
static void ext_yahoo_chat_cat_xml(int id, char *xml) 
{
}

static void ext_yahoo_chat_join(int id, char *room, char * topic, YList *members, int fd)
{
}

static void ext_yahoo_chat_userjoin(int id, char *room, struct yahoo_chat_member *who)
{
}

static void ext_yahoo_chat_userleave(int id, char *room, char *who)
{
}

static void ext_yahoo_chat_message(int id, char *who, char *room, char *msg, int msgtype, int utf8)
{
}

/*
 * Yahoo Chat code ends here
 ******************************/

/******************************
 * Webcam code starts here
 */
static void ay_yahoo_stop_webcam(eb_local_account *);

static void _image_window_closed(int tag, void *data)
{
	struct webcam_feed *wf = data;
	eb_local_account *ela;
	eb_yahoo_local_account_data *yla;

	if(!wf || tag != wf->image_window_tag)
		return;

	ela = yahoo_find_local_account_by_id(wf->id);

	if(ela) {
		yla = ela->protocol_local_account_data;
		yla->webcams = y_list_remove(yla->webcams, wf);
	}

	if(wf->who)
		yahoo_webcam_close_feed(wf->id, wf->who);
	else
		ay_yahoo_stop_webcam(ela);

	FREE(wf->who);
	FREE(wf->buff);
	FREE(wf);
}

static void ext_yahoo_got_webcam_image(int id, const char *who,
		const unsigned char *image, unsigned int image_size, unsigned int real_size,
		unsigned int timestamp)
{
	struct webcam_feed *wf = NULL;
	eb_local_account *ela = yahoo_find_local_account_by_id(id);
	eb_yahoo_local_account_data *yla = ela->protocol_local_account_data;

	if(!image_size)
		return;

	if(!(wf = find_webcam_feed(yla, who)))
		return;

	if(timestamp < wf->timestamp)
		return;

	if(!wf->image_window_tag) {
		char buff[1024];
		snprintf(buff, sizeof(buff), _("%s's webcam"), wf->who);
		wf->image_window_tag = ay_image_window_new(320, 240, buff, _image_window_closed, wf);
	}

	if(wf->image_size != image_size || wf->recd_size==0) {
		FREE(wf->buff);
		wf->image_size = image_size;
		wf->timestamp = timestamp;
		wf->recd_size=0;
		wf->buff = malloc(image_size);
	}

	memcpy(wf->buff+wf->recd_size, image, real_size);
	wf->recd_size += real_size;

	if(wf->recd_size == wf->image_size) {
		ay_image_window_add_data(wf->image_window_tag, wf->buff, wf->image_size, 1);
		ay_image_window_add_data(wf->image_window_tag, NULL, 0, 0);
		FREE(wf->buff);
		wf->recd_size=0;
	}

}

static void eb_yahoo_webcam_invite_callback(gpointer data, int result)
{
	struct yahoo_authorize_data *wd = data;

	if(!wd)
		return;
	
	if(result) {
		struct webcam_feed *wf=NULL;
		eb_local_account *ela = yahoo_find_local_account_by_id(wd->id);
		eb_yahoo_local_account_data *yla=NULL;
		if(ela) {
			yla = ela->protocol_local_account_data;
			if((wf = find_webcam_feed(yla, wd->who))) {
				FREE(wd->who);
				yahoo_webcam_close_feed(wf->id, wf->who);
			} else {
				wf = y_new0(struct webcam_feed, 1);
				wf->id = wd->id;
				wf->who = wd->who;
				yla->webcams = y_list_prepend(yla->webcams, wf);
			}
			yahoo_webcam_get_feed(wf->id, wf->who);
		} else
			FREE(wd->who);
	} else
		FREE(wd->who);

	FREE(wd);
}

static void ay_yahoo_view_users_webcam(ebmCallbackData *data)
{
	ebmContactData *ecd = (ebmContactData *)data;
	eb_local_account *ela;
	eb_yahoo_local_account_data *ylad;
	char *who=ecd->remote_account;

	if(!who && ecd->contact && ecd->group) {
		grouplist *g = find_grouplist_by_name(ecd->group);
		struct contact *c = find_contact_in_group_by_nick(ecd->contact, g);
		eb_account *ea = find_account_for_protocol(c, SERVICE_INFO.protocol_id);
		who = ea->handle;
	}

	if(!who) {
		return;
	}

	ela = find_local_account_by_handle(ecd->local_account, SERVICE_INFO.protocol_id);
	if(!ela) {
		ay_do_warning(_("Yahoo Error"), _("Cannot find a valid local account to view user's webcam.") );
		return;
	}

	ylad = ela->protocol_local_account_data;

	yahoo_webcam_get_feed(ylad->id, who);
}

static void ext_yahoo_webcam_invite(int id, char *who)
{
	char buff[1024];
	struct yahoo_authorize_data *wd = g_new0(struct yahoo_authorize_data, 1);
	eb_local_account * ela = yahoo_find_local_account_by_id(id);

	snprintf(buff, sizeof(buff), _("%s, the yahoo user %s has invited you to view their webcam."
				"Do you want to accept?"), ela->handle, who);

	wd->id = id;
	wd->who = strdup(who);
	eb_do_dialog(buff, _("Yahoo Webcam Invitation"), eb_yahoo_webcam_invite_callback, wd);
}

static void eb_yahoo_close_webcam_window(gpointer data, int result)
{
	struct webcam_feed *wf = data;

	/* data can be null if there wasn't a window in the first place */
	if(!wf)
		return;

	if(result) {
		ay_image_window_close(wf->image_window_tag);
		_image_window_closed(wf->image_window_tag, wf);
	}
}

static void ext_yahoo_webcam_closed(int id, char *who, int reason)
{
	struct webcam_feed *wf = NULL;
	eb_local_account *ela = yahoo_find_local_account_by_id(id);
	eb_yahoo_local_account_data *yla = ela->protocol_local_account_data;

	char buff[1024];

	if(!reason)
		return;

	snprintf(buff, sizeof(buff), _("%s, webcam connection closed. %s "), ela->handle, who);
	switch(reason) {
		case 1: strncat(buff, _("stopped broadcasting."), sizeof(buff) - strlen(buff));
			break;
		case 2: strncat(buff, _("cancelled viewing permission."), sizeof(buff) - strlen(buff));
			break;
		case 3: strncat(buff, _("declined you permission."), sizeof(buff) - strlen(buff));
			break;
		case 4: strncat(buff, _("does not have their webcam online."), sizeof(buff) - strlen(buff));
			break;
		default:strncat(buff, _("did something we don't know about."), sizeof(buff) - strlen(buff));
			WARNING(("webcam close reason unknown %d", reason));
			break;
	}

	if((wf = find_webcam_feed(yla, who))) {
		strncat(buff, _("\nClose image window?"), sizeof(buff) - strlen(buff));
		eb_do_dialog(buff, _("Webcam connection closed"), eb_yahoo_close_webcam_window, wf);
	} else {
		ay_do_info(_("Webcam connection closed"), buff);
	}

}

/* added to allow compilation on Win32 : plasmoidia */
#ifdef __MINGW32__
static double get_time()
{
	struct _timeb ct;
	_ftime(&ct);

	/* return time in milliseconds */
	return (ct.time * 1E3 + ct.millitm);
}

#else
static double get_time()
{
	struct timeval ct;
	gettimeofday(&ct, 0);

	/* return time in milliseconds */
	return (ct.tv_sec * 1E3 + ct.tv_usec / 1E3);
}
#endif

static int ay_yahoo_webcam_timeout_callback(gpointer);

static void ay_yahoo_start_webcam(eb_local_account *ela)
{
	eb_yahoo_local_account_data *ylad = ela->protocol_local_account_data;
	struct webcam_feed *wf = NULL;

	if(ylad->webcam_timeout)
		return;

	yahoo_webcam_get_feed(ylad->id, NULL);
	ylad->webcam_start = get_time();

	if(!(wf = find_webcam_feed(ylad, NULL))) {
		wf = y_new0(struct webcam_feed, 1);
		wf->id = ylad->id;
		ylad->webcams = y_list_prepend(ylad->webcams, wf);
	}

	if(!wf->image_window_tag) {
		char buff[1024];
		snprintf(buff, sizeof(buff), _("My webcam (%d viewer%s)"), 
				ylad->viewers, ylad->viewers==1?"":"s");
		wf->image_window_tag = ay_image_window_new(320, 240, buff, _image_window_closed, wf);
	}

	if(ay_yahoo_webcam_timeout_callback(wf))
		ylad->webcam_timeout = eb_timeout_add(5000, (void *) ay_yahoo_webcam_timeout_callback, wf);
}

static void ay_yahoo_stop_webcam(eb_local_account *ela)
{
	eb_yahoo_local_account_data *ylad = ela->protocol_local_account_data;
	eb_timeout_remove(ylad->webcam_timeout);
	ylad->viewers=ylad->webcam_timeout = ylad->webcam_start = 0;
	yahoo_webcam_close_feed(ylad->id, NULL);
}

static void ay_yahoo_authorise_webcam(gpointer data, int result)
{
	struct webcam_feed *wf = data;
	eb_local_account *ela = yahoo_find_local_account_by_id(wf->id);
	eb_yahoo_local_account_data *ylad = ela->protocol_local_account_data;

	if(!ylad->webcam_start && result)
		ay_yahoo_start_webcam(ela);

	yahoo_webcam_accept_viewer(wf->id, wf->who, result);

	free(wf->who);
	free(wf);
}

static void ext_yahoo_webcam_viewer(int id, char *who, int connect)
{
	eb_local_account *ela;
	eb_yahoo_local_account_data *yla;
	char buff[1024];
	struct webcam_feed *wf;

	ela = yahoo_find_local_account_by_id(id);
	yla = ela->protocol_local_account_data;

	switch(connect) {
		case 0:
			snprintf(buff, sizeof(buff), 
					_("%s, the yahoo user %s has stopped viewing your webcam."), ela->handle, who);
			yla->viewers--;
			break;
		case 1:
			snprintf(buff, sizeof(buff), 
					_("%s, the yahoo user %s is viewing your webcam."), ela->handle, who);
			yla->viewers++;
			break;
		case 2:
			snprintf(buff, sizeof(buff), 
					_("%s, the yahoo user %s wants to view your webcam.\nDo you want to allow this?"), ela->handle, who);
			break;
		default:
			snprintf(buff, sizeof(buff), 
					_("%s, the yahoo user %s wants to %d your webcam.  Tell the ayttm devels about this."), ela->handle, who, connect);
			break;

	}
	if(yla->viewers <= 0)
		yla->viewers = 0;

	if(connect != 2) {
		ay_do_info(_("Yahoo Webcam"), buff);
		if((wf = find_webcam_feed(yla, NULL))) {
			snprintf(buff, sizeof(buff), _("My webcam (%d viewer%s)"), 
					yla->viewers, yla->viewers==1?"":"s");
			ay_image_window_update_title(wf->image_window_tag, buff);
		}
	} else {
		wf = y_new0(struct webcam_feed, 1);
		wf->id = id;
		wf->who = strdup(who);
		eb_do_dialog(buff, _("Yahoo Webcam Request"), ay_yahoo_authorise_webcam, wf);
	}
}

static int ay_yahoo_webcam_timeout_callback(gpointer data)
{
	struct webcam_feed *wf=data;
	int id = wf->id;
	eb_local_account *ela = yahoo_find_local_account_by_id(id);
	eb_yahoo_local_account_data *ylad = ela->protocol_local_account_data;
	unsigned char *image = NULL, *image2000=NULL;
	long length = 0;
	unsigned int timestamp;

	if(!video_grab_frame) {
		WARNING(("No frame grabber found"));
		ay_do_warning(_("Yahoo Webcam"), _("Could not read images from your webcam, could not find a video grabber.") );
		ay_yahoo_stop_webcam(ela);
		return 0;
	}

	timestamp = (unsigned int)(get_time() - ylad->webcam_start);
	if((length = video_grab_frame(&image)) <= 0) {
		WARNING(("Error reading from video grabber"));
		length=0;
	}

	if(wf && image) {
		ay_image_window_add_data(wf->image_window_tag, image, length, 1);
		ay_image_window_add_data(wf->image_window_tag, NULL, 0, 0);
	}

	if(ylad->viewers && image) {
		if(image_2_jpc)
			image2000 = image_2_jpc(image, &length);
		else {
			WARNING(("jpc encoder not available, using jpeg"));
			image2000 = ay_memdup(image, length);
		}
	} else {
		length=0;
	}

	if(image)
		ay_free(image);

	LOG(("Sending a webcam image (%d bytes)", length));

	yahoo_webcam_send_image(id, image2000, length, timestamp);
	if(image2000)
		ay_free(image2000);
	return 1;
}

static void ext_yahoo_webcam_data_request(int id, int send)
{
	if (send) {
		LOG(("Got request to start sending images"));
	} else {
		LOG(("Got request to stop sending images"));
	}
}

static void ext_yahoo_webcam_invite_reply(int id, char *from, int accept)
{
	eb_local_account *ela;

	if(from && !accept) {
		char buff[1024];
		ela = yahoo_find_local_account_by_id(id);
		snprintf(buff, sizeof(buff), _("%s, the yahoo user %s has declined your invitation to view your webcam."), ela->handle, from);
		ay_do_info(_("Webcam invite declined"), buff);
	}
}

static void ay_yahoo_invite_to_view_my_webcam(ebmCallbackData *data)
{
	ebmContactData *ecd = (ebmContactData *)data;
	eb_local_account *ela;
	eb_yahoo_local_account_data *ylad;
	char *who=ecd->remote_account;

	if(!who && ecd->contact && ecd->group) {
		grouplist *g = find_grouplist_by_name(ecd->group);
		struct contact *c = find_contact_in_group_by_nick(ecd->contact, g);
		eb_account *ea = find_account_for_protocol(c, SERVICE_INFO.protocol_id);
		who = ea->handle;
	}

	if (!who)
		return;

	ela = find_local_account_by_handle(ecd->local_account, SERVICE_INFO.protocol_id);
	if(!ela) {
		ay_do_warning(_("Yahoo Error"), _("Cannot find a valid local account to view user's webcam.") );
		return;
	}

	ylad = ela->protocol_local_account_data;

	if(!ylad->webcam_start)
		ay_yahoo_start_webcam(ela);
	yahoo_webcam_invite(ylad->id, who);
}

/*
 * Webcam code ends here
 ******************************/

static void eb_yahoo_authorize_callback(gpointer data, int result)
{
	struct yahoo_authorize_data *ay = data;
	eb_local_account *ela = yahoo_find_local_account_by_id(ay->id);

	if(result) {
		if(!find_account_with_ela(ay->who, ela)) {
			eb_account *ea = find_account_by_handle(ay->who, ela->service_id);
			struct contact *c = NULL;
			if(ea)
				c = ea->account_contact;
			ea = eb_yahoo_new_account(ela, ay->who);
			ea->account_contact = c;
			add_unknown_account_window_new(ea);
		}
	} else {
		yahoo_reject_buddy(ay->id, ay->who, "Thanks, but no thanks.");
	}

	if (ay) 
		FREE(ay->who);
	FREE(ay);
}
static void ext_yahoo_rejected(int id, char *who, char *msg)
{
	char buff[1024];
	snprintf(buff, sizeof(buff), _("%s has rejected your request to be added as a buddy%s%s"),
			who, (msg?_(" with the message:\n"):"."), (msg?msg:""));
	ay_do_warning( _("Yahoo Error"), buff );
}

static void ext_yahoo_contact_added(int id, char *myid, char *who, char *msg)
{
	char buff[1024];
	struct yahoo_authorize_data *ay = g_new0(struct yahoo_authorize_data, 1);
	eb_local_account * ela = yahoo_find_local_account_by_id(id);

	snprintf(buff, sizeof(buff), _("%s, the yahoo user %s has added you to their contact list"), (myid?myid:ela->handle), who);
	if(msg) {
		strcat(buff, _(" with the following message:\n"));
		strcat(buff, msg);
		strcat(buff, "\n");
	} else {
		strcat(buff, ".  ");
	}
	strcat(buff, _("Do you want to allow this?"));

	ay->id = id;
	ay->who = strdup(who);
	eb_do_dialog(buff, _("Yahoo New Contact"), eb_yahoo_authorize_callback, ay);
}

static int eb_yahoo_send_typing_stop(gpointer data)
{
	eb_ext_yahoo_typing_notify_data *tcd = data;
	eb_local_account * ela = yahoo_find_local_account_by_id(tcd->id);
	eb_yahoo_local_account_data * ylad = NULL;
	
	if (!ela) {
		ext_yahoo_log("ela == null !\n");
		return 0;
	}
	ylad = ela->protocol_local_account_data;

	ext_yahoo_log("Stop typing\n");

	yahoo_send_typing(tcd->id, ylad->act_id, tcd->ea->handle, 0);

	((eb_yahoo_account_data *)tcd->ea->protocol_account_data)->typing_timeout_tag = 0;

	FREE(tcd);

	/* return 0 to remove the timeout */
	return 0;
}

static int eb_yahoo_send_typing(eb_local_account *from, eb_account *to)
{
	eb_yahoo_local_account_data *ylad = from->protocol_local_account_data;
	eb_yahoo_account_data *yad = to->protocol_account_data;
	eb_ext_yahoo_typing_notify_data *tcd;
	

	/* if there's already a typing stop notify in the queue,
	 * cancel it and set a new one */
	if(yad->typing_timeout_tag)
		eb_timeout_remove(yad->typing_timeout_tag);

	if( !iGetLocalPref("do_send_typing_notify") )
		return 0;

	yahoo_send_typing(ylad->id, ylad->act_id, to->handle, 1);


	tcd = g_new0(eb_ext_yahoo_typing_notify_data, 1);
	tcd->id = ylad->id;
	tcd->ea = to;
	
	/* now, set a timeout to send typing stop notify */
	yad->typing_timeout_tag = eb_timeout_add(5*1000, 
			(void *) eb_yahoo_send_typing_stop, tcd);
	return 20;
}

static void ext_yahoo_typing_notify(int id, char *who, int stat)
{
	eb_local_account *ela = yahoo_find_local_account_by_id(id);
	
	eb_account *ea = find_account_with_ela(who, ela);

	if(ea) {
		if(stat && iGetLocalPref("do_typing_notify"))
			eb_update_status(ea, _("typing..."));
		else
			eb_update_status(ea, NULL);
	}
}

static void ext_yahoo_got_search_result(int id, 
		int found, int start, int total, YList *contacts)
{
}

static void ext_yahoo_game_notify(int id, char *who, int stat)
{
}

static void ext_yahoo_mail_notify(int id, char *from, char *subj, int cnt)
{
	eb_local_account * ela = yahoo_find_local_account_by_id(id);
	eb_yahoo_local_account_data *ylad = ela->protocol_local_account_data;
	char buff[1024] = {0}, buff2[200] = {0};

	snprintf(buff, sizeof(buff), "%s: ", ela->handle);
	
	if(!ylad->do_mail_notify)
		return;

	if(from && *from && subj && *subj) {
		snprintf(buff2, sizeof(buff2), 
				_("You have new mail from %s about %s\n"), 
				from, subj);
		strncat(buff, buff2, sizeof(buff)-strlen(buff));
	}
	if(cnt) {
		snprintf(buff2, sizeof(buff2), 
				_("You have %d message%s\n"), 
				cnt, cnt==1?"":_("s"));
		strncat(buff, buff2, sizeof(buff)-strlen(buff));
	}

	if(buff2[0])
		ay_do_info( _("Yahoo Mail"), buff );
}

static void ext_yahoo_system_message(int id, char *msg)
{
	eb_local_account * ela = yahoo_find_local_account_by_id(id);
	eb_yahoo_local_account_data *ylad = ela->protocol_local_account_data;

	if(ylad->ignore_system)
		return;

	ay_do_info( _("Yahoo System Message"), msg );
}

static void ext_yahoo_error(int id, char *err, int fatal)
{
	eb_local_account *ela = yahoo_find_local_account_by_id(id);

	ay_do_warning( _("Yahoo Error"), err );
	if(fatal) {
		eb_yahoo_logout(ela);
	}
}

static int eb_yahoo_query_connected(eb_account * ea)
{
	eb_yahoo_account_data *yad = ea->protocol_account_data;

	if (ref_count <= 0) {
		yad->status = YAHOO_STATUS_OFFLINE;
		yad->away = 1;
	}
	return yad->status != YAHOO_STATUS_OFFLINE;
}

struct connect_callback_data {
	eb_local_account *ela;
	yahoo_connect_callback callback;
	void *data;
	int tag;
};
static LList *conn = NULL;

static void ay_yahoo_cancel_connect(void *data)
{
	eb_local_account *ela = data;
	eb_yahoo_local_account_data *ylad = ela->protocol_local_account_data;
	if(ylad->connect_tag) {
		LList *l;
		ay_socket_cancel_async(ylad->connect_tag);
		if(ela->connecting) {
			for(l=conn; l; l=l_list_next(l)) {
				struct connect_callback_data *ccd = l->data;
				if(ccd->tag == ylad->connect_tag) {
					conn = l_list_remove_link(conn, l);
					ccd->callback(-1, 0, ccd->data);
					FREE(ccd);
					break;
				}
			}
			yahoo_close(ylad->id);
			ref_count--;
			ela->connecting = 0;
			ylad->connect_tag = 0;
			ylad->connect_progress_tag = 0;
		}
	}
	is_setting_state = 1;
	if (ela->status_menu) {
		eb_set_active_menu_status(ela->status_menu, EB_DISPLAY_YAHOO_OFFLINE);
	}
	is_setting_state = 0;
}

static void eb_yahoo_finish_login(const char *password, void *data)
{
	eb_local_account *ela = data;
	eb_yahoo_local_account_data *ylad = ela->protocol_local_account_data;
	char buff[1024];

	/* don't allow double connects */
	if(ela->connecting || ela->connected)
		return;

	ela->connecting = 1;

	ref_count++;
	ylad->id = yahoo_init_with_attributes(ela->handle, password,
			"pager_host", pager_host,
			"pager_port", atoi(pager_port),
			"filetransfer_host", filetransfer_host,
			"filetransfer_port", atoi(filetransfer_port),
			"webcam_host", webcam_host,
			"webcam_port", atoi(webcam_port),
			"webcam_description", webcam_description,
			"local_host", get_local_addresses(),
			"conn_type", conn_type,
			NULL);

	snprintf(buff, sizeof(buff), _("Logging in to Yahoo account: %s"), ela->handle);
	ylad->connect_progress_tag = ay_activity_bar_add(buff, ay_yahoo_cancel_connect, ela);

	LOG(("eb_yahoo_finish_login"));
	yahoo_set_log_level(do_yahoo_debug?YAHOO_LOG_DEBUG:YAHOO_LOG_NONE);

	ela->connected = 0;
	ylad->status = YAHOO_STATUS_OFFLINE;

	if(ylad->initial_state != -1)
		yahoo_login(ylad->id, ylad->initial_state);
	else if(ylad->login_invisible)
		yahoo_login(ylad->id, YAHOO_STATUS_INVISIBLE);
	else
		yahoo_login(ylad->id, YAHOO_STATUS_AVAILABLE);

	ylad->initial_state = -1;
}

static void eb_yahoo_login(eb_local_account * ela)
{
	eb_yahoo_local_account_data *ylad = ela->protocol_local_account_data;
	char buff[1024];

	/* don't allow double connects */
	if(ela->connecting || ela->connected)
		return;

	if(ylad->prompt_password || !ylad->password || !strlen(ylad->password)) {
		snprintf(buff, sizeof(buff), _("Yahoo! password for: %s"), ela->handle);
		do_password_input_window(buff, "", eb_yahoo_finish_login, ela);
	} else {
		eb_yahoo_finish_login(ylad->password, ela);
	}
}

static void ext_yahoo_login_response(int id, int succ, char *url)
{
	eb_local_account *ela = yahoo_find_local_account_by_id(id);
	eb_yahoo_local_account_data *ylad;
	char buff[1024];

	if(!ela)
		return;

	ylad = ela->protocol_local_account_data;

	if(succ == YAHOO_LOGIN_OK) {
		ela->connecting = 0;
		ela->connected = 1;
		ylad->status = yahoo_current_status(id);

		ay_activity_bar_update_label(ylad->connect_progress_tag, _("Fetching buddies..."));
		is_setting_state = 1;
		if(ela->status_menu)
			eb_set_active_menu_status(ela->status_menu, yahoo_to_eb_state_translation(ylad->status));
		is_setting_state = 0;

		ylad->timeout_tag = eb_timeout_add(600 * 1000, 
				(void *) eb_yahoo_ping_timeout_callback, ylad);

		return;
		
	} else if(succ == YAHOO_LOGIN_UNAME) {

		snprintf(buff, sizeof(buff), _("Could not log into Yahoo service - username not recognised.  Please verify that your username is correctly typed."));
	} else if(succ == YAHOO_LOGIN_PASSWD) {

		snprintf(buff, sizeof(buff), _("Could not log into Yahoo service - incorrect password.  Please verify that your password is correctly typed."));

	} else if(succ == YAHOO_LOGIN_LOCK) {
		
		snprintf(buff, sizeof(buff), _("Could not log into Yahoo service.  Your account has been locked.\nVisit %s to reactivate it."), url);

	} else if(succ == YAHOO_LOGIN_DUPL) {
		snprintf(buff, sizeof(buff), _("You have been logged out of the yahoo service, possibly due to a duplicate login."));
	} else if(succ == YAHOO_LOGIN_SOCK) {
		snprintf(buff, sizeof(buff), _("Could not connect to Yahoo server.  Please verify that you are connected to the net and the pager host and port are correctly entered."));
	} else {		
		snprintf(buff, sizeof(buff), _("Could not log into Yahoo service due to unknown state: %d\n"), succ);
	}	
	
	if(ylad->connect_progress_tag)
		ay_activity_bar_remove(ylad->connect_progress_tag);
	ylad->connect_progress_tag = 0;

	ay_do_warning( _("Yahoo Error"), buff );
	eb_yahoo_logout(ela);
	ela->connecting = 0;

}

static void eb_yahoo_logout(eb_local_account * ela)
{
	eb_yahoo_local_account_data *ylad;
	const YList * l = NULL;
	int i = 0;

	LOG(("eb_yahoo_logout"));

	ylad = ela->protocol_local_account_data;

	if (ylad == NULL || ylad->id <= 0) {
		LOG(("ylad NULL or invalid id"));
		return;
	}

	if(ylad->timeout_tag) {
		eb_timeout_remove(ylad->timeout_tag);
		ylad->timeout_tag=0;
	}
	if(ylad->webcam_timeout) {
		ay_yahoo_stop_webcam(ela);
	}
	
	if (!(ela->connected || ela->connecting)) {
		LOG(("eb_yahoo_logout called for already logged out account!"));
		return;
	}

	for(i = 0; i < 2; i++) {
		if(i == 0)
			l = yahoo_get_buddylist(ylad->id);
		else
			l = yahoo_get_ignorelist(ylad->id);
		
		for (; l; l=l->next) {
			struct yahoo_buddy *bud = l->data;
			eb_account *ea = find_account_with_ela(bud->id, ela);
			eb_yahoo_account_data *yad;
			if(!ea)
				continue;
			buddy_logoff(ea);
			buddy_update_status(ea);
			yad = ea->protocol_account_data;
			if(yad->typing_timeout_tag) {
				eb_timeout_remove(yad->typing_timeout_tag);
				/* FIXME */
				/* This will cause a memory leak because the
				 * timeout data has not been freed.  For now
				 * this avoids a segfault, which is a bigger
				 * problem from the user's pov.
				 */
				yad->typing_timeout_tag = 0;
			}
			
			yad->status = YAHOO_STATUS_OFFLINE;
			yad->away = 1;

		}
	}

	for(l = identities; l; l = l->next) {
		struct act_identity * i = l->data;
		if(i->id == ylad->id) {
			eb_remove_menu_item(EB_PROFILE_MENU, i->tag);
			identities = y_list_remove_link(identities, l);
			free(i->identity);
			free(i);
		}
	}

	yahoo_logoff(ylad->id);

	ylad->status = YAHOO_STATUS_OFFLINE;
	ylad->id = 0;
	ylad->act_id = NULL;
	free_yahoo_local_account(ylad, 0);

	ref_count--;
	ela->connected = 0;

	is_setting_state = 1;
	if (ela->status_menu) {
		eb_set_active_menu_status(ela->status_menu, EB_DISPLAY_YAHOO_OFFLINE);
	}
	is_setting_state = 0;
}

static void eb_yahoo_send_im(eb_local_account * account_from,
		      eb_account * account_to,
		      char * message)
{
	eb_yahoo_local_account_data *ylad = account_from->protocol_local_account_data;
	char * encoded = y_str_to_utf8(message);

	LOG(("eb_yahoo_send_im: %s => %s: %s", account_from->handle,
	     account_to->handle, message));
	yahoo_send_im(ylad->id, ylad->act_id, account_to->handle, encoded, 1);

	FREE(encoded);
}

static void yahoo_init_account_prefs(eb_local_account * ela)
{
	eb_yahoo_local_account_data *ylad = ela->protocol_local_account_data;
	input_list *il = g_new0(input_list, 1);

	ela->prefs = il;

	il->widget.entry.value = ela->handle;
	il->name = "SCREEN_NAME";
	il->label= _("_Yahoo Id:");
	il->type = EB_INPUT_ENTRY;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.entry.value = ylad->password;
	il->name = "PASSWORD";
	il->label= _("_Password:");
	il->type = EB_INPUT_PASSWORD;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.checkbox.value = &ylad->prompt_password;
	il->name = "prompt_password";
	il->label= _("_Ask for password at Login time");
	il->type = EB_INPUT_CHECKBOX;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.checkbox.value = &ela->connect_at_startup;
	il->name = "CONNECT";
	il->label= _("_Connect at startup");
	il->type = EB_INPUT_CHECKBOX;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.checkbox.value = &ylad->do_mail_notify;
	il->name = "do_mail_notify";
	il->label= _("Yahoo _Mail Notification");
	il->type = EB_INPUT_CHECKBOX;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.checkbox.value = &ylad->login_invisible;
	il->name = "login_invisible";
	il->label= _("Login _invisible");
	il->type = EB_INPUT_CHECKBOX;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.checkbox.value = &ylad->ignore_system;
	il->name = "ignore_system";
	il->label= _("Ignore _System Messages");
	il->type = EB_INPUT_CHECKBOX;
}

static eb_local_account *eb_yahoo_read_local_account_config(LList * pairs)
{
	eb_local_account *ela;
	eb_yahoo_local_account_data *ylad;

	if(!pairs) {
		WARNING(("eb_yahoo_read_local_account_config: pairs == NULL"));
		return NULL;
	}

	ela = g_new0(eb_local_account, 1);
	ylad = g_new0(eb_yahoo_local_account_data, 1);

	ela->service_id = SERVICE_INFO.protocol_id;
	ela->protocol_local_account_data = ylad;
	ylad->status = YAHOO_STATUS_OFFLINE;
	ylad->initial_state = -1;

	yahoo_init_account_prefs(ela);

	eb_update_from_value_pair(ela->prefs, pairs);

	strncpy(ela->alias, ela->handle, 255);

	return ela;
}

static LList *eb_yahoo_write_local_config(eb_local_account * ela)
{
	return eb_input_to_value_pair( ela->prefs );
}

static eb_account *eb_yahoo_read_account_config(eb_account *ea, LList * config)
{
	eb_yahoo_account_data *yad = g_new0(eb_yahoo_account_data, 1);

	yad->status = YAHOO_STATUS_OFFLINE;
	yad->away = 1;

	ea->protocol_account_data = yad;

	eb_yahoo_add_user(ea);

	return ea;
}

static LList *eb_yahoo_get_states()
{
	LList *states = NULL;

	states = l_list_append(states, "Available");
	states = l_list_append(states, "Be Right Back");
	states = l_list_append(states, "Busy");
	states = l_list_append(states, "Not at Home");
	states = l_list_append(states, "Not at my Desk");
	states = l_list_append(states, "Not in the Office");
	states = l_list_append(states, "On the Phone");
	states = l_list_append(states, "On Vacation");
	states = l_list_append(states, "Out to Lunch");
	states = l_list_append(states, "Stepped Out");
	states = l_list_append(states, "Invisible");
	states = l_list_append(states, "Idle");
	states = l_list_append(states, "Offline");
	states = l_list_append(states, "Custom");

	return states;
}

static char * eb_yahoo_check_login(char * user, char * pass)
{
	char *s = strchr(user, '@');
	if(s) {
		char ret[1024];
		snprintf(ret, sizeof(ret), _("Yahoo logins do NOT have the %s part."), s);
		return strdup(ret);
	}
	return NULL;
}

static int eb_yahoo_get_current_state(eb_local_account * ela)
{
	eb_yahoo_local_account_data *ylad;

	ylad = ela->protocol_local_account_data;
	if (eb_services[ela->service_id].protocol_id != SERVICE_INFO.protocol_id) {
		LOG(("eb_yahoo_get_current_state: protocol_id != SERVICE_INFO.protocol_id"));
	}
	return yahoo_to_eb_state_translation(ylad->status);
}

static void eb_yahoo_set_current_state(eb_local_account * ela, int state)
{
	eb_yahoo_local_account_data *ylad;
	int yahoo_state = eb_to_yahoo_state_translation[state];

	if (is_setting_state)
		return;

	LOG(("eb_yahoo_set_current_state to %d/%d", yahoo_state, state));
	if (ela == NULL) {
		WARNING(("ACCOUNT is NULL"));
		return;
	}
	if (ela->protocol_local_account_data == NULL) {
		WARNING(("Account Protocol Local Data is NULL"));
		return;
	}
	ylad = ela->protocol_local_account_data;
	if (eb_services[ela->service_id].protocol_id != SERVICE_INFO.protocol_id) {
		LOG(("eb_yahoo_get_current_state: protocol_id != SERVICE_INFO.protocol_id"));
	}
	LOG(("ylad->status = %d, state = %d, yahoo_state = %d", ylad->status, state, yahoo_state));
	LOG(("ela->connected = %d", ela->connected));


	/* Sanity check */
	if (ylad->status == YAHOO_STATUS_OFFLINE && ela->connected == 1) {
		LOG(("Sanity Check: ylad->status == offline but ela->connected == 1"));
	}
	if (ylad->status != YAHOO_STATUS_OFFLINE && ela->connected == 0) {
		LOG(("Sanity Check: ylad->status == online but ela->connected == 0"));
	}
	if (ylad->status == YAHOO_STATUS_OFFLINE && yahoo_state != YAHOO_STATUS_OFFLINE) {
		ylad->initial_state = yahoo_state;
		eb_yahoo_login(ela);
		return;
	} else if (ylad->status != YAHOO_STATUS_OFFLINE && yahoo_state == YAHOO_STATUS_OFFLINE) {
		eb_yahoo_logout(ela);
		return;
	}

	if(ylad->status == YAHOO_STATUS_AVAILABLE)
		ylad->away = 0;

	ylad->status = yahoo_state;
	if(yahoo_state == YAHOO_STATUS_CUSTOM) {
		if(ylad->status_message)
			yahoo_set_away(ylad->id, yahoo_state, ylad->status_message, ylad->away);
		else
			yahoo_set_away(ylad->id, yahoo_state, "delta p * delta x too large", 1);
	} else
		yahoo_set_away(ylad->id, yahoo_state, NULL, 1);
}

static void eb_yahoo_set_buddy_nick(eb_yahoo_local_account_data *ylad, 
		struct yahoo_buddy * bud, const char * nick)
{
	struct yab * yab;
	char * tmp;
	int i;

	if(!bud)
		return;

	yab = y_new0(struct yab, 1);

	/* if there's already an entry, don't destroy it */
	if(bud->yab_entry) {
		memcpy(yab, bud->yab_entry, sizeof(yab));
		yab->dbid = bud->yab_entry->dbid;
	}

	yab->id = bud->id;
/*	yab->nname = nick; */ /* too many hassles with nicks */
	yab->fname = strdup(nick);

	/* get rid of leading spaces */
	for(i=0; yab->fname[i] == ' '; i++)
		;
	if(i)
		memmove(yab->fname, yab->fname+i, strlen(yab->fname+i));

	for(i = strlen(yab->fname)-1; i>=0 && yab->fname[i]==' '; i--)
		yab->fname[i]='\0';

	tmp = strchr(yab->fname, ' ');
	if(tmp) {
		*tmp++='\0';

		yab->lname = tmp;
		tmp = strchr(yab->lname, ' ');
		if(tmp)
			yab->lname=tmp;
	}

	yahoo_set_yab(ylad->id, yab);

	free(yab->fname);
	free(yab);
}

static void eb_yahoo_change_user_name(eb_account * ea, const char * name)
{
	eb_local_account * ela;
	eb_yahoo_local_account_data *ylad;

	if(!(ela = eb_yahoo_find_active_local_account(ea)))
		return;

	ylad = ela->protocol_local_account_data;

	eb_yahoo_set_buddy_nick(ylad, 
			yahoo_find_buddy_by_handle(ylad->id, ea->handle), 
			name);
}

static void eb_yahoo_add_user(eb_account * ea)
{
	eb_local_account * ela;
	eb_yahoo_local_account_data *ylad;
	eb_yahoo_account_data * yad;

	const YList * buddies;
	int i=0;

/*	LOG(("eb_yahoo_add_user: %s", ea->handle)); */

	if (!ea) {
		WARNING(("Warning: eb_yahoo_add_user: ea == NULL\n"));
		return;
	}

	if(!(ela = eb_yahoo_find_active_local_account(ea)))
		return;

	ylad = ela->protocol_local_account_data;
	yad = ea->protocol_account_data;

	yad->status = YAHOO_STATUS_OFFLINE;
	yad->away = 1;

	/* find out if this is already a buddy */
	for(i = 0; i < 2; i++) {
		if(i == 0) buddies = yahoo_get_buddylist(ylad->id);
		else buddies = yahoo_get_ignorelist(ylad->id);

		for (; buddies; buddies = buddies->next) {
			struct yahoo_buddy *bud = buddies->data;

			LOG(("cache: looking at %s\n", bud->id));

			if (!strcasecmp(bud->id, ea->handle)) {
				LOG(("buddy %s exists, not adding", ea->handle));
				if(!i && !bud->real_name && ea->account_contact && ea->account_contact->nick) {
					eb_yahoo_set_buddy_nick(ylad, bud, ea->account_contact->nick);
				}
				return;
			}
		}
	}
	LOG(("Adding buddy %s to group %s", ea->handle,
			ea->account_contact->group->name));

	if(!strcmp(ea->account_contact->group->name, _("Ignore")))
		yahoo_ignore_buddy(ylad->id, ea->handle, FALSE);
	else {
		struct yahoo_buddy b = {NULL, ea->handle, NULL, NULL };
		yahoo_add_buddy(ylad->id, ea->handle, ea->account_contact->group->name);
		eb_yahoo_set_buddy_nick(ylad, &b, ea->account_contact->nick);
	}
	yahoo_refresh(ylad->id);
}

static void eb_yahoo_del_user(eb_account * ea)
{
	eb_local_account *ela;
	eb_yahoo_local_account_data *ylad;

	const YList * buddy=NULL;
	int i = 0;

	LOG(("eb_yahoo_del_user: %s", ea->handle));

	free_yahoo_account(ea->protocol_account_data);
	
	if(!(ela = eb_yahoo_find_active_local_account(ea)))
		return;

	ylad = ela->protocol_local_account_data;

	/* find out if this is already a buddy */
	for(i = 0; i < 2; i++) {
		if(i == 0) {
			LOG(("Searching buddylist"));
			buddy = yahoo_get_buddylist(ylad->id);
		} else {
			LOG(("Searching ignore list"));
			buddy = yahoo_get_ignorelist(ylad->id);
		}

		for (; buddy; buddy=buddy->next) {
			struct yahoo_buddy *bud = buddy->data;

			if (!strcmp(bud->id, ea->handle)) {
				if(!strcmp(ea->account_contact->group->name, _("Ignore")))
					yahoo_ignore_buddy(ylad->id, ea->handle, TRUE);
				else
					yahoo_remove_buddy(ylad->id, ea->handle, 
						(ea->account_contact == NULL ? "Default" :
						 ea->account_contact->group->name)); 
				return;
			}
		}
	}
}

static void eb_yahoo_ignore_user(eb_account * ea)
{
	eb_local_account *ela;
	eb_yahoo_local_account_data *ylad;
	const YList *buddy = NULL;
	struct yahoo_buddy *bud;

	LOG(("eb_yahoo_ignore_user: %s", ea->handle));

	if(!(ela = eb_yahoo_find_active_local_account(ea)))
		return;

	ylad = ela->protocol_local_account_data;
			
	/* find out if this is already ignored */
	for (buddy = yahoo_get_ignorelist(ylad->id); buddy; buddy=buddy->next) {
		struct yahoo_buddy *bud = buddy->data;

		if (!strcmp(bud->id, ea->handle))
			return;
	}

	/* if a buddy, remove.  buddy can be in more than one group */
	if((bud = yahoo_find_buddy_by_handle(ylad->id, ea->handle))) {
		yahoo_remove_buddy(ylad->id, ea->handle, 
			(ea->account_contact == NULL ? "Default" :
			 ea->account_contact->group->name)); 
	}

	/* now ignore him */
	yahoo_ignore_buddy(ylad->id, ea->handle, FALSE);
	yahoo_get_list(ylad->id);
}

static void eb_yahoo_unignore_user(eb_account * ea, const char *new_group)
{
	eb_local_account *ela;
	eb_yahoo_local_account_data *ylad;
	const YList *buddy = NULL;

	LOG(("eb_yahoo_unignore_user: %s", ea->handle));

	if(!(ela = eb_yahoo_find_active_local_account(ea)))
		return;

	ylad = ela->protocol_local_account_data;
			
	/* find out if this is already ignored */
	for (buddy = yahoo_get_ignorelist(ylad->id); buddy; buddy=buddy->next) {
		struct yahoo_buddy *bud = buddy->data;

		if(!strcmp(bud->id, ea->handle)) {
			yahoo_ignore_buddy(ylad->id, ea->handle, TRUE);

			/* add him only if he was moved to another group */
			if(new_group)
				yahoo_add_buddy(ylad->id, ea->handle, new_group);

			yahoo_get_list(ylad->id);
			return;
		}
	}
}

static void eb_yahoo_change_group(eb_account * ea, const char *new_group)
{
	eb_yahoo_local_account_data *ylad;

	if (!ea->ela->connected || ea->ela->service_id != SERVICE_INFO.protocol_id)
		return;

	ylad = ea->ela->protocol_local_account_data;

	yahoo_change_buddy_group(ylad->id, ea->handle,
			ea->account_contact->group->name, new_group);

	yahoo_refresh(ylad->id);
}

static void eb_yahoo_rename_group(eb_local_account *ela, const char *old_group, const char *new_group)
{
	eb_yahoo_local_account_data *ylad;

	if (!ela->connected || ela->service_id != SERVICE_INFO.protocol_id)
		return;

	ylad = ela->protocol_local_account_data;

	yahoo_group_rename(ylad->id, old_group, new_group);

	yahoo_refresh(ylad->id);
}

static eb_account *eb_yahoo_new_account(eb_local_account *ela, const char * account)
{
	eb_account *ea = g_new0(eb_account, 1);
	eb_yahoo_account_data *yad = g_new0(eb_yahoo_account_data, 1);

	LOG(("eb_yahoo_new_account"));

	ea->protocol_account_data = yad;
	strncpy(ea->handle, account, 255);
	ea->service_id = SERVICE_INFO.protocol_id;
	yad->status = YAHOO_STATUS_OFFLINE;
	yad->away = 1;
	ea->ela = ela;
	return ea;
}

static char **eb_yahoo_get_status_pixmap(eb_account * ea)
{
	eb_yahoo_account_data *yad;

	/*if (!pixmaps)
		eb_yahoo_init_pixmaps();*/

	yad = ea->protocol_account_data;

	if(yad->away < 0)
		WARNING(("%s->away is %d", ea->handle, yad->away));

	/* Don't translate this string */
	if (yad->status_message && !strcmp(yad->status_message, "I'm on SMS"))
		return yahoo_sms_xpm;
	else if (yad->away )
		return yahoo_away_xpm;
	else
		return yahoo_online_xpm;
}

static char *eb_yahoo_get_status_string(eb_account * ea)
{
	eb_yahoo_account_data *yad = ea->protocol_account_data;
	int i;

	if (yad->status == YAHOO_STATUS_CUSTOM && yad->status_message) {
//		LOG(("eb_yahoo_get_status_string: %s is %s", ea->handle, yad->status_message));
		return yad->status_message;
	}
	for (i = 0; eb_yahoo_status_codes[i].label; i++) {
		if (eb_yahoo_status_codes[i].id == yad->status) {
			if(yad->away > 100 && do_show_away_time) {
				static char buff[1024];
				snprintf(buff, sizeof(buff), _("%s for %d:%02d:%02d"), 
						eb_yahoo_status_codes[i].label,
						yad->away/3600, (yad->away/60)%60, yad->away%60);
				return buff;
			} else {
				return eb_yahoo_status_codes[i].label;
			}
		}
	}

	LOG(("eb_yahoo_get_status_string: %s is Unknown [%d]", ea->handle, yad->status));
	return "Unk";
}

static void eb_yahoo_set_idle(eb_local_account * ela, int idle)
{
	eb_yahoo_local_account_data *ylad;

	LOG(("eb_yahoo_set_idle: %d", idle));

	ylad = ela->protocol_local_account_data;
	if (idle == 0 && eb_yahoo_get_current_state(ela) == YAHOO_STATUS_IDLE) {
		if (ela->status_menu) {
			eb_set_active_menu_status(ela->status_menu, EB_DISPLAY_YAHOO_ONLINE);
		}
	} else if ((idle >= 600) && (eb_yahoo_get_current_state(ela) == YAHOO_STATUS_AVAILABLE)) {
		if (ela->status_menu) {
			eb_set_active_menu_status(ela->status_menu, EB_DISPLAY_YAHOO_IDLE);
		}
	}
}

static void eb_yahoo_set_away(eb_local_account * ela, char * message, int away)
{
	eb_yahoo_local_account_data *ylad;

	ylad = ela->protocol_local_account_data;
	if (!message) {
		if (ela->status_menu)
			eb_set_active_menu_status(ela->status_menu, EB_DISPLAY_YAHOO_ONLINE);
	} else {
		/* yeah, this was taken from Meredydd's MSN code */
		int state = EB_DISPLAY_YAHOO_CUSTOM;

		if(do_guess_away) {
			int pos = 0;
			char * lmsg = g_strdup(message);

			for(; lmsg[pos]; pos++)
				lmsg[pos] = tolower(lmsg[pos]);

			if(strstr(lmsg, "out") || strstr(lmsg, "away"))
				state=EB_DISPLAY_YAHOO_STEPPEDOUT;

			if(strstr(lmsg, "be right back") || strstr(lmsg, "brb"))
				state=EB_DISPLAY_YAHOO_BRB;

			if(strstr(lmsg, "busy") || strstr(lmsg, "working"))
				state=EB_DISPLAY_YAHOO_BUSY;

			if(strstr(lmsg, "phone"))
				state=EB_DISPLAY_YAHOO_ONPHONE;

			if(strstr(lmsg, "eating") || strstr(lmsg, "breakfast") || strstr(lmsg, "lunch") || strstr(lmsg, "dinner"))
				state=EB_DISPLAY_YAHOO_OUTTOLUNCH;

			if((strstr(lmsg, "not") || strstr(lmsg, "away")) && 
					strstr(lmsg, "desk"))
				state=EB_DISPLAY_YAHOO_NOTATDESK;

			if((strstr(lmsg, "not") || strstr(lmsg, "away") || strstr(lmsg, "out")) && 
					strstr(lmsg, "office"))
				state=EB_DISPLAY_YAHOO_NOTINOFFICE;

			if((strstr(lmsg, "not") || strstr(lmsg, "away") || strstr(lmsg, "out")) && 
					(strstr(lmsg, "home") || strstr(lmsg, "house")))
				state=EB_DISPLAY_YAHOO_NOTATHOME;

			FREE(lmsg);
			ylad->away = away;
		}

		if(state == EB_DISPLAY_YAHOO_CUSTOM) {
			LOG(("Custom away message"));
			FREE(ylad->status_message);
			ylad->status_message = strdup(message);
			ylad->away = away;
		} 
		
		if(state == EB_DISPLAY_YAHOO_CUSTOM && ylad->status == YAHOO_STATUS_CUSTOM)
			yahoo_set_away(ylad->id, YAHOO_STATUS_CUSTOM, ylad->status_message, ylad->away);
		else if (ela->status_menu)
			eb_set_active_menu_status(ela->status_menu, state);
	}
}

static void eb_yahoo_get_info(eb_local_account * reciever, eb_account * sender)
{
	char buff[1024];

	snprintf(buff, sizeof(buff), "%s%s", yahoo_get_profile_url(),
			sender->handle);
	open_url(NULL, buff);
}

static void eb_yahoo_read_prefs_config(LList * values)
{
}

static LList *eb_yahoo_write_prefs_config()
{
	return NULL;
}

static input_list * eb_yahoo_get_prefs()
{
	return NULL;
}

static int eb_yahoo_is_suitable(eb_local_account *ela, eb_account *ea)
{
	/* in yahoo you can message anyone */
	return TRUE;

	/*
	eb_yahoo_local_account_data *ylad;
	const YList * buddy;

	ylad = ela->protocol_local_account_data;
	
	for(buddy = yahoo_get_buddylist(ylad->id); buddy; buddy=buddy->next) {
		struct yahoo_buddy *bud = buddy->data;
		if(!strcmp(ea->handle, bud->id))
			return TRUE;
	}
	
	return FALSE;
	*/
}

static void _yahoo_connected(int fd, int error, void * data)
{
	struct connect_callback_data *ccd = data;
	eb_local_account * ela = ccd->ela;
	eb_yahoo_local_account_data *ylad = ela->protocol_local_account_data;

	conn = l_list_remove(conn, ccd);

	ccd->callback(fd, error, ccd->data);
	FREE(ccd);
	ylad->connect_tag = 0;
}

static void ay_yahoo_connect_status(const char *msg, void *data)
{
	struct connect_callback_data *ccd = data;
	eb_local_account *ela = ccd->ela;
	eb_yahoo_local_account_data *ylad = ela->protocol_local_account_data;
	ay_activity_bar_update_label(ylad->connect_progress_tag, msg);
}

static int ext_yahoo_connect_async(int id, char *host, int port, yahoo_connect_callback callback, void *data)
{
#ifdef __MINGW32__
	int fd = ay_socket_new(host,port);
	if(fd == -1)
		callback(fd, WSAGetLastError(), data);
	else
		callback(fd, 0, data);
#else
	struct connect_callback_data *ccd = y_new0(struct connect_callback_data, 1);
	eb_yahoo_local_account_data *ylad;

	ccd->ela = yahoo_find_local_account_by_id(id);
	if(!ccd->ela) {
		free(ccd);
		return 0;
	}
	ccd->callback = callback;
	ccd->data = data;

	ylad = ccd->ela->protocol_local_account_data;
	ccd->tag = ylad->connect_tag = 
		proxy_connect_host(host, port, _yahoo_connected, ccd, ay_yahoo_connect_status);

	conn = l_list_prepend(conn, ccd);

	if(ylad->connect_tag < 0)
		_yahoo_connected(-1, errno, ccd);

	return ylad->connect_tag;
#endif
}

static int ext_yahoo_connect(char *host, int port)
{
	return ay_socket_new(host, port);
}

/*************************************
 * Callback handling code starts here
 */
typedef struct {
	int id;
	int fd;
	gpointer data;
	int tag;
} eb_yahoo_callback_data;

static void eb_yahoo_callback(void * data, int source, eb_input_condition condition)
{
	eb_yahoo_callback_data *d = data;
	int ret=1;
	char buff[1024]={0};

	if(condition & EB_INPUT_READ) {
		LOG(("Read: %d", source));
		ret = yahoo_read_ready(d->id, source, d->data);

		if(ret == -1)
			snprintf(buff, sizeof(buff), 
				_("Yahoo read error (%d): %s"), errno, 
				strerror(errno));
		else if(ret == 0)
			snprintf(buff, sizeof(buff), 
				_("Yahoo read error: Server closed socket"));
	}

	if(ret > 0 && condition & EB_INPUT_WRITE) {
		LOG(("Write: %d", source));
		ret = yahoo_write_ready(d->id, source, d->data);

		if(ret == -1)
			snprintf(buff, sizeof(buff), 
				_("Yahoo write error (%d): %s"), errno, 
				strerror(errno));
		else if(ret == 0)
			snprintf(buff, sizeof(buff), 
				_("Yahoo write error: Server closed socket"));
	}

	if(condition & EB_INPUT_EXCEPTION)
		LOG(("Exception: %d", source));
	if(!(condition & (EB_INPUT_READ | EB_INPUT_WRITE | EB_INPUT_EXCEPTION)))
		LOG(("Unknown: %d", condition));

	if(buff[0])
		ay_do_warning( _("Yahoo Error"), buff );
}

static YList * handlers = NULL;

static int ext_yahoo_add_handler(int id, int fd, yahoo_input_condition cond, void *data)
{
	eb_yahoo_callback_data *d = g_new0(eb_yahoo_callback_data, 1);
	d->id = id;
	d->fd = fd;
	d->data = data;
	d->tag = eb_input_add(fd, cond, eb_yahoo_callback, d);
	LOG(("client:%d added fd:%d for cond:%d; tag:%d", id, fd, cond, d->tag));

	handlers = y_list_append(handlers, d);
	return d->tag;
}

static void ext_yahoo_remove_handler(int id, int tag)
{
	YList * l;
	for(l = handlers; l; l = l->next)
	{
		eb_yahoo_callback_data *d = l->data;
		if(d->tag == tag) {
			LOG(("client:%d removed fd:%d with tag:%d", d->id, d->fd, d->tag));
			eb_input_remove(d->tag);
			handlers = y_list_remove_link(handlers, l);
			FREE(d);
			y_list_free_1(l);
			break;
		}
	}
}
/*
 * Callback handling code ends here
 ***********************************/

static char *eb_yahoo_get_color(void) { return "#d08020"; }

static LList *psmileys = NULL;

static void yahoo_init_smileys()
{
	psmileys=add_protocol_smiley(psmileys, ":(|)", "monkey");

	psmileys=add_protocol_smiley(psmileys, "<):)", "cowboy");

	psmileys=add_protocol_smiley(psmileys, "/:)", "eyebrow");
	psmileys=add_protocol_smiley(psmileys, "/:-)", "eyebrow");

	psmileys=add_protocol_smiley(psmileys, ">:)", "devil");
	psmileys=add_protocol_smiley(psmileys, ">:-)", "devil");

	psmileys=add_protocol_smiley(psmileys, "=:-)", "bug");
	psmileys=add_protocol_smiley(psmileys, "=:)", "bug");

	psmileys=add_protocol_smiley(psmileys, ">-)", "alien");

	psmileys=add_protocol_smiley(psmileys, "8-}", "silly");

	psmileys=add_protocol_smiley(psmileys, "=D>", "applause");
	psmileys=add_protocol_smiley(psmileys, "=d>", "applause");

	psmileys=add_protocol_smiley(psmileys, "#-o", "doh");
	psmileys=add_protocol_smiley(psmileys, "#-O", "doh");

	psmileys=add_protocol_smiley(psmileys, ":-?", "thinking");

	psmileys=add_protocol_smiley(psmileys, "=P~", "drooling");

	psmileys=add_protocol_smiley(psmileys, "(:|", "yawn");

	psmileys=add_protocol_smiley(psmileys, "o:)", "angel");
	psmileys=add_protocol_smiley(psmileys, "O:)", "angel");
	psmileys=add_protocol_smiley(psmileys, "0:)", "angel");
	psmileys=add_protocol_smiley(psmileys, "o:-)", "angel");
	psmileys=add_protocol_smiley(psmileys, "O:-)", "angel");
	psmileys=add_protocol_smiley(psmileys, "0:-)", "angel");

	psmileys=add_protocol_smiley(psmileys, ":((", "cry");
	psmileys=add_protocol_smiley(psmileys, ":-((", "cry");

	psmileys=add_protocol_smiley(psmileys, ":))", "biglaugh");
	psmileys=add_protocol_smiley(psmileys, ":-))", "biglaugh");

	psmileys=add_protocol_smiley(psmileys, ":*", "kiss");
	psmileys=add_protocol_smiley(psmileys, ":-*", "kiss");

	psmileys=add_protocol_smiley(psmileys, ":o)", "clown");
	psmileys=add_protocol_smiley(psmileys, ":O)", "clown");
	psmileys=add_protocol_smiley(psmileys, ":0)", "clown");
	psmileys=add_protocol_smiley(psmileys, "<@:)", "clown");

	psmileys=add_protocol_smiley(psmileys, "*-:)", "lightbulb");

	psmileys=add_protocol_smiley(psmileys, ":(", "sad");	
	psmileys=add_protocol_smiley(psmileys, ":-(", "sad");

	psmileys=add_protocol_smiley(psmileys, ";))", "heehee");	

	psmileys=add_protocol_smiley(psmileys, ";;)", "eyelash");	
	psmileys=add_protocol_smiley(psmileys, ";;-)", "eyelash");

	psmileys=add_protocol_smiley(psmileys, ";)", "wink");	
	psmileys=add_protocol_smiley(psmileys, ";-)", "wink");

	psmileys=add_protocol_smiley(psmileys, "\\:d/", "dancing");
	psmileys=add_protocol_smiley(psmileys, "\\:D/", "dancing");

	psmileys=add_protocol_smiley(psmileys, ":D", "grin");	
	psmileys=add_protocol_smiley(psmileys, ":-D", "grin");
	psmileys=add_protocol_smiley(psmileys, ":d", "grin");
	psmileys=add_protocol_smiley(psmileys, ":-d", "grin");

	psmileys=add_protocol_smiley(psmileys, ":P", "tongue");	
	psmileys=add_protocol_smiley(psmileys, ":-P", "tongue");
	psmileys=add_protocol_smiley(psmileys, ":p", "tongue");
	psmileys=add_protocol_smiley(psmileys, ":-p", "tongue");

	psmileys=add_protocol_smiley(psmileys, ":B", "glasses");
	psmileys=add_protocol_smiley(psmileys, ":-B", "glasses");
	psmileys=add_protocol_smiley(psmileys, ":b", "glasses");
	psmileys=add_protocol_smiley(psmileys, ":-b", "glasses");

	psmileys=add_protocol_smiley(psmileys, "#:S", "whew");
	psmileys=add_protocol_smiley(psmileys, "#:-S", "whew");
	psmileys=add_protocol_smiley(psmileys, "#:s", "whew");
	psmileys=add_protocol_smiley(psmileys, "#:-s", "whew");

	psmileys=add_protocol_smiley(psmileys, ":SS", "nail_biting");
	psmileys=add_protocol_smiley(psmileys, ":-SS", "nail_biting");
	psmileys=add_protocol_smiley(psmileys, ":ss", "nail_biting");
	psmileys=add_protocol_smiley(psmileys, ":-ss", "nail_biting");

	psmileys=add_protocol_smiley(psmileys, ":S", "worried");
	psmileys=add_protocol_smiley(psmileys, ":-S", "worried");
	psmileys=add_protocol_smiley(psmileys, ":s", "worried");
	psmileys=add_protocol_smiley(psmileys, ":-s", "worried");

	psmileys=add_protocol_smiley(psmileys, ">:/", "bring_it");

	psmileys=add_protocol_smiley(psmileys, ":/", "confused");
	psmileys=add_protocol_smiley(psmileys, ":-/", "confused");

	psmileys=add_protocol_smiley(psmileys, ":|", "blank");	
	psmileys=add_protocol_smiley(psmileys, ":-|", "blank");

	psmileys=add_protocol_smiley(psmileys, ":x", "heart");
	psmileys=add_protocol_smiley(psmileys, ":X", "heart");
	psmileys=add_protocol_smiley(psmileys, ":-x", "heart");
	psmileys=add_protocol_smiley(psmileys, ":-X", "heart");

	psmileys=add_protocol_smiley(psmileys, "x(", "angry");
	psmileys=add_protocol_smiley(psmileys, "X(", "angry");
	psmileys=add_protocol_smiley(psmileys, "x-(", "angry");
	psmileys=add_protocol_smiley(psmileys, "X-(", "angry");

	psmileys=add_protocol_smiley(psmileys, "3:O", "cow");
	psmileys=add_protocol_smiley(psmileys, "3:o", "cow");
	psmileys=add_protocol_smiley(psmileys, "3:-O", "cow");
	psmileys=add_protocol_smiley(psmileys, "3:-o", "cow");

	psmileys=add_protocol_smiley(psmileys, ":o", "oh");	
	psmileys=add_protocol_smiley(psmileys, ":O", "oh");
	psmileys=add_protocol_smiley(psmileys, ":-o", "oh");
	psmileys=add_protocol_smiley(psmileys, ":-O", "oh");

	psmileys=add_protocol_smiley(psmileys, "~:>", "chicken");	
	psmileys=add_protocol_smiley(psmileys, "~o)", "coffee");	

	psmileys=add_protocol_smiley(psmileys, ":>", "heyyy");	
	psmileys=add_protocol_smiley(psmileys, ":->", "heyyy");

	psmileys=add_protocol_smiley(psmileys, "B)", "cooldude");
	psmileys=add_protocol_smiley(psmileys, "B-)", "cooldude");
	psmileys=add_protocol_smiley(psmileys, "b)", "cooldude");
	psmileys=add_protocol_smiley(psmileys, "b-)", "cooldude");

	psmileys=add_protocol_smiley(psmileys, ":\">", "blush");

	psmileys=add_protocol_smiley(psmileys, "=;", "stop");

	psmileys=add_protocol_smiley(psmileys, ":$", "shh");
	psmileys=add_protocol_smiley(psmileys, ":-$", "shh");

	psmileys=add_protocol_smiley(psmileys, "i-)", "zzz");
	psmileys=add_protocol_smiley(psmileys, "i)", "zzz");
	psmileys=add_protocol_smiley(psmileys, "I-)", "zzz");
	psmileys=add_protocol_smiley(psmileys, "I)", "zzz");
	psmileys=add_protocol_smiley(psmileys, "|-)", "zzz");
	psmileys=add_protocol_smiley(psmileys, "|)", "zzz");

	psmileys=add_protocol_smiley(psmileys, ":-&", "sick");
	psmileys=add_protocol_smiley(psmileys, ":&", "sick");

	psmileys=add_protocol_smiley(psmileys, "[-(", "flying");

	psmileys=add_protocol_smiley(psmileys, "[-x", "shame");

	psmileys=add_protocol_smiley(psmileys, ":@)", "pig");

	psmileys=add_protocol_smiley(psmileys, "8-|", "rolling_eyes");
	psmileys=add_protocol_smiley(psmileys, "8-|", "rolling_eyes");

	psmileys=add_protocol_smiley(psmileys, "8-X", "skull");
	psmileys=add_protocol_smiley(psmileys, "8-x", "skull");

	psmileys=add_protocol_smiley(psmileys, "(~~)", "pumkin");

	psmileys=add_protocol_smiley(psmileys, "**==", "usflag");

	psmileys=add_protocol_smiley(psmileys, "%%-", "luck");
	psmileys=add_protocol_smiley(psmileys, "@};-", "rose");

	psmileys=add_protocol_smiley(psmileys, ">:D<", "hugs");

	psmileys=add_protocol_smiley(psmileys, "=((", "broken_heart");

	psmileys=add_protocol_smiley(psmileys, "l-)", "loser");
	psmileys=add_protocol_smiley(psmileys, "L-)", "loser");

	psmileys=add_protocol_smiley(psmileys, "<:-p", "party");
	psmileys=add_protocol_smiley(psmileys, "<:-P", "party");

	psmileys=add_protocol_smiley(psmileys, "@-)", "hypnotised");

	psmileys=add_protocol_smiley(psmileys, ":^o", "liar");
	psmileys=add_protocol_smiley(psmileys, ":^O", "liar");

	psmileys=add_protocol_smiley(psmileys, ":w", "waiting");
	psmileys=add_protocol_smiley(psmileys, ":W", "waiting");
	psmileys=add_protocol_smiley(psmileys, ":-w", "waiting");
	psmileys=add_protocol_smiley(psmileys, ":-W", "waiting");

	psmileys=add_protocol_smiley(psmileys, ":-<", "sigh");
	psmileys=add_protocol_smiley(psmileys, ":<", "sigh");

	psmileys=add_protocol_smiley(psmileys, ">:p", "phbbbt");
	psmileys=add_protocol_smiley(psmileys, ">:P", "phbbbt");

	psmileys=add_protocol_smiley(psmileys, "=))", "rotfl");

	psmileys=add_protocol_smiley(psmileys, ":-\"", "whistle");

	psmileys=add_protocol_smiley(psmileys, "b-(", "black_eye");

	psmileys=add_protocol_smiley(psmileys, ":)>-", "peace");

	psmileys=add_protocol_smiley(psmileys, ":-\"", "whistling");

	psmileys=add_protocol_smiley(psmileys, "(%)", "yinyang");

	psmileys=add_protocol_smiley(psmileys, "(*)", "star");

	psmileys=add_protocol_smiley(psmileys, ":-@", "sore_ears");

	psmileys=add_protocol_smiley(psmileys, "^:)^", "not_worthy");

	psmileys=add_protocol_smiley(psmileys, ":-j", "just_kidding");

	psmileys=add_protocol_smiley(psmileys, ":)", "smile");	
	psmileys=add_protocol_smiley(psmileys, ":-)", "smile");

	psmileys=add_protocol_smiley(psmileys, ":-l", "frustrated");
	psmileys=add_protocol_smiley(psmileys, "[-o<", "praying");
	psmileys=add_protocol_smiley(psmileys, "$-)", "money_eyes");

	psmileys=add_protocol_smiley(psmileys, "o->", "hiro");
	psmileys=add_protocol_smiley(psmileys, "o=>", "billy");
	psmileys=add_protocol_smiley(psmileys, "o-+", "april");

}

static LList *eb_yahoo_get_smileys(void)
{
	if(!psmileys)
		yahoo_init_smileys();

	return psmileys;
}

struct service_callbacks *query_callbacks()
{
	struct service_callbacks *sc;

	LOG(("yahoo_query_callbacks"));
	sc = y_new0(struct service_callbacks, 1);

	sc->query_connected 		= eb_yahoo_query_connected;
	sc->login			= eb_yahoo_login;
	sc->logout 			= eb_yahoo_logout;

	sc->send_im 			= eb_yahoo_send_im;
	sc->send_typing			= eb_yahoo_send_typing;
	sc->send_cr_typing		= NULL;

	sc->read_local_account_config 	= eb_yahoo_read_local_account_config;
	sc->write_local_config 		= eb_yahoo_write_local_config;
	sc->read_account_config 	= eb_yahoo_read_account_config;

	sc->get_states 			= eb_yahoo_get_states;
	sc->get_current_state 		= eb_yahoo_get_current_state;
	sc->set_current_state 		= eb_yahoo_set_current_state;
	sc->check_login			= eb_yahoo_check_login;
	
	sc->add_user 			= eb_yahoo_add_user;
	sc->del_user 			= eb_yahoo_del_user;
	sc->ignore_user			= eb_yahoo_ignore_user;
	sc->unignore_user		= eb_yahoo_unignore_user;
	sc->change_user_name		= eb_yahoo_change_user_name;
	sc->change_group		= eb_yahoo_change_group;
	sc->rename_group		= eb_yahoo_rename_group;
	sc->del_group			= NULL;
	sc->free_account_data		= eb_yahoo_free_account_data;

	sc->is_suitable			= eb_yahoo_is_suitable;
	sc->new_account 		= eb_yahoo_new_account;

	sc->get_status_string 		= eb_yahoo_get_status_string;
	sc->get_status_pixmap 		= eb_yahoo_get_status_pixmap;

	sc->set_idle 			= eb_yahoo_set_idle;
	sc->set_away 			= eb_yahoo_set_away;

	sc->send_chat_room_message 	= eb_yahoo_send_chat_room_message;

	sc->join_chat_room 		= eb_yahoo_join_chat_room;
	sc->leave_chat_room 		= eb_yahoo_leave_chat_room;

	sc->make_chat_room 		= eb_yahoo_make_chat_room;

	sc->send_invite	 		= eb_yahoo_send_invite;
	sc->accept_invite 		= eb_yahoo_accept_invite;
	sc->decline_invite 		= eb_yahoo_decline_invite;

	sc->send_file			= eb_yahoo_send_file;

	sc->terminate_chat		= NULL;

	sc->get_info 			= eb_yahoo_get_info;
	sc->get_prefs 			= eb_yahoo_get_prefs;
	sc->read_prefs_config 		= eb_yahoo_read_prefs_config;
	sc->write_prefs_config 		= eb_yahoo_write_prefs_config;

	sc->add_importers 		= NULL;

	sc->get_color 			= eb_yahoo_get_color;
	sc->get_smileys 		= eb_yahoo_get_smileys;

	return sc;
}

static void register_menuentries()
{
	mywebcam_chat_menu_tag = eb_add_menu_item(_("Invite to view Webcam"), 
			EB_CHAT_WINDOW_MENU, 
			ay_yahoo_invite_to_view_my_webcam, 
			ebmCONTACTDATA, 
			NULL);
	mywebcam_contact_menu_tag = eb_add_menu_item(_("Invite to view Webcam"), 
			EB_CONTACT_MENU, 
			ay_yahoo_invite_to_view_my_webcam, 
			ebmCONTACTDATA, 
			NULL);

	eb_menu_item_set_protocol(mywebcam_chat_menu_tag, "Yahoo");
	eb_menu_item_set_protocol(mywebcam_contact_menu_tag, "Yahoo");

	webcam_chat_menu_tag = eb_add_menu_item(_("View Webcam"), 
			EB_CHAT_WINDOW_MENU, 
			ay_yahoo_view_users_webcam, 
			ebmCONTACTDATA, 
			NULL);
	webcam_contact_menu_tag = eb_add_menu_item(_("View Webcam"), 
			EB_CONTACT_MENU, 
			ay_yahoo_view_users_webcam, 
			ebmCONTACTDATA, 
			NULL);

	eb_menu_item_set_protocol(webcam_chat_menu_tag, "Yahoo");
	eb_menu_item_set_protocol(webcam_contact_menu_tag, "Yahoo");
}

static void unregister_menuentries()
{
	if(mywebcam_chat_menu_tag)
		eb_remove_menu_item(EB_CHAT_WINDOW_MENU, mywebcam_chat_menu_tag);
	if(mywebcam_contact_menu_tag)
		eb_remove_menu_item(EB_CONTACT_MENU, mywebcam_contact_menu_tag);

	if(webcam_chat_menu_tag)
		eb_remove_menu_item(EB_CHAT_WINDOW_MENU, webcam_chat_menu_tag);
	if(webcam_contact_menu_tag)
		eb_remove_menu_item(EB_CONTACT_MENU, webcam_contact_menu_tag);

	mywebcam_chat_menu_tag = mywebcam_contact_menu_tag = 0;
	webcam_chat_menu_tag = webcam_contact_menu_tag = 0;
}

static void register_callbacks()
{
#ifdef USE_STRUCT_CALLBACKS
	static struct yahoo_callbacks yc;

	yc.ext_yahoo_login_response = ext_yahoo_login_response;
	yc.ext_yahoo_got_buddies = ext_yahoo_got_buddies;
	yc.ext_yahoo_got_ignore = ext_yahoo_got_ignore;
	yc.ext_yahoo_got_identities = ext_yahoo_got_identities;
	yc.ext_yahoo_got_cookies = ext_yahoo_got_cookies;
	yc.ext_yahoo_status_changed = ext_yahoo_status_changed;
	yc.ext_yahoo_got_im = ext_yahoo_got_im;
	yc.ext_yahoo_got_conf_invite = ext_yahoo_got_conf_invite;
	yc.ext_yahoo_conf_userdecline = ext_yahoo_conf_userdecline;
	yc.ext_yahoo_conf_userjoin = ext_yahoo_conf_userjoin;
	yc.ext_yahoo_conf_userleave = ext_yahoo_conf_userleave;
	yc.ext_yahoo_conf_message = ext_yahoo_conf_message;
	yc.ext_yahoo_got_file = ext_yahoo_got_file;
	yc.ext_yahoo_contact_added = ext_yahoo_contact_added;
	yc.ext_yahoo_rejected = ext_yahoo_rejected;
	yc.ext_yahoo_typing_notify = ext_yahoo_typing_notify;
	yc.ext_yahoo_game_notify = ext_yahoo_game_notify;
	yc.ext_yahoo_mail_notify = ext_yahoo_mail_notify;
	yc.ext_yahoo_system_message = ext_yahoo_system_message;
	yc.ext_yahoo_error = ext_yahoo_error;
	yc.ext_yahoo_log = ext_yahoo_log;
	yc.ext_yahoo_add_handler = ext_yahoo_add_handler;
	yc.ext_yahoo_remove_handler = ext_yahoo_remove_handler;
	yc.ext_yahoo_connect_async = ext_yahoo_connect_async;
	yc.ext_yahoo_connect = ext_yahoo_connect;

	yc.ext_yahoo_chat_cat_xml = ext_yahoo_chat_cat_xml;
	yc.ext_yahoo_chat_join = ext_yahoo_chat_join;
	yc.ext_yahoo_chat_userjoin = ext_yahoo_chat_userjoin;
	yc.ext_yahoo_chat_userleave = ext_yahoo_chat_userleave;
	yc.ext_yahoo_chat_message = ext_yahoo_chat_message;

	yc.ext_yahoo_got_search_result = ext_yahoo_got_search_result;

	yc.ext_yahoo_got_webcam_image = ext_yahoo_got_webcam_image;
	yc.ext_yahoo_webcam_invite = ext_yahoo_webcam_invite;
	yc.ext_yahoo_webcam_invite_reply = ext_yahoo_webcam_invite_reply;
	yc.ext_yahoo_webcam_data_request = ext_yahoo_webcam_data_request;
	yc.ext_yahoo_webcam_viewer = ext_yahoo_webcam_viewer;
	yc.ext_yahoo_webcam_closed = ext_yahoo_webcam_closed;

	yahoo_register_callbacks(&yc);
	
#endif
}

