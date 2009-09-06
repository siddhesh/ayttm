/*
 * Ayttm
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

/*
 * msn.c
 */

#ifdef __MINGW32__
#define __IN_PLUGIN__
#endif

#include "intl.h"

#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>

#ifdef __MINGW32__
#include <winsock2.h>
#else
#include <sys/poll.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

#include "info_window.h"
#include "value_pair.h"
#include "plugin_api.h"
#include "activity_bar.h"
#include "util.h"
#include "status.h"
#include "service.h"
#include "pixmaps/msn_online.xpm"
#include "pixmaps/msn_away.xpm"
#include "message_parse.h"
#include "browser.h"
#include "smileys.h"
#include "file_select.h"
#include "add_contact_window.h"
#include "offline_queue_mgmt.h"
#include "messages.h"
#include "dialog.h"
#include "platform_defs.h"

#include "libproxy/networking.h"

#include "libmsn2/msn.h"
#include "libmsn2/msn-connection.h"
#include "libmsn2/msn-account.h"
#include "libmsn2/msn-ext.h"
#include "libmsn2/msn-sb.h"

/*************************************************************************************
 *			     Begin Module Code
 ************************************************************************************/
/*  Module defines */
#define plugin_info msn2_LTX_plugin_info
#define SERVICE_INFO msn2_LTX_SERVICE_INFO
#define plugin_init msn2_LTX_plugin_init
#define plugin_finish msn2_LTX_plugin_finish
#define module_version msn2_LTX_module_version


typedef struct _ay_msn_local_account
{
	MsnAccount* ma;
	int connect_tag;
	int activity_tag;
	int waiting_ans;
	int listsyncing;
	int do_mail_notify;
	int do_mail_notify_folders;
	int do_mail_notify_run_script;
	char do_mail_notify_script_name[MAX_PREF_LEN];
	int login_invisible;
	int prompt_password;
	int initial_state;

	LList *chatrooms;
	LList *transfer_windows;
	LList *waiting_auth_callbacks;

	char password[MAX_PREF_LEN];
	char friendlyname[MAX_PREF_LEN];
} ay_msn_local_account;


static eb_account *ay_msn_new_account(eb_local_account *ela, const char * account );
static eb_account *ay_msn_new_account_for_buddy(eb_local_account *ela, MsnBuddy *buddy);

static LList * psmileys=NULL;

/* Function Prototypes */
unsigned int module_version() {return CORE_VERSION;}
static int plugin_init();
static int plugin_finish();
struct service_callbacks * query_callbacks();

static void msn_new_mail_run_script(eb_local_account *ela);
static void ay_msn_format_message (MsnIM *msg);
static char *ay_msn_get_color(void) { static char color[]="#aa0000"; return color; }
static void ay_msn_change_group(eb_account * ea, const char *new_group);
static void invite_gnomemeeting(ebmCallbackData * data);

static LList *ay_msn_get_smileys(void) { return psmileys; }

static int ref_count = 0;
static int is_setting_state = 0;

int do_msn_debug = 0;
#define DBG_MSN do_msn_debug

static int do_guess_away = 0;
static int do_rename_contacts = 0;
/*  Module Exports */
PLUGIN_INFO plugin_info = {
	PLUGIN_SERVICE,
	"MSN",
	"Provides MSN Messenger support",
	"$Revision: 1.12 $",
	"$Date: 2009/09/06 13:36:28 $",
	&ref_count,
	plugin_init,
	plugin_finish,
	NULL
};

struct service SERVICE_INFO = { 
				"MSN", 
				-1, 
/*				SERVICE_CAN_OFFLINEMSG | */
				SERVICE_CAN_GROUPCHAT | 
/*				SERVICE_CAN_FILETRANSFER | */
				SERVICE_CAN_MULTIACCOUNT, 
				NULL 
			};
/* End Module Exports */

static void ay_msn_connect_status(const char *msg, void *data);
static void ay_msn_logout( eb_local_account * account );
static int ay_msn_authorize_user( eb_local_account *ela, MsnBuddy *bud );

static void *mi1 = NULL;
static void *mi2 = NULL;

static char ay_msn_host[MAX_PREF_LEN] = MSN_DEFAULT_HOST;
static char ay_msn_port[MAX_PREF_LEN] = MSN_DEFAULT_PORT;

static int plugin_init()
{
	eb_debug(DBG_MSN, "MSN\n");

	ref_count=0;
	input_list * il = g_new0(input_list, 1);
	plugin_info.prefs = il;

	il->widget.entry.value = ay_msn_host;
	il->name = "msn_server";
	il->label = _("Server:");
	il->type = EB_INPUT_ENTRY;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.entry.value = ay_msn_port;
	il->name = "msn_port";
	il->label = _("Port:");
	il->type = EB_INPUT_ENTRY;
	
	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.checkbox.value = &do_guess_away;
	il->name = "do_guess_away";
	il->label = _("Guess status from Away messages");
	il->type = EB_INPUT_CHECKBOX;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.checkbox.value = &do_rename_contacts;
	il->name = "do_rename_contacts";
	il->label = _("Rename my MSN-only contacts whenever they change their alias");
	il->type = EB_INPUT_CHECKBOX;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.checkbox.value = &do_msn_debug;
	il->name = "do_msn_debug";
	il->label = _("Enable debugging");
	il->type = EB_INPUT_CHECKBOX;
	

	psmileys=add_protocol_smiley(psmileys, "(y)", "yes");
	psmileys=add_protocol_smiley(psmileys, "(Y)", "yes");

	psmileys=add_protocol_smiley(psmileys, "(n)", "no");
	psmileys=add_protocol_smiley(psmileys, "(N)", "no");

	psmileys=add_protocol_smiley(psmileys, "(b)", "beer");
	psmileys=add_protocol_smiley(psmileys, "(B)", "beer");

	psmileys=add_protocol_smiley(psmileys, "(d)", "wine");
	psmileys=add_protocol_smiley(psmileys, "(D)", "wine");

	psmileys=add_protocol_smiley(psmileys, "(x)", "girl");
	psmileys=add_protocol_smiley(psmileys, "(X)", "girl");

	psmileys=add_protocol_smiley(psmileys, "(z)", "boy");
	psmileys=add_protocol_smiley(psmileys, "(Z)", "boy");

	psmileys=add_protocol_smiley(psmileys, ":-[", "bat");
	psmileys=add_protocol_smiley(psmileys, ":[", "bat");

	psmileys=add_protocol_smiley(psmileys, "({)", "boy_right");
	psmileys=add_protocol_smiley(psmileys, "(})", "girl_left");

	psmileys=add_protocol_smiley(psmileys, ":)", "smile");
	psmileys=add_protocol_smiley(psmileys, ":-)", "smile");

	psmileys=add_protocol_smiley(psmileys, ":D", "biglaugh");
	psmileys=add_protocol_smiley(psmileys, ":-D", "biglaugh");
	psmileys=add_protocol_smiley(psmileys, ":d", "biglaugh");
	psmileys=add_protocol_smiley(psmileys, ":-d", "biglaugh");

	psmileys=add_protocol_smiley(psmileys, ":O", "oh");
	psmileys=add_protocol_smiley(psmileys, ":-O", "oh");
	psmileys=add_protocol_smiley(psmileys, ":o", "oh");
	psmileys=add_protocol_smiley(psmileys, ":-o", "oh");

	psmileys=add_protocol_smiley(psmileys, ":P", "tongue");
	psmileys=add_protocol_smiley(psmileys, ":-P", "tongue");
	psmileys=add_protocol_smiley(psmileys, ":p", "tongue");
	psmileys=add_protocol_smiley(psmileys, ":-p", "tongue");

	psmileys=add_protocol_smiley(psmileys, ";)", "wink");
	psmileys=add_protocol_smiley(psmileys, ";-)", "wink");

	psmileys=add_protocol_smiley(psmileys, ":(", "sad");
	psmileys=add_protocol_smiley(psmileys, ":-(", "sad");

	psmileys=add_protocol_smiley(psmileys, ":S", "worried");
	psmileys=add_protocol_smiley(psmileys, ":-S", "worried");
	psmileys=add_protocol_smiley(psmileys, ":s", "worried");
	psmileys=add_protocol_smiley(psmileys, ":-s", "worried");

	psmileys=add_protocol_smiley(psmileys, ":-|", "neutral");
	psmileys=add_protocol_smiley(psmileys, ":|", "neutral");

	psmileys=add_protocol_smiley(psmileys, ":'(", "cry");

	psmileys=add_protocol_smiley(psmileys, ":$", "blush");
	psmileys=add_protocol_smiley(psmileys, ":-$", "blush");

	psmileys=add_protocol_smiley(psmileys, "(h)", "cooldude");
	psmileys=add_protocol_smiley(psmileys, "(H)", "cooldude");

	psmileys=add_protocol_smiley(psmileys, ":@", "angry");
	psmileys=add_protocol_smiley(psmileys, ":-@", "angry");

	psmileys=add_protocol_smiley(psmileys, "(A)", "angel");
	psmileys=add_protocol_smiley(psmileys, "(a)", "angel");

	psmileys=add_protocol_smiley(psmileys, "(6)", "devil");

	psmileys=add_protocol_smiley(psmileys, "(L)", "heart");
	psmileys=add_protocol_smiley(psmileys, "(l)", "heart");

	psmileys=add_protocol_smiley(psmileys, "(U)", "broken_heart");
	psmileys=add_protocol_smiley(psmileys, "(u)", "broken_heart");

	psmileys=add_protocol_smiley(psmileys, "(K)", "kiss");
	psmileys=add_protocol_smiley(psmileys, "(k)", "kiss");

	psmileys=add_protocol_smiley(psmileys, "(G)", "gift");
	psmileys=add_protocol_smiley(psmileys, "(g)", "gift");

	psmileys=add_protocol_smiley(psmileys, "(F)", "flower");
	psmileys=add_protocol_smiley(psmileys, "(f)", "flower");

	psmileys=add_protocol_smiley(psmileys, "(W)", "dead_flower");
	psmileys=add_protocol_smiley(psmileys, "(w)", "dead_flower");

	psmileys=add_protocol_smiley(psmileys, "(P)", "camera");
	psmileys=add_protocol_smiley(psmileys, "(p)", "camera");

	psmileys=add_protocol_smiley(psmileys, "(~)", "film");

	psmileys=add_protocol_smiley(psmileys, "(T)", "phone");
	psmileys=add_protocol_smiley(psmileys, "(t)", "phone");

	psmileys=add_protocol_smiley(psmileys, "(@)", "cat");

	psmileys=add_protocol_smiley(psmileys, "(&)", "dog");

	psmileys=add_protocol_smiley(psmileys, "(C)", "coffee");
	psmileys=add_protocol_smiley(psmileys, "(c)", "coffee");

	psmileys=add_protocol_smiley(psmileys, "(I)", "lightbulb");
	psmileys=add_protocol_smiley(psmileys, "(i)", "lightbulb");

	psmileys=add_protocol_smiley(psmileys, "(S)", "moon");

	psmileys=add_protocol_smiley(psmileys, "(*)", "star");

	psmileys=add_protocol_smiley(psmileys, "(#)", "sun");

	psmileys=add_protocol_smiley(psmileys, "(8)", "note");

	psmileys=add_protocol_smiley(psmileys, "(E)", "letter");
	psmileys=add_protocol_smiley(psmileys, "(e)", "letter");

	psmileys=add_protocol_smiley(psmileys, "(^)", "cake");

	psmileys=add_protocol_smiley(psmileys, "(O)", "clock");
	psmileys=add_protocol_smiley(psmileys, "(o)", "clock");

	psmileys=add_protocol_smiley(psmileys, "(M)", "msn_icon");
	psmileys=add_protocol_smiley(psmileys, "(m)", "msn_icon");

	psmileys=add_protocol_smiley(psmileys, "(%)", "handcuffs");

	psmileys=add_protocol_smiley(psmileys, "(?)", "a/s/l");

	psmileys=add_protocol_smiley(psmileys, "(R)", "rainbow");
	psmileys=add_protocol_smiley(psmileys, "(r)", "rainbow");

	psmileys=add_protocol_smiley(psmileys, "8o|", "furious");

	psmileys=add_protocol_smiley(psmileys, "8-|", "nerd");

	psmileys=add_protocol_smiley(psmileys, "+o(", "sick");

	psmileys=add_protocol_smiley(psmileys, "<:o)", "party");
	
	psmileys=add_protocol_smiley(psmileys, "<8-)", "crazy");

	psmileys=add_protocol_smiley(psmileys, "|-)", "sleepy");

	psmileys=add_protocol_smiley(psmileys, "*-)", "thinking");

	psmileys=add_protocol_smiley(psmileys, ":-#", "secret");

	psmileys=add_protocol_smiley(psmileys, ":-*", "whisper");

	psmileys=add_protocol_smiley(psmileys, "^o)", "sarcastic");

	psmileys=add_protocol_smiley(psmileys, "8-)", "eyeroll");

	psmileys=add_protocol_smiley(psmileys, "(bah)", "sheep");
	psmileys=add_protocol_smiley(psmileys, "(BAH)", "sheep");

	psmileys=add_protocol_smiley(psmileys, "(mp)", "mobile");
	psmileys=add_protocol_smiley(psmileys, "(MP)", "mobile");

	psmileys=add_protocol_smiley(psmileys, "(au)", "auto");
	psmileys=add_protocol_smiley(psmileys, "(AU)", "auto");

	psmileys=add_protocol_smiley(psmileys, "(ap)", "plane");
	psmileys=add_protocol_smiley(psmileys, "(AP)", "plane");

	psmileys=add_protocol_smiley(psmileys, "(sn)", "snail");
	psmileys=add_protocol_smiley(psmileys, "(SN)", "snail");

	psmileys=add_protocol_smiley(psmileys, "(co)", "computer");
	psmileys=add_protocol_smiley(psmileys, "(CO)", "computer");

	psmileys=add_protocol_smiley(psmileys, "(mo)", "money");
	psmileys=add_protocol_smiley(psmileys, "(MO)", "money");

	psmileys=add_protocol_smiley(psmileys, "(pi)", "pizza");
	psmileys=add_protocol_smiley(psmileys, "(PI)", "pizza");

	psmileys=add_protocol_smiley(psmileys, "(so)", "soccer");
	psmileys=add_protocol_smiley(psmileys, "(SO)", "soccer");

	psmileys=add_protocol_smiley(psmileys, "(ip)", "island");
	psmileys=add_protocol_smiley(psmileys, "(IP)", "island");

	psmileys=add_protocol_smiley(psmileys, "(um)", "umbrella");
	psmileys=add_protocol_smiley(psmileys, "(UM)", "umbrella");


#ifndef __MINGW32__
	if ((mi1 = eb_add_menu_item(_("Invite to Gnomemeeting"), EB_CHAT_WINDOW_MENU,
			      invite_gnomemeeting, ebmCONTACTDATA, NULL)) == NULL) {
		return (-1);
	}

	if ((mi2 = eb_add_menu_item(_("Invite to Gnomemeeting"), EB_CONTACT_MENU,
			      invite_gnomemeeting, ebmCONTACTDATA, NULL)) == NULL) {
		eb_remove_menu_item(EB_CHAT_WINDOW_MENU, mi1);
		eb_debug(DBG_MOD, "Error!  Unable to add Language menu to contact menu\n");
		return (-1);
	}
	eb_menu_item_set_protocol(mi1, "MSN");
	eb_menu_item_set_protocol(mi2, "MSN");
#endif
	
	return(0);
}

static int plugin_finish()
{
	while(plugin_info.prefs) {
		input_list *il = plugin_info.prefs->next;
		g_free(plugin_info.prefs);
		plugin_info.prefs = il;
	}
	
	if (mi1)
		eb_remove_menu_item(EB_CHAT_WINDOW_MENU, mi1);
	if (mi2)
		eb_remove_menu_item(EB_CONTACT_MENU, mi2);
	mi1 = NULL;
	mi2 = NULL;
	eb_debug(DBG_MSN, "Returning the ref_count: %i\n", ref_count);
	return(ref_count);
}


static void msn_init_account_prefs(eb_local_account *ela)
{
	ay_msn_local_account *mlad = (ay_msn_local_account *)ela->protocol_local_account_data;
	input_list *il = g_new0(input_list, 1);
	ela->prefs = il;
	
	il->widget.entry.value = ela->handle;
	il->name = (char *)"SCREEN_NAME";
	il->label= _("_MSN Login:");
	il->type = EB_INPUT_ENTRY;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.entry.value = mlad->password;
	il->name = (char *)"PASSWORD";
	il->label= _("_Password:");
	il->type = EB_INPUT_PASSWORD;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.checkbox.value = &mlad->prompt_password;
	il->name = (char *)"prompt_password";
	il->label= _("_Ask for password at Login time");
	il->type = EB_INPUT_CHECKBOX;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.checkbox.value = &ela->connect_at_startup;
	il->name = (char *)"CONNECT";
	il->label= _("_Connect at startup");
	il->type = EB_INPUT_CHECKBOX;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.checkbox.value = &mlad->login_invisible;
	il->name = (char *)"LOGIN_INVISIBLE";
	il->label= _("_Login invisible");
	il->type = EB_INPUT_CHECKBOX;

	il->next = g_new0(input_list, 1);
	il = il->next;	
	il->widget.entry.value = mlad->friendlyname;
	il->name = (char *)"fname_pref";
	il->label = _("Friendly Name:");
	il->type = EB_INPUT_ENTRY;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.checkbox.value = &mlad->do_mail_notify;
	il->name = (char *)"do_mail_notify";
	il->label = _("Tell me about new Hotmail/MSN mail");
	il->type = EB_INPUT_CHECKBOX;

/* Unnecessary in the UI
	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.checkbox.value = &mlad->do_mail_notify_folders;
	il->name = (char *)"do_mail_notify_folders";
	il->label = _("Notify me about new mail even if it isn't in my Inbox");
	il->type = EB_INPUT_CHECKBOX;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.checkbox.value = &mlad->do_mail_notify_run_script;
	il->name = "do_mail_notify_run_script";
	il->label = _("Run Script on Mail Notification");
	il->type = EB_INPUT_CHECKBOX;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.entry.value = mlad->do_mail_notify_script_name;
	il->name = "do_mail_notify_script_name";
	il->label = _("Script Name:");
	il->type = EB_INPUT_ENTRY;
*/
}

/*******************************************************************************
 *			     End Module Code
 ******************************************************************************/


/* Aligned to MsnState */
static const char *ay_msn_status_strings[] =
{ "",  "Hidden", "Busy", "Idle", "BRB", "Away", "Phone", "Lunch", "Offline"};


/* TODO
static void ay_html_to_im(MsnIM *msg)
{

}
*/

static gboolean ay_msn_query_connected( eb_account * account )
{
	MsnBuddy *mad = (MsnBuddy *)account->protocol_account_data;
	eb_debug(DBG_MSN,"msn ref_count=%d\n",ref_count);
	
	if(ref_count <= 0 && mad)
		mad->status = MSN_STATE_OFFLINE;
	
	return (mad && mad->status != MSN_STATE_OFFLINE);
}


static void ay_msn_cancel_connect(void *data)
{
	eb_local_account *ela = (eb_local_account *)data;
	ay_msn_local_account * mlad;
	mlad = (ay_msn_local_account *)ela->protocol_local_account_data;
	
	ay_connection_cancel_connect(mlad->connect_tag);

	ela->connecting = 0;
}


static void ay_msn_connect_status(const char *msg, void *data)
{
	MsnConnection *mc = (MsnConnection *)data;
  
	if (mc->type == MSN_CONNECTION_NS) {
		char *handle = mc->account->passport;
	
		eb_local_account *ela = find_local_account_by_handle(handle, SERVICE_INFO.protocol_id);
		if (!ela)
			return;

		ay_msn_local_account *mlad = (ay_msn_local_account *)ela->protocol_local_account_data;
		if (!mlad)
			return;
	
		ay_activity_bar_update_label(mlad->activity_tag, msg);
	}
}


static void ay_msn_finish_login(const char *password, void *data)
{
	eb_local_account *account = (eb_local_account *)data;
	char buff[1024];
	ay_msn_local_account * mlad = (ay_msn_local_account *)account->protocol_local_account_data;
	
	snprintf(buff, sizeof(buff), _("Logging in to MSN account: %s"), account->handle);
	mlad->activity_tag = ay_activity_bar_add(buff, ay_msn_cancel_connect, account);

	mlad->ma->password = strdup(password);

	strcpy(msn_host, ay_msn_host);
	strcpy(msn_port, ay_msn_port);

	if(!mlad->friendlyname[0])
		mlad->ma->friendlyname = strdup(account->alias);
	else
		mlad->ma->friendlyname = strdup(mlad->friendlyname);

	ref_count++;
	
	msn_login(mlad->ma);
}


static void ay_msn_login( eb_local_account * account )
{
	ay_msn_local_account * mlad;
	char buff[1024];
	if(account->connected || account->connecting) {
		eb_debug(DBG_MSN, "called while already logged or logging in\n");
		return;
	}
	
	account->connecting = 1;
	account->connected  = 0;
	
	mlad = (ay_msn_local_account *)account->protocol_local_account_data;
	if (mlad->prompt_password || !mlad->password || !mlad->password[0]) {
		snprintf(buff, sizeof(buff), _("MSN password for: %s"), account->handle);
		do_password_input_window(buff, "", 
				ay_msn_finish_login, account);
	}
	else
		ay_msn_finish_login(mlad->password, account);
}


static void ay_msn_logout( eb_local_account * account )
{
	ay_msn_local_account * mlad = (ay_msn_local_account *)account->protocol_local_account_data;
	LList *l;

	if(mlad->activity_tag)
		ay_activity_bar_remove(mlad->activity_tag);
		
	mlad->activity_tag = mlad->connect_tag = 0;
	eb_debug(DBG_MSN, "Logging out\n");

	for (l = mlad->ma->buddies; l != NULL && l->data != NULL; l = l->next) {
		MsnBuddy *mad = (MsnBuddy *)l->data;
		eb_account * ea = (eb_account *)mad->ext_data;

		mad->status=MSN_STATE_OFFLINE;
		buddy_logoff(ea);
		buddy_update_status(ea);
	}

	if(account->connected)
		msn_logout(mlad->ma);
	else
		msn_account_cancel_connect(mlad->ma);

	account->connected = 0;
	account->connecting = 0;

	is_setting_state = 1;
	eb_set_active_menu_status(account->status_menu, MSN_STATE_OFFLINE);
	is_setting_state = 0;

	if(ref_count >0)
		ref_count--;
}


struct ay_msn_cbdata {
	MsnConnection *mc;
	MsnConnectionCallback callback;
} ;


/* Establish Connection */
void ay_msn_connected(AyConnection *fd, int error, void *data)
{
	MsnConnection *mc = ((struct ay_msn_cbdata *)data)->mc;
	MsnConnectionCallback callback = ((struct ay_msn_cbdata *)data)->callback;
	eb_local_account *ela = (eb_local_account *)mc->account->ext_data;

	mc->ext_data = fd;
	
	if(!fd || error) {
		char errbuf[1024];

		if(error == AY_CANCEL_CONNECT || !ela->connecting) {
			ay_msn_logout(ela);
		}
		else {
			const MsnError *err = msn_strerror(error);
			const char *msg = err->message;

			if(err->error_num != error)
				msg = ay_connection_strerror(error);

			snprintf(errbuf, sizeof(errbuf), "Could not Connect to server %s:\n%s",
					mc->host, msg); /* replace with ay_connection* */
	
			ay_do_error(_("MSN Error"), errbuf);

			ay_msn_logout(ela);
		}

		return;
	}

	mc->tag_c = 0;

	ext_register_read(mc);

	ay_msn_connect_status(_("Connected, sending login information"), mc);

	callback(mc);
}


void ext_msn_connect(MsnConnection *mc, MsnConnectionCallback callback)
{
	int tag;

	struct ay_msn_cbdata *data = g_new0(struct ay_msn_cbdata, 1);
	ay_msn_local_account *mlad;
	eb_local_account *ela;

	data->mc = mc;
	data->callback = callback;

	ela = (eb_local_account *)mc->account->ext_data;
	mlad = (ay_msn_local_account *)ela->protocol_local_account_data;
	
	/* Connection cancelled */
	if(!ela->connecting && !ela->connected)
		return;

	AyConnection *fd = ay_connection_new(mc->host, mc->port, (mc->use_ssl?AY_CONNECTION_TYPE_SSL:AY_CONNECTION_TYPE_PLAIN));

	tag = ay_connection_connect(fd, ay_msn_connected, ay_msn_connect_status, eb_do_confirm_dialog, data);

	if(tag < 0) {
		char buff[1024];
		snprintf(buff, sizeof(buff), _("Cannot connect to %s."), mc->host);
		ay_do_error( _("MSN Error"), buff );
		eb_debug(DBG_MSN, "%s\n", buff);
		ay_activity_bar_remove(mlad->activity_tag);
		mlad->activity_tag = 0;
		ela->connecting = 0;
		ay_msn_logout(ela);
		return;
	}
 
	if (mc->type == MSN_CONNECTION_NS)
		mlad->connect_tag = tag;

	mc->tag_c = tag;
}


static gboolean ay_msn_add_buddy(eb_local_account *ela, MsnBuddy *bud)
{
	char *contact_name;
	struct contact *con;
	grouplist *g;

	gboolean ret = FALSE;

	eb_account *ea = find_account_with_ela(bud->passport, ela);

	if(!bud->friendlyname)
		bud->friendlyname = strdup(bud->passport);

	contact_name = bud->friendlyname;

	if(ea) {
		if(strcmp(ea->account_contact->nick, contact_name)
			&& !strcmp(ea->account_contact->nick, ea->handle))
		{
			rename_contact(ea->account_contact, contact_name);
			ret = TRUE;
		}

		bud->ext_data = ea;
		ea->protocol_account_data = bud;

		return ret;
	}

	g = find_grouplist_by_name(bud->group?bud->group->name:_("Unknown"));

	con = find_contact_in_group_by_nick(contact_name, g);
	if(!con)
		con = find_contact_in_group_by_nick(bud->passport, g);
	if(!con)
		con = find_contact_by_nick(contact_name);
	if(!con)
		con = find_contact_by_nick(bud->passport);

	if(!con) {
		ret = TRUE;
		con=add_new_contact(bud->group?bud->group->name:_("Unknown"), contact_name, SERVICE_INFO.protocol_id);
	}
	ea = ay_msn_new_account_for_buddy(ela, bud);
	add_account(con->nick, ea);

	return ret;
}


/* Done. Set our presence now */
void ext_msn_contacts_synced(MsnAccount *ma)
{
	gboolean update = FALSE;
	eb_local_account *ela = (eb_local_account *)ma->ext_data;
	ay_msn_local_account *mlad = (ay_msn_local_account *)ela->protocol_local_account_data;

	LList *buds = ma->buddies;

	if(!ela->connecting) {
		ay_msn_logout(ela);
		return;
	}

	ay_activity_bar_remove(mlad->activity_tag);
	mlad->activity_tag = 0;

	ela->connected=1;
	ela->connecting=0;

	for(;buds; buds = l_list_next(buds)) {
		MsnBuddy *bud = buds->data;
		int changed = FALSE;

		/* I got a case when buddy was in allow as well as pending. Weird. */
		if (!(bud->list & MSN_BUDDY_ALLOW) && bud->list & MSN_BUDDY_PENDING) {
			changed = ay_msn_authorize_user(ela, bud);
			if(!changed)
				continue;
		}

		if (!(bud->list & MSN_BUDDY_ALLOW)) {
			eb_debug(DBG_MSN, "%s blocked or not in our list. Skipping...\n", bud->passport);
			continue;
		}

		changed = ay_msn_add_buddy(ela, bud);

		if(changed)
			update = TRUE;
	}

	if(update) {
		update_contact_list();
		write_contact_list();
	}

	eb_debug(DBG_MSN,"Connected. Setting state to %d\n", mlad->initial_state);

	is_setting_state = 1;
	eb_set_active_menu_status(ela->status_menu, mlad->initial_state);
	is_setting_state = 0;

	msn_set_initial_presence(ma, mlad->initial_state);
}


void ext_msn_login_response(MsnAccount *ma, int error)
{
	eb_local_account *account = (eb_local_account *)ma->ext_data;
	ay_msn_local_account * mlad;

	mlad = (ay_msn_local_account *)account->protocol_local_account_data;
	
	if(error == MSN_LOGIN_OK) {
		/* Connection was cancelled */
		if(!account->connecting) {
			ay_msn_logout((eb_local_account *)ma->ext_data);	/* Connection was cancelled */
			return;
		}

		ay_msn_connect_status(_("Logged in. Downloading contact information."), ma->ns_connection);

		/* Get contacts list */
		msn_sync_contacts(ma);
	}
	else {
		char buf[1024];

		const MsnError *err = msn_strerror(error);

		snprintf(buf, sizeof(buf), _("MSN Login Failed:\n\n%s"), err->message);

		ay_do_error(_("Login Failed"), buf);

		ay_msn_logout(account);
	}
}


static int ay_msn_send_typing( eb_local_account * from, eb_account * account_to )
{
	ay_msn_local_account *mlad = (ay_msn_local_account *)from->protocol_local_account_data;
	MsnBuddy *buddy = account_to->protocol_account_data;

	if (buddy && iGetLocalPref("do_send_typing_notify")) {
		MsnIM *im = m_new0(MsnIM, 1);
		im->typing = 1;
		if(account_to->account_contact->chatwindow
			&& account_to->account_contact->chatwindow->protocol_local_chat_room_data)
		{
			msn_send_IM_to_sb(account_to->account_contact->chatwindow->protocol_local_chat_room_data, im);
		}
		else {
			buddy->mq = l_list_append(buddy->mq, im);
			msn_send_IM(mlad->ma, buddy);
		}
		return 4;
	}

	return 10;
}


static int ay_msn_send_cr_typing( eb_chat_room *chatroom )
{
	if (!iGetLocalPref("do_send_typing_notify"))
		return 4;

	if(chatroom->protocol_local_chat_room_data) {
		MsnIM *im = m_new0(MsnIM, 1);
		im->typing = 1;
		msn_send_IM_to_sb((MsnConnection *)chatroom->protocol_local_chat_room_data, im);
	}
	else
		return 10;

	return 4;
}


/* TODO Rich text messages */
static void ay_msn_send_im( eb_local_account * from, eb_account * account_to,
					 gchar * mess)
{
	MsnIM *msg = m_new0(MsnIM, 1);
	MsnBuddy *buddy = account_to->protocol_account_data;
	ay_msn_local_account *mlad = from->protocol_local_account_data;

	msg->body = strdup(mess);

	buddy->mq = l_list_append(buddy->mq, msg);

	msn_send_IM(mlad->ma, buddy);
}


static eb_chat_room * ay_msn_make_chat_room( gchar * name, eb_local_account * account, int is_public )
{
	eb_chat_room *ecr = g_new0(eb_chat_room, 1);
	ay_msn_local_account *mlad;

	if(!account) {
		g_warning("NO ela!");
		return NULL;
	}
	
	mlad = account->protocol_local_account_data;

	if(name && *name)
		sprintf(ecr->room_name, "MSN Chat Room (#%s)", name);
	else
		sprintf(ecr->room_name, "MSN :: %s", mlad->ma->passport);

	strcpy(ecr->id, ecr->room_name);
	ecr->fellows = NULL;
	ecr->connected = FALSE;
	ecr->local_user = account;
	ecr->protocol_local_chat_room_data = NULL;
	
	eb_join_chat_room(ecr, TRUE);
	eb_chat_room_buddy_arrive(ecr, account->alias, mlad->ma->passport);

	return ecr;
}


static eb_chat_room *ay_msn_find_chat_room(const char *name)
{
	char room[255];

	snprintf(room, sizeof(room), "MSN Chat Room (#%s)", name);
	return find_chat_room_by_id(room);
}


void ext_new_sb(MsnConnection *mc)
{
	MsnBuddy *bud;
	SBPayload *payload = mc->sbpayload;
	eb_chat_room *ecr = NULL;

	/* This should be a buddy */
	bud = payload->data;

	/* Create a chat room if the buddy already has an active switchboard with me */
	if(bud && !bud->sb) {
		bud->sb = mc;
		payload->data = NULL;
	}
	else {
		ecr = ay_msn_find_chat_room(payload->session_id);
		if(!ecr)
			ecr = ay_msn_make_chat_room(payload->session_id, mc->account->ext_data, 0);

		mc->sbpayload->data = ecr;
		ecr->protocol_local_chat_room_data = mc;
	}
}


void ext_got_ans(MsnConnection *mc)
{

}


void ext_buddy_left(MsnConnection *mc, const char *passport)
{
	SBPayload *payload = mc->sbpayload;
	eb_chat_room *ecr = ay_msn_find_chat_room(payload->session_id);

	if(ecr)
		eb_chat_room_buddy_leave(ecr, passport);
}


void ext_buddy_joined_chat(MsnConnection *mc, char *passport, char *friendlyname)
{
	SBPayload *payload = mc->sbpayload;
	eb_chat_room *ecr;

	if((ecr = ay_msn_find_chat_room(payload->session_id)))
		eb_chat_room_buddy_arrive(ecr, friendlyname, passport);
}


static void ay_msn_send_file(eb_local_account *from, eb_account *to, char *file)
{
	/* Not implemented */
}


static void ay_msn_add_user(eb_account * account )
{
	ay_msn_local_account *mlad;
	LList *buds;

	if(!account->ela || !account->ela->connected)
		return;

	mlad = (ay_msn_local_account *)account->ela->protocol_local_account_data;

	buds = mlad->ma->buddies;

	while(buds) {
		MsnBuddy *bud = buds->data;
		if(!strcasecmp(bud->passport, account->handle) && bud->type & MSN_BUDDY_ALLOW ) {
			eb_debug(DBG_MSN, "Buddy %s Already Exists", bud->passport);
			return;
		}

		buds = l_list_next(buds);
	}

	msn_buddy_add(mlad->ma, account->handle, account->account_contact->nick, account->account_contact->group->name);
}


static eb_local_account * ay_msn_read_local_account_config( LList * values )
{
	char *c = NULL;

	eb_local_account * ela;
	ay_msn_local_account *  mlad;

	if(!values)
		return NULL;

	ela = g_new0(eb_local_account,1);
	mlad = g_new0( ay_msn_local_account, 1);

	ela->protocol_local_account_data = mlad;

	ela->service_id = SERVICE_INFO.protocol_id;

	msn_init_account_prefs(ela);

	eb_update_from_value_pair(ela->prefs, values);

	/*the alias will be the persons login minus the @hotmail.com */
	c = strchr( ela->handle, '@' );
	*c = '\0';
	strncpy(ela->alias, ela->handle, sizeof(ela->alias));
	*c='@';

	mlad->ma = msn_account_new();
	mlad->ma->ext_data = ela;
	mlad->ma->status = MSN_STATE_OFFLINE;
	mlad->ma->passport = strdup(ela->handle);

	return ela;
}

static LList * ay_msn_write_local_config( eb_local_account * ela )
{
	return eb_input_to_value_pair(ela->prefs);
}


static eb_account * ay_msn_read_account_config( eb_account *ea, LList * config)
{
	ay_msn_add_user(ea);

	return ea;
}


static LList * ay_msn_get_states(void)
{
	LList * list = NULL;
	int i=0;

	for(i=0; i<MSN_STATES_COUNT; i++)
		list = l_list_append( list, (void *)(ay_msn_status_strings[i][0]?ay_msn_status_strings[i]:"Online") );

	return list;
}


static gint ay_msn_get_current_state( eb_local_account * account )
{
	ay_msn_local_account * mlad = (ay_msn_local_account *)account->protocol_local_account_data;
	return mlad->ma->status;
}


static void ay_msn_set_current_state( eb_local_account * account, gint state )
{
	ay_msn_local_account * mlad = (ay_msn_local_account *)account->protocol_local_account_data;

	if(!account || !account->protocol_local_account_data) {
		g_warning("ACCOUNT state == NULL!!!!!!!!!1111111oneone");
		return;
	}

	if(is_setting_state)
		return;

	if(state == MSN_STATE_OFFLINE && account->connected)
		ay_msn_logout(account);
	else if(account->connected) {
		msn_set_state(mlad->ma, state);
	} else {
		mlad->initial_state = state;
		ay_msn_login(account);
	}
}


static char *ay_msn_check_login(const char *user, const char *pass)
{
	if(strchr(user,'@') == NULL)
		return strdup(_("MSN passport ID must have @domain.tld part. For example foo@live.com or bar@hotmail.com"));

	return NULL;
}


static void ay_msn_terminate_chat(eb_account * account )
{
	eb_debug(DBG_MSN, "Terminated chat for %s(%s)(%p)\n", account->handle, account->account_contact->nick,
		account->account_contact->chatwindow);
	if(account->account_contact->chatwindow
		&& account->account_contact->chatwindow->protocol_local_chat_room_data)
	{
		msn_sb_disconnect(account->account_contact->chatwindow->protocol_local_chat_room_data);
	}
}


static void ay_msn_del_user(eb_account * ea)
{
	ay_msn_local_account *mlad = ea->ela->protocol_local_account_data;
	MsnBuddy *bud = ea->protocol_account_data;

	if(bud)
		msn_buddy_remove(mlad->ma, bud);
}


void ext_buddy_removed(MsnAccount *ma, const char *bud)
{
	eb_debug(DBG_MSN, "Removed buddy %s\n", bud);
}


static eb_account *ay_msn_new_account_for_buddy(eb_local_account *ela, MsnBuddy *buddy)
{
	eb_account * ea = g_new0(eb_account, 1);

	eb_debug(DBG_MSN, "Adding new account for existing buddy %s\n", buddy->passport);

	ea->protocol_account_data = buddy;
	ea->ela = ela;
	strncpy(ea->handle, buddy->passport, 255 );
	ea->service_id = SERVICE_INFO.protocol_id;
	buddy->ext_data = ea;

	return ea;
}


static eb_account *ay_msn_new_account(eb_local_account *ela, const char *nick)
{
	eb_account * ea = g_new0(eb_account, 1);

	eb_debug(DBG_MSN, "Adding new account %s\n", nick);

	ea->ela = ela;
	strncpy(ea->handle, nick, 255 );
	ea->service_id = SERVICE_INFO.protocol_id;

	return ea;
}


static void ay_msn_ignore_user(eb_account *ea)
{
	/* Not implemented */
}


static void ay_msn_unignore_user(eb_account *ea, const char *new_group)
{
	/* Not implemented */
}


static GdkPixbuf *msn_icon_online = NULL;
static GdkPixbuf *msn_icon_away = NULL;


static const void *ay_msn_get_status_pixbuf( eb_account * account)
{
	MsnBuddy * mad = (MsnBuddy *)account->protocol_account_data;

	if(!msn_icon_online) {
		msn_icon_online = gdk_pixbuf_new_from_xpm_data((const char **)msn_online_xpm);
		msn_icon_away = gdk_pixbuf_new_from_xpm_data((const char **)msn_away_xpm);
	}

	if (mad && mad->status == MSN_STATE_ONLINE)
		return msn_icon_online;
	else 
		return msn_icon_away;
}


static const char * ay_msn_get_status_string( eb_account * account )
{
	MsnBuddy * mad = (MsnBuddy *)account->protocol_account_data;

	if(mad)
		return ay_msn_status_strings[mad->status];
	else
		return ay_msn_status_strings[MSN_STATE_OFFLINE];
}


static void ay_msn_set_idle( eb_local_account * account, gint idle )
{

}


static void ay_msn_set_away( eb_local_account * account, char * message, int away )
{
	/* Not implemented */
}


static void ay_msn_send_chat_room_message( eb_chat_room * room, gchar * mess )
{
	MsnIM *im = m_new0(MsnIM, 1);
	MsnConnection *sb = room->protocol_local_chat_room_data;
	eb_local_account *ela;

	if(!sb) {
		eb_debug(DBG_MSN, "No Switchboard!\n");
		return;
	}
	
	ela= sb->account->ext_data;

	im->body = strdup(mess);

	msn_send_IM_to_sb(sb, im);

	/* We don't get the message back in msn */
	eb_chat_room_show_message(room, ela->alias, mess);
}


static void ay_msn_leave_chat_room( eb_chat_room * room )
{
	if(room->protocol_local_chat_room_data)
		msn_sb_disconnect(room->protocol_local_chat_room_data);
}


void ext_buddy_added(MsnAccount *ma, MsnBuddy *bud)
{
	eb_debug(DBG_MSN, "Added buddy %s\n", bud->passport);

	/* Associate an eb_account if it has not been done already */
	if(!bud->ext_data) {
		eb_local_account *ela = ma->ext_data;
		eb_account *ea = find_account_with_ela(bud->passport, ela);
		if(!ea) {
			eb_debug(DBG_MSN, "Could not find account!\n");
		}
		bud->ext_data = ea;
		ea->protocol_account_data = bud;
	}
}


void ext_buddy_unblock_response(MsnAccount *ma, int error, MsnBuddy *buddy)
{
	if(error) {
		char buf[1024];

		snprintf(buf, sizeof(buf), _("Could not unblock <i>%s</i>. Server returned an error."), buddy->passport);

		ay_do_warning(_("MSN"), buf);
	}
}


void ext_buddy_block_response(MsnAccount *ma, int error, MsnBuddy *buddy)
{
	if(error) {
		char buf[1024];

		snprintf(buf, sizeof(buf), _("Could not block <i>%s</i>. Server returned an error."), buddy->passport);

		ay_do_warning(_("MSN"), buf);
	}
}


void ext_buddy_group_remove_failed(MsnAccount *ma, MsnBuddy *bud, MsnGroup *group)
{
	char buf[1024];

	snprintf(buf, sizeof(buf), _("Could not remove <i>%s</i> from group <i>%s</i>.\nServer returned an error."), 
		bud->passport, group->name);

	ay_do_warning(_("MSN"), buf);
}


void ext_buddy_group_add_failed(MsnAccount *ma, MsnBuddy *bud, MsnGroup *group)
{
	char buf[1024];

	snprintf(buf, sizeof(buf), _("Could not add <i>%s</i> to group <i>%s</i>.\nServer returned an error."), 
		bud->passport, group->name);

	ay_do_warning(_("MSN"), buf);
}


void ext_group_add_failed(MsnAccount *ma, const char *group, char *msg)
{
	char buf[1024];

	snprintf(buf, sizeof(buf), _("Unable to add group <b>%s</b>. Server returned error:\n\n<i>%s</i>"),
		group, msg?msg:_("Unknown error"));

	ay_do_warning(_("MSN"), buf);
}


void ext_buddy_add_failed(MsnAccount *ma, const char *passport, char *friendlyname)
{
	char buf[1024];

	snprintf(buf, sizeof(buf), _("Could not add buddy %s(<i>%s</i>). Server returned an error."), passport, friendlyname);

	ay_do_warning(_("MSN"), buf);
}


int ext_buddy_request(MsnAccount *ma, MsnBuddy *bud)
{
	if(ay_msn_authorize_user(ma->ext_data, bud)) {
		update_contact_list();
		write_contact_list();

		return 1;
	}
	return 0;
}


static int ay_msn_authorize_user( eb_local_account *ela, MsnBuddy *bud )
{
	char buff[1024];
	int result;

	ay_msn_local_account *mlad = ela->protocol_local_account_data;

	snprintf(buff, sizeof(buff), 
			_("The MSN user:\n\n <b>%s(%s)</b>\n\nhas added you to their contact list.\nDo you want to allow this?"), 
			bud->friendlyname?bud->friendlyname:bud->passport, bud->passport);

	if((result = eb_do_confirm_dialog(buff, _("MSN New Contact"))))
		ay_msn_add_buddy(ela, bud);
	else
		msn_buddy_remove_pending(mlad->ma, bud);

	return result;
}


static void ay_msn_invite_callback(MsnConnection *sb, int error, void *data)
{
	eb_chat_room *room = data;

	if(error) {
		const MsnError *err = msn_strerror(error);
		ext_msn_error(sb, err);
		room->protocol_local_chat_room_data = NULL;
		return;
	}

	sprintf(room->room_name, "MSN Chat Room (#%s)", sb->sbpayload->session_id);
	strcpy(room->id, room->room_name);

	room->protocol_local_chat_room_data = sb;
	sb->sbpayload->data = room;
}


static void ay_msn_send_invite( eb_local_account * account, eb_chat_room * room,
						  char * user, const char * message )
{
	MsnConnection *mc;

	/* Get switchboard if we don't have one */
	if (!(mc = room->protocol_local_chat_room_data)) {
		ay_msn_local_account *mlad = account->protocol_local_account_data;

		msn_get_sb(mlad->ma, user, room, ay_msn_invite_callback);
		/* HACK! we're pointing to the NS connection till our SB is ready so that
		 * we don't keep requesting a new one for every invite*/
		room->protocol_local_chat_room_data = mlad->ma->ns_connection;
		return;	
	}

	while(mc->type != MSN_CONNECTION_SB) {
		gtk_main_iteration();
		mc = room->protocol_local_chat_room_data;

		/* Our SB connection failed. Cry. */
		if(!mc) {
			ay_do_error(_("MSN Invitation"), _("Invite failed!"));
			return;
		}
	}

	/* Invite */
	msn_buddy_invite(room->protocol_local_chat_room_data, user);
}


int ext_confirm_invitation(MsnConnection *mc, const char *buddy)
{
	char msgbuf[1024];

	snprintf(msgbuf, sizeof(msgbuf), _("%s has invited you to chat.\nDo you want to accept?"), buddy);

	return eb_do_confirm_dialog(msgbuf, _("MSN Chat Invitation"));
}


static void ay_msn_get_info( eb_local_account * receiver, eb_account * sender)
{
	ay_do_info(_("MSN"), _("Not Implemented"));
}


static void ay_msn_change_group(eb_account * ea, const char *new_group)
{
	MsnGroup *grp;
	MsnBuddy *bud = ea->protocol_account_data;

	ay_msn_local_account *mlad = ea->ela->protocol_local_account_data;

	LList *l = mlad->ma->groups;

	if(!bud) {
		eb_debug(DBG_MSN, "No buddy home!\n");
		return;
	}

	while(l) {
		grp = l->data;

		if(!strcmp(new_group, grp->name))
			break;

		l = l_list_next(l);
	}

	if(l)
		msn_change_buddy_group(mlad->ma, bud, grp);
}


static void ay_msn_del_group(eb_local_account *ela, const char *group) 
{
	ay_msn_local_account *mlad = ela->protocol_local_account_data;

	MsnGroup *grp;
	LList *l = mlad->ma->groups;

	while(l) {
		grp = l->data;

		if(!strcmp(group, grp->name))
			break;

		l = l_list_next(l);
	}

	if(l)
		msn_group_del(mlad->ma, grp);
}


static void ay_msn_add_group(eb_local_account *ela, const char *group) 
{
	ay_msn_local_account *mlad = ela->protocol_local_account_data;

	msn_group_add(mlad->ma, group);
}


static void ay_msn_rename_group(eb_local_account *ela, const char *ogroup, const char *ngroup) 
{
	ay_msn_local_account *mlad = ela->protocol_local_account_data;

	MsnGroup *grp;
	LList *l = mlad->ma->groups;

	while(l) {
		grp = l->data;

		if(!strcmp(ogroup, grp->name))
			break;

		l = l_list_next(l);
	}

	if(l)
		msn_group_mod(mlad->ma, grp, ngroup);
}


static input_list * ay_msn_get_prefs()
{
	return(NULL);
}


static void ay_msn_read_prefs_config(LList * values)
{

}


static LList * ay_msn_write_prefs_config()
{
	return(NULL);
}


static void ay_msn_incoming(AyConnection *source, eb_input_condition condition, void *data)
{
	char buf[2049];
	int len = 0;
	MsnConnection *mc = (MsnConnection *)data;

	eb_local_account *ela = mc->account->ext_data;

	memset(buf, 0, sizeof(buf));

	/* We're logged out. Clean up. */
	if((!ela->connecting && !ela->connected) || !mc->account->ns_connection) {
		ay_msn_logout(ela);
		return;
	}


	if(condition&EB_INPUT_EXCEPTION) {
		if(mc->type == MSN_CONNECTION_SB) {
			eb_debug(DBG_MSN, "Disconnected switchboard %p due to inactivity\n", source);
			eb_chat_room *ecr = mc->sbpayload->data;

			if(ecr)
				ecr->protocol_local_chat_room_data = NULL;

			msn_sb_disconnected(mc);
		}
		else {
			eb_debug(DBG_MSN, "Exception in %p: %s\n", source, strerror(errno));
			abort();	/* To catch cases when this can happen */
		}

		return;
	} 

	while ( (len = ext_msn_read(source, buf, sizeof(buf) - 1)) >= 0 ) {
		eb_debug(DBG_MSN, "Received from %p (%d bytes):: %s\n", source, len, buf);

		if(msn_got_response(mc, buf, len))
			break;

		memset(buf, 0, sizeof(buf));
	}

	if(len < 0 && errno != EAGAIN && errno != EINTR) {
		char errbuf[1024];
		char buf[1024];

		strerror_r(errno, errbuf, sizeof(errbuf));
		snprintf(buf, sizeof(buf), _("Connection error: %s"), errbuf);

		ext_show_error(mc, buf);
	}
}


void ext_msn_send_data(MsnConnection *mc, char *data, int len)
{
	if(len == -1)
		len=strlen(data);

	eb_debug(DBG_MSN, "Sending :: %s\n", data);
	
	ay_connection_write(AY_CONNECTION(mc->ext_data), data, len);
}


void ext_register_read(MsnConnection *mc)
{
	eb_debug(DBG_MSN, "Registering sock %p\n", mc->ext_data);

	if(mc->tag_r) {
		g_warning("Already registered read. This should not happen!");

		return;
	}

	mc->tag_r = ay_connection_input_add(AY_CONNECTION(mc->ext_data), 
						EB_INPUT_READ, 
						ay_msn_incoming, 
						mc);
}


void ext_register_write(MsnConnection *mc)
{
	eb_debug(DBG_MSN, "Registering sock %p\n", mc->ext_data);

	if(mc->tag_w) {
		g_warning("Already registered write. This should not happen!");

		return;
	}

	mc->tag_w = ay_connection_input_add(AY_CONNECTION(mc->ext_data), 
						EB_INPUT_WRITE, 
						ay_msn_incoming, 
						mc);
}


void ext_unregister_write(MsnConnection *mc)
{
	if(mc->tag_w)
		ay_connection_input_remove(mc->tag_w);

	mc->tag_w = 0;
}


void ext_unregister_read(MsnConnection *mc)
{
	if(mc->tag_r)
		ay_connection_input_remove(mc->tag_r);		

	mc->tag_r = 0;
}


int ext_msn_read(void *fd, char *buf, int len)
{
	return ay_connection_read(AY_CONNECTION(fd), buf, len);
}


int ext_msn_write(void *fd, char *buf, int len)
{
	return ay_connection_write(AY_CONNECTION(fd), buf, len);
}


void ext_update_friendlyname(MsnConnection *mc)
{
	eb_local_account *ela = mc->account->ext_data;
	ay_msn_local_account *mlad = (ay_msn_local_account *)ela->protocol_local_account_data;

	strncpy(ela->alias, mc->account->friendlyname, 255);

	strncpy(mlad->friendlyname, mc->account->friendlyname, MAX_PREF_LEN);

	eb_debug(DBG_MSN, "Your friendlyname is now: %s\n", mlad->friendlyname);
}


void ext_msn_error(MsnConnection *mc, const MsnError *error)
{
	if(error->nsfatal || error->fatal)
		ay_do_error(_("MSN Error"), error->message);
	else
		ay_do_warning(_("MSN :: Operation failed"), error->message);

	if(error->nsfatal)
		ay_msn_logout(mc->account->ext_data);
	else if(error->fatal) {
		if(mc->type == MSN_CONNECTION_SB)
			msn_sb_disconnect(mc);
		else
			ay_msn_logout(mc->account->ext_data);
	}
}


void ext_show_error(MsnConnection *mc, const char *msg)
{
	eb_local_account *ela = (eb_local_account *)mc->account->ext_data;

	char *m = strdup(msg);
	ay_do_error( "MSN Error", m );
	eb_debug(DBG_MSN, "MSN: Error: %s\n", m);
	free(m);

	ay_msn_logout(ela);
}


void ext_got_status_change(MsnAccount *ma)
{
	eb_local_account *ela = (eb_local_account *)ma->ext_data;

	is_setting_state = 1;
	eb_set_active_menu_status(ela->status_menu, ma->status);
	is_setting_state = 0;
}


void ext_got_buddy_status(MsnConnection *mc, MsnBuddy *buddy)
{
	eb_account *ea =  buddy->ext_data;

	if(!ea) {
		eb_debug(DBG_MSN, "Server has gone crazy. Sending me status for some %s\n", buddy->passport);
		return;
	}

	if(strcmp(ea->account_contact->nick, buddy->friendlyname))
		rename_contact(ea->account_contact, buddy->friendlyname);

	if(buddy->status == MSN_STATE_OFFLINE)
		buddy_logoff(ea);
	else
		buddy_login(ea);

	buddy_update_status_and_log(ea);
}


void ext_got_SB(MsnConnection *mc)
{

}

void ext_user_joined(MsnConnection *mc, MsnBuddy *buddy)
{

}

void ext_user_left(MsnConnection *mc, MsnBuddy *buddy)
{

}

static void ay_msn_join_chat_room(eb_chat_room *ecr)
{

}


void ext_got_unknown_IM(MsnConnection *mc, MsnIM *msg, const char *sender)
{
	eb_chat_room * ecr=mc->sbpayload->data;

	eb_account *ea = NULL;
	char *local_account_name=NULL;
	eb_local_account *ela=NULL;

	/* format message */
	ay_msn_format_message (msg);

	/* The username element is always valid, even if it's not an SB */
	local_account_name=mc->account->passport;
	ela = find_local_account_by_handle(local_account_name, SERVICE_INFO.protocol_id);

	if(!ela) {
		eb_debug(DBG_MSN, "Unable to find local account by handle: %s\n", local_account_name);
		return;
	}

	if(ecr!=NULL)
		eb_chat_room_show_message(ecr, sender, msg->body);
	else {
		ea = g_new0(eb_account, 1);
		strncpy(ea->handle, sender, 255);
		ea->service_id = ela->service_id;
		ea->ela = ela;

		add_dummy_contact(sender, ea);

		eb_parse_incoming_message(ela, ea, msg->body);
	}
}


void ext_got_IM(MsnConnection *mc, MsnIM *msg, MsnBuddy *buddy)
{
	eb_chat_room * ecr=mc->sbpayload->data;

	eb_account *sender = NULL;
	char *local_account_name=NULL;
	eb_local_account *ela=NULL;

	/* format message */
	ay_msn_format_message (msg);

	/* The username element is always valid, even if it's not an SB */
	local_account_name=mc->account->passport;
	ela = find_local_account_by_handle(local_account_name, SERVICE_INFO.protocol_id);

	if(!ela) {
		eb_debug(DBG_MSN, "Unable to find local account by handle: %s\n", local_account_name);
		return;
	}

	/* Do we need to set our state? */
	sender = buddy->ext_data;

	if (!sender) {
		eb_debug(DBG_MSN, "Cannot find sender: %s, quitting\n", buddy->passport);
		return;
	}
 
	if(ecr!=NULL) {
		if(sender->account_contact->nick)
			eb_chat_room_show_message(ecr, sender->account_contact->nick, msg->body);
		else
			eb_chat_room_show_message(ecr, buddy->friendlyname, msg->body);

		if(sender != NULL)
			eb_update_status(sender, NULL);
	
		return;
	}

	eb_parse_incoming_message(ela, sender, msg->body);
	if(sender != NULL)
		eb_update_status(sender, NULL);
}

void ext_IM_failed(MsnConnection *mc)
{
	printf("**************************************************\n");
	printf("ERROR:  Your last message failed to send correctly\n");
	printf("**************************************************\n");
}

/*void ext_filetrans_invite(MsnConnection *mc, MsnInvitation *mi)
{
	char dialog_message[1025];

	snprintf(dialog_message, sizeof(dialog_message), 
		_("The MSN user %s (%s) would like to send you this file:\n\n   %s (%lu bytes).\n\nDo you want to accept this file ?"),
		mi->friendlyname, mi->sender, mi->filename, mi->filesize);

	eb_debug(DBG_MSN, "got invitation : inv->filename:%s, inv->filesize:%lu\n",
 		mi->filename,
		mi->filesize);

	eb_do_dialog(dialog_message, _("Accept file"), ay_msn_filetrans_callback, (gpointer)mi);
}*/

void ext_start_netmeeting(char *ip)
{

}

/*void ext_netmeeting_invite(MsnConnection *mc, MsnInvitation* inv)
{
	char dialog_message[1025];
	snprintf(dialog_message, sizeof(dialog_message), 
		_("The MSN user %s (%s) would like to speak with you using (Gnome|Net)Meeting.\n\nDo you want to accept ?"),
		mi->friendlyname, mi->from);

	eb_debug(DBG_MSN, "got netmeeting invitation\n");
	eb_do_dialog(dialog_message, _("Accept invitation"), ay_msn_netmeeting_callback, (gpointer) inv );
}*/

void ext_initial_email(MsnConnection *mc, int unread_ibc, int unread_fold)
{
	char buf[1024];
	eb_local_account *ela = (eb_local_account *)mc->account->ext_data;
	ay_msn_local_account *mlad = (ay_msn_local_account *)ela->protocol_local_account_data;

	if(mlad->do_mail_notify == 0)
		return;

	if(unread_ibc==0 && (!mlad->do_mail_notify_folders || unread_fold==0))
		return;

	snprintf(buf, 1024, "You have %d new %s in your Inbox",
		unread_ibc, (unread_ibc==1)?("message"):("messages"));

	if(mlad->do_mail_notify_folders) {
		int sl=strlen(buf);
		snprintf(buf+sl, 1024-sl, ", and %d in other folders", unread_fold);
	}

	ay_do_info( _("MSN Mail"), buf );
}


void ext_new_mail_arrived(MsnConnection *mc, char * from, char * subject) {
	char buf[1024];
	eb_local_account *ela = (eb_local_account *)mc->ext_data;
	ay_msn_local_account *mlad = (ay_msn_local_account *)ela->protocol_local_account_data;

	if(!mlad->do_mail_notify)
		return;

	if (!mlad->do_mail_notify_run_script) {
		snprintf(buf, sizeof(buf), "New mail from %s: \"%s\"", from, subject);
		ay_do_info( _("MSN Mail"), buf );
	} else
		msn_new_mail_run_script(ela);
}


void ext_got_typing(MsnConnection *mc, MsnBuddy *buddy)
{
	eb_local_account *ela = (eb_local_account *)mc->account->ext_data;
	eb_account * ea = find_account_with_ela(buddy->passport, ela);

	if(ea != NULL && iGetLocalPref("do_typing_notify"))
		eb_update_status(ea, _("typing..."));
}


void ext_got_IM_sb(MsnConnection *mc, MsnBuddy *buddy)
{
	eb_account *ea = buddy->ext_data;

	/* Attach the connection to a window */
	if(ea->account_contact->chatwindow) {
		eb_debug(DBG_MSN, "Connected to a switchboard for IM\n");
		((eb_chat_room *)ea->account_contact->chatwindow)->protocol_local_chat_room_data = mc;
		mc->sbpayload->data= ea->account_contact->chatwindow;
	}
	else
		eb_debug(DBG_MSN, "Could not connect chat window to the switchboard\n");
}


void ext_changed_state(MsnConnection *mc, char * state)
{
	eb_debug(DBG_MSN, "Your state is now: %s\n", state);
}


void *ext_server_socket(int port)
{
	AyConnection *fd = ay_connection_new("", port, AY_CONNECTION_TYPE_SERVER);

	if(ay_connection_listen(fd) < 0) {
		ay_connection_free(fd);
		return NULL;
	}
	else {
		return fd;
	}
}


void *ext_socket_accept(void *fd)
{
	if (ay_connection_connect(AY_CONNECTION(fd), NULL, NULL, NULL, NULL) < 0)
		return NULL;
	else
		return fd;
}


void ext_msn_free(MsnConnection *mc)
{
	ext_unregister_read(mc);
	ext_unregister_write(mc);
	ay_connection_free(AY_CONNECTION(mc->ext_data));
}


char *ext_base64_encode(unsigned char *in, int len)
{
	return ay_base64_encode(in, len);
}


unsigned char *ext_base64_decode(char *in, int *len)
{
	return ay_base64_decode(in, len);
}


/*
** Name: ay_msn_format_message
** Purpose: format an MSN message following the X-MMS-IM-Format header
** Input: head - the headers ; msg - the message
** Output: the formatted message
*/
static void ay_msn_format_message (MsnIM *msg)
{
	char * retval = NULL;

	if(msg->font==NULL)
		return;

	if (msg->italic)
		retval = g_strdup_printf ("<i>%s</i>", msg->body);
	if (msg->bold)
		retval = g_strdup_printf ("<b>%s</b>", msg->body);
	if (msg->underline)
		retval = g_strdup_printf ("<u>%s</u>", msg->body);
	/* todo: other formatting fields
	 * CO: color - color IS in HTML colour format (RGB hex)
	 * CS: ? (haven't the faintest - Meredydd)
	 * PF: ? (ditto)
	 */
	if (!retval)
		retval = g_strdup (msg->body);

	g_free(msg->body);
	msg->body=retval;
}


static void invite_gnomemeeting(ebmCallbackData * data)
{

}

void msn_new_mail_run_script(eb_local_account *ela)
{
/*
	ay_msn_local_account *mlad = (ay_msn_local_account *)ela->protocol_local_account_data;

	ay_exec(mlad->do_mail_notify_script_name);
*/
}


static int ay_msn_is_suitable(eb_local_account *local, eb_account *remote)
{
	if(!local || !remote)
		return FALSE;

	if(remote->ela == local)
		return TRUE;

	return FALSE;
}


struct service_callbacks * query_callbacks()
{
	struct service_callbacks * sc;

	sc = g_new0( struct service_callbacks, 1 );

	sc->query_connected = ay_msn_query_connected;
	sc->login = ay_msn_login;
	sc->logout = ay_msn_logout;
	sc->send_im = ay_msn_send_im;
	sc->send_typing = ay_msn_send_typing;
	sc->send_cr_typing = ay_msn_send_cr_typing;
	sc->read_local_account_config = ay_msn_read_local_account_config;
	sc->write_local_config = ay_msn_write_local_config;
	sc->read_account_config = ay_msn_read_account_config;
	sc->get_states = ay_msn_get_states;
	sc->get_current_state = ay_msn_get_current_state;
	sc->set_current_state = ay_msn_set_current_state;
	sc->check_login = ay_msn_check_login;
	sc->add_user = ay_msn_add_user;
	sc->del_user = ay_msn_del_user;
	sc->is_suitable = ay_msn_is_suitable;
	sc->new_account = ay_msn_new_account;
	sc->ignore_user = ay_msn_ignore_user;
	sc->unignore_user = ay_msn_unignore_user;
	sc->get_status_string = ay_msn_get_status_string;
	sc->get_status_pixbuf = ay_msn_get_status_pixbuf;
	sc->set_idle = ay_msn_set_idle;
	sc->set_away = ay_msn_set_away;
	sc->send_chat_room_message = ay_msn_send_chat_room_message;
	sc->join_chat_room = ay_msn_join_chat_room;
	sc->leave_chat_room = ay_msn_leave_chat_room;
	sc->make_chat_room = ay_msn_make_chat_room;
	sc->send_invite = ay_msn_send_invite;
	sc->terminate_chat = ay_msn_terminate_chat;
	sc->get_info = ay_msn_get_info;
	sc->send_file = ay_msn_send_file;

	sc->get_prefs = ay_msn_get_prefs;
	sc->read_prefs_config = ay_msn_read_prefs_config;
	sc->write_prefs_config = ay_msn_write_prefs_config;
	sc->add_importers = NULL;

	sc->get_color = ay_msn_get_color;
	sc->get_smileys = ay_msn_get_smileys;
	sc->change_group = ay_msn_change_group;
	sc->del_group = ay_msn_del_group;
	sc->add_group = ay_msn_add_group;
	sc->rename_group = ay_msn_rename_group;
	return sc;
}


