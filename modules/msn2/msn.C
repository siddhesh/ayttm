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
#include <fcntl.h>
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
#include "msn_core.h"
#include "msn_bittybits.h"
#include "message_parse.h"
#include "browser.h"
#include "smileys.h"
#include "file_select.h"
#include "add_contact_window.h"
#include "offline_queue_mgmt.h"
#include "tcp_util.h"
#include "messages.h"
#include "dialog.h"
#include "platform_defs.h"

#include "libproxy/libproxy.h"


/*************************************************************************************
 *                             Begin Module Code
 ************************************************************************************/
/*  Module defines */
#define plugin_info msn2_LTX_plugin_info
#define SERVICE_INFO msn2_LTX_SERVICE_INFO
#define plugin_init msn2_LTX_plugin_init
#define plugin_finish msn2_LTX_plugin_finish
#define module_version msn2_LTX_module_version


class eb_msn_chatroom : public llist_data
{
  public:
  msnconn * conn;
  eb_chat_room * ecr;
};

class transfer_window : public llist_data
{
  public:
  invitation_ftp * inv;
  int window_tag;
};

typedef struct _eb_msn_local_account_data
{
	char login[MAX_PREF_LEN];
	char password[MAX_PREF_LEN]; // account password
	int fd;				// the file descriptor
	int status;			// the current status of the user
	msnconn * mc;
	int connect_tag;
	int activity_tag;
	LList *msn_contacts;
	int waiting_ans;
	LList *msn_grouplist;
	int listsyncing;
	char fname_pref[MAX_PREF_LEN];
	int do_mail_notify;
	int do_mail_notify_folders;
	int do_mail_notify_run_script;
	char do_mail_notify_script_name[MAX_PREF_LEN];
	int login_invisible;
	int prompt_password;
} eb_msn_local_account_data;


static llist * chatrooms=NULL;
static llist * transfer_windows = NULL;
static llist * waiting_auth_callbacks = NULL;

static eb_chat_room * eb_msn_get_chat_room(msnconn * conn);
static void eb_msn_clean_up_chat_room(msnconn * conn);
static eb_account * eb_msn_new_account(eb_local_account *ela, const char * account );

static LList * psmileys=NULL;

/* Function Prototypes */
extern "C"
{
unsigned int module_version() {return CORE_VERSION;}
static int plugin_init();
static int plugin_finish();
struct service_callbacks * query_callbacks();
//char * msn_create_mail_initial_notify (int unread_ibc, int unread_fold);
//char * msn_create_new_mail_notify (char * from, char * subject);
static void msn_new_mail_run_script(eb_local_account *ela);
static void eb_msn_format_message (message * msg);
static char *eb_msn_get_color(void) { static char color[]="#aa0000"; return color; }
static void close_conn(msnconn *conn);
static void eb_msn_change_group(eb_account * ea, const char *new_group);
static void eb_msn_real_change_group(eb_local_account *ela, eb_account * ea, const char *old_group, const char *new_group);
static void invite_gnomemeeting(ebmCallbackData * data);

static LList *eb_msn_get_smileys(void) { return psmileys; }
}

static int ref_count = 0;

/* hack to check conn */

int do_msn_debug = 0;
#define DBG_MSN do_msn_debug

static char msn_server[MAX_PREF_LEN] = "messenger.hotmail.com";
static char msn_port[MAX_PREF_LEN] = "1863";
static int do_guess_away = 0;
static int do_check_connection = 0;
static int do_reconnect = 0;
static int do_rename_contacts = 0;
/*  Module Exports */
PLUGIN_INFO plugin_info = {
	PLUGIN_SERVICE,
	"MSN",
	"Provides MSN Messenger support",
	"$Revision: 1.86 $",
	"$Date: 2009/05/22 06:02:23 $",
	&ref_count,
	plugin_init,
	plugin_finish,
	NULL
};
struct service SERVICE_INFO = { "MSN", -1, SERVICE_CAN_OFFLINEMSG | SERVICE_CAN_GROUPCHAT | SERVICE_CAN_FILETRANSFER | SERVICE_CAN_MULTIACCOUNT, NULL };
/* End Module Exports */

static void *mi1 = NULL;
static void *mi2 = NULL;
static void eb_msn_set_current_state( eb_local_account * account, gint state );

static int plugin_init()
{
	eb_debug(DBG_MSN, "MSN\n");

	ref_count=0;
	input_list * il = g_new0(input_list, 1);
	plugin_info.prefs = il;
	il->widget.entry.value = msn_server;
	il->name = "msn_server";
	il->label = _("Server:");
	il->type = EB_INPUT_ENTRY;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.entry.value = msn_port;
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
	il->widget.checkbox.value = &do_check_connection;
	il->name = "do_check_connection";
	il->label = _("Check the connection state");
	il->type = EB_INPUT_CHECKBOX;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.checkbox.value = &do_reconnect;
	il->name = "do_reconnect";
	il->label = _("Reconnect if connection unexpectedly drops");
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
	eb_msn_local_account_data *mlad = (eb_msn_local_account_data *)ela->protocol_local_account_data;
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
	il->widget.entry.value = mlad->fname_pref;
	il->name = (char *)"fname_pref";
	il->label = _("Friendly Name:");
	il->type = EB_INPUT_ENTRY;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.checkbox.value = &mlad->do_mail_notify;
	il->name = (char *)"do_mail_notify";
	il->label = _("Tell me about new Hotmail/MSN mail");
	il->type = EB_INPUT_CHECKBOX;

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
}

/*******************************************************************************
 *                             End Module Code
 ******************************************************************************/

//static LList *msn_contacts = NULL;

static transfer_window * eb_find_window_by_inv(invitation_ftp * inv) {
  llist * l = transfer_windows;
  while(l!=NULL) {
    if( ((transfer_window *)l->data)->inv == inv )
      return ((transfer_window *)l->data); 
  }  
  return NULL;
}

/*
static transfer_window * eb_find_window_by_tag(int tag) {
  llist * l = transfer_windows;
  while(l!=NULL) {
    if( ((transfer_window *)l->data)->window_tag == tag )
      return ((transfer_window *)l->data); 
  }  
  return NULL;
}
*/

enum
{
	MSN_ONLINE,
	MSN_HIDDEN,
	MSN_BUSY,
	MSN_IDLE,
	MSN_BRB,
	MSN_AWAY,
	MSN_PHONE,
	MSN_LUNCH,
	MSN_OFFLINE,
};

static char * msn_status_strings[] =
{ "",  "Hidden", "Busy", "Idle", "BRB", "Away", "Phone", "Lunch", "Offline"};
static char * msn_state_strings[] =
{ "NLN",  "HDN", "BSY", "IDL", "BRB", "AWY", "PHN", "LUN", "FLN"};


/* Use this struct to hold any service specific information you need
 * about people on your contact list
 */

typedef struct _eb_msn_account_data
{
	gint status;   //the status of the user
} eb_msn_account_data;

typedef struct _msn_info_data
{
    gchar *profile;
} msn_info_data;

/* Use this struct to hold any service specific information you need about
 * local accounts
 * below are just some suggested values
 */

class pending_invitation : public llist_data
{
  public:
  char * dest; // destination user
  char * path;
  unsigned long size;
  int app;
  pending_invitation() { dest=path=NULL;size=0;app=0; }
  ~pending_invitation() { if(dest!=NULL) { delete dest; } if(path!=NULL) { delete path; } }
};

static llist * pending_invitations=NULL;

static void eb_msn_terminate_chat( eb_account * account );
static void eb_msn_add_user( eb_account * account );
static void eb_msn_del_user( eb_account * account );
static void eb_msn_login( eb_local_account * account );
static void eb_msn_logout( eb_local_account * account );
static void eb_msn_authorize_callback( gpointer data, int response );
static void eb_msn_filetrans_callback( gpointer data, int response );
static void eb_msn_netmeeting_callback( gpointer data, int response );
static int eb_msn_authorize_user( eb_local_account *ela, char * username, char * friendlyname );
static void eb_msn_filetrans_accept(const char * filename, void * invitation);


static gboolean eb_msn_query_connected( eb_account * account )
{
    eb_msn_account_data * mad = (eb_msn_account_data *)account->protocol_account_data;
    eb_debug(DBG_MSN,"msn ref_count=%d\n",ref_count);
    if(ref_count <= 0 && mad)
        mad->status = MSN_OFFLINE;
    return (mad && mad->status != MSN_OFFLINE);
}

static int conncheck_handler = -1;

void ext_disable_conncheck() {
    if (conncheck_handler != -1 && do_check_connection) {
	    eb_timeout_remove(conncheck_handler);
	    conncheck_handler = -1;
    }	
}

static void eb_msn_filetrans_cancel(void *cb_data)
{
	invitation_ftp *inv = (invitation_ftp *)cb_data;
	
	if (inv) {
		eb_debug(DBG_MSN,"cancelling FTP transfer with %s\n", inv->other_user);
		msn_filetrans_cancel(inv);
	}
}

static int checkconn(msnconn *conn) {
	int status = 1;
	eb_local_account * ela = NULL;
	char * local_account_name = NULL;
	local_account_name=((authdata_NS *)conn->auth)->username;
	ela = find_local_account_by_handle(local_account_name, SERVICE_INFO.protocol_id);
	eb_msn_local_account_data * mlad = (eb_msn_local_account_data *)ela->protocol_local_account_data;
	eb_debug(DBG_MSN, "msn: checking conn\n");
	if(mlad->waiting_ans > 2) {
		eb_debug(DBG_MSN, "msn conn closed !\n");
		close_conn(conn);
		mlad->waiting_ans = 0; 
		if (do_reconnect) {
			close_conn(conn);
			eb_msn_login(ela);
		}
		return 1;
	}
	msn_send_ping(conn);
	mlad->waiting_ans++;

	if (status == 0) {
		eb_debug(DBG_MSN, "conn closed... :(\n");
		close_conn(conn);
		mlad->waiting_ans = 0; 
		if (do_reconnect) {
			close_conn(conn);
			eb_msn_login(ela);
		}
		return 1;
	}
	return 1;
}

static void ay_msn_cancel_connect(void *data)
{
	eb_local_account *ela = (eb_local_account *)data;
	eb_msn_local_account_data * mlad;
	mlad = (eb_msn_local_account_data *)ela->protocol_local_account_data;
	
	ay_socket_cancel_async(mlad->connect_tag);
	mlad->activity_tag=0;
	eb_msn_logout(ela);
}

static void eb_msn_finish_login(const char *password, void *data)
{
	eb_local_account *account = (eb_local_account *)data;
	int port=atoi(msn_port);
	char buff[1024];
	eb_msn_local_account_data * mlad = (eb_msn_local_account_data *)account->protocol_local_account_data;
	
	snprintf(buff, sizeof(buff), _("Logging in to MSN account: %s"), account->handle);
	mlad->activity_tag = ay_activity_bar_add(buff, ay_msn_cancel_connect, account);

	mlad->mc = new msnconn;
	mlad->mc->ext_data = account;
	for(int a=0; a<20; a++)
	{ mlad->mc->tags[a].fd=-1; 
	  mlad->mc->tags[a].tag_r=-1; 
	  mlad->mc->tags[a].tag_w=-1; }

	ref_count++;
	
	msn_init(mlad->mc, account->handle, (char *)password);
	msn_connect(mlad->mc, msn_server, port);
}

static void eb_msn_login( eb_local_account * account )
{
	eb_msn_local_account_data * mlad;
	char buff[1024];
	if(account->connected || account->connecting) {
		eb_debug(DBG_MSN, "called while already logged or logging in\n");
		return;
	}
	
	account->connecting = 1;
	
	mlad = (eb_msn_local_account_data *)account->protocol_local_account_data;
	if (mlad->prompt_password || !mlad->password || !strlen(mlad->password)) {
		snprintf(buff, sizeof(buff), _("MSN password for: %s"), account->handle);
		do_password_input_window(buff, "", 
				eb_msn_finish_login, account);
	} else {
		eb_msn_finish_login(mlad->password, account);
	}
}

static void eb_msn_connected(eb_local_account * account)
{
	eb_msn_local_account_data * mlad;
	mlad = (eb_msn_local_account_data *)account->protocol_local_account_data;
	
	/* don't change if hidden */
	if (mlad->status == MSN_OFFLINE && !mlad->login_invisible)
		mlad->status=MSN_ONLINE;
	else if (mlad->status == MSN_OFFLINE && mlad->login_invisible)
		mlad->status = MSN_HIDDEN;
	
	if(account->status_menu)
	{
		/* Make sure set_current_state doesn't call us back */
		account->connected=-1;
		eb_set_active_menu_status(account->status_menu, mlad->status);
	}
	account->connected=1;
	account->connecting = 0;
        eb_debug(DBG_MSN,"SETTTING STATE TO %d\n",mlad->status);
        eb_msn_set_current_state(account, mlad->status);
	ay_activity_bar_remove(mlad->activity_tag);
	mlad->connect_tag = 0;
	mlad->activity_tag = 0;
}

static void eb_msn_logout( eb_local_account * account )
{
	eb_msn_local_account_data * mlad = (eb_msn_local_account_data *)account->protocol_local_account_data;
	LList *l;

	if(!account->connected && !account->connecting)
		return;
	
	ay_activity_bar_remove(mlad->activity_tag);
	mlad->activity_tag = mlad->connect_tag = 0;
	eb_debug(DBG_MSN, "Logging out\n");
	for (l = mlad->msn_contacts; l != NULL && l->data != NULL; l = l->next) {
		eb_account * ea = (eb_account *)find_account_with_ela((char *)l->data, account);
		if (ea) {
			eb_msn_account_data * mad = (eb_msn_account_data *)ea->protocol_account_data;
			mad->status=MSN_OFFLINE;
			buddy_logoff(ea);
			buddy_update_status(ea);
		}
	}
	account->connected = 0;
	account->connecting = 0;
	eb_set_active_menu_status(account->status_menu, MSN_OFFLINE);
	eb_debug(DBG_MSN, "mlad->mc now %p\n",mlad->mc);
	if(mlad->mc)
	{
		msn_clean_up(mlad->mc);
		mlad->mc=NULL;
	}
	if(ref_count >0)
		ref_count--;
}

static int eb_msn_send_typing( eb_local_account * from, eb_account * account_to )
{
    llist * list;

    list=msnconnections;

    if (!iGetLocalPref("do_send_typing_notify"))
    	return 4;

    while(1)
    {
      msnconn * c;
      llist * users;

      if(list==NULL) { break ; }
      c=(msnconn *)list->data;
      if(c->type==CONN_NS) { list=list->next; continue; }
      users=c->users;
      // the below sends a typing message into this session if the only other participant is
      // the user we want to talk to
      if(users!=NULL && users->next==NULL && !strcmp(((char_data *)users->data)->c, account_to->handle))
      {
        msn_send_typing(c);
        return 4;
      }

      list=list->next;
    }
    // otherwise, just connect
    return 10;
}

static int eb_msn_send_cr_typing( eb_chat_room *chatroom )
{
    msnconn * conn=(msnconn *)chatroom->protocol_local_chat_room_data;

    if (!iGetLocalPref("do_send_typing_notify"))
    	return 4;
    
    if (conn) {
	msn_send_typing(conn);
        return 4;
    }
    return 10;
}


static void eb_msn_send_im( eb_local_account * from, eb_account * account_to,
					 gchar * mess)
{
	message * msg = new message;
	msg->header = NULL;
	msg->font = NULL;
	msg->colour = NULL;
	msg->content = msn_permstring("text/plain; charset=UTF-8");
        if(strlen(mess)>1100)
        {
          char *begin = NULL;
	  char *remain = NULL;
	  char *lastspace = NULL;
	  
	  begin = (char *)malloc(1100*sizeof(char));
	  
	  strncpy(begin, mess, 1090);
	  
	  lastspace = strrchr(begin, ' ');
	  *lastspace = '\0';
	  
	  remain = (char *) malloc ((strlen(mess)-strlen(begin)+2)*sizeof(char));
	  remain = strdup(mess+strlen(begin)+1);
	  
	  eb_msn_send_im(from, account_to, begin);
	  eb_msn_send_im(from, account_to, remain);
	  
	  free(begin);
	  free(remain);
	  
	  return;
        }
	msg->body = g_strndup(mess, 1098);
        eb_msn_local_account_data * mlad;
	mlad = (eb_msn_local_account_data *)from->protocol_local_account_data;

	msn_send_IM(mlad->mc, account_to->handle, msg);

        delete msg;
}

static void eb_msn_send_file(eb_local_account *from, eb_account *to, char *file)
{
        struct stat stats;
	eb_msn_local_account_data *mlad =
		(eb_msn_local_account_data *)from->protocol_local_account_data;

	if(stat(file, &stats) < 0) {
		ay_do_warning( "MSN Error", "File is not readable." );
		return;
	}
	eb_debug(DBG_MSN, "file==%s\n",file);

        llist * list;

        list=msnconnections;

        while(1)
        {
          msnconn * c;
          llist * users;

          if(list==NULL) { break ; }
          c=(msnconn *)list->data;
          if(c->type==CONN_NS) { list=list->next; continue; }
          users=c->users;
          // the below sends an invitation into this session if the only other participant is
          // the user we want to talk to
          if(users!=NULL && users->next==NULL && !strcmp(((char_data *)users->data)->c, to->handle))
          {
	    invitation_ftp * inv = msn_filetrans_send(c, file);
	    char label[1024];
	    snprintf(label, 1024, "Sending %s...",inv->filename);
	    int tag = ay_progress_bar_add(label, inv->filesize, eb_msn_filetrans_cancel, inv);
	    transfer_window * t_win = new transfer_window;
	    t_win->inv = inv;
	    t_win->window_tag = tag;
	    msn_add_to_llist(transfer_windows, t_win);
	    
            return;
          }

          list=list->next;
        }

        pending_invitation * pfs=new pending_invitation;
        pfs->dest=msn_permstring(to->handle);
        pfs->path=msn_permstring(file);
	pfs->size=stats.st_size;
	pfs->app=APP_FTP;
        msn_add_to_llist(pending_invitations, pfs);

        msn_new_SB(mlad->mc, NULL);
}

static eb_local_account * eb_msn_read_local_account_config( LList * values )
{
	char buff[255];
	char * c = NULL;

	eb_local_account * ela;
	eb_msn_local_account_data *  mlad;
	if(!values)
		return NULL;

	ela = g_new0(eb_local_account,1);
	mlad = g_new0( eb_msn_local_account_data, 1);

	mlad->status = MSN_OFFLINE;
	ela->protocol_local_account_data = mlad;

	ela->service_id = SERVICE_INFO.protocol_id;

	msn_init_account_prefs(ela);

	eb_update_from_value_pair(ela->prefs, values);

	/*the alias will be the persons login minus the @hotmail.com */
	strncpy( mlad->login, ela->handle, sizeof(mlad->login));
	strncpy( buff, ela->handle , sizeof(buff));
	c = strtok( buff, "@" );
	strncpy(ela->alias, buff , sizeof(ela->alias));

	return ela;
}

static LList * eb_msn_write_local_config( eb_local_account * ela )
{
	return eb_input_to_value_pair(ela->prefs);
}


static eb_account * eb_msn_read_account_config( eb_account *ea, LList * config)
{
	eb_msn_account_data * mad = g_new0( eb_msn_account_data, 1 );

	mad->status = MSN_OFFLINE;

	ea->protocol_account_data = mad;
	
	eb_msn_add_user(ea);

	return ea;
}

static LList * eb_msn_get_states()
{
	LList * list = NULL;
	list = l_list_append( list, (void *)"Online" );
	list = l_list_append( list, (void *)"Hidden" );
	list = l_list_append( list, (void *)"Busy" );
	list = l_list_append( list, (void *)"Idle" );
	list = l_list_append( list, (void *)"Be Right Back" );
	list = l_list_append( list, (void *)"Away" );
	list = l_list_append( list, (void *)"On Phone" );
	list = l_list_append( list, (void *)"Out To Lunch" );
	list = l_list_append( list, (void *)"Offline" );
	return list;
}

static gint eb_msn_get_current_state( eb_local_account * account )
{
	eb_msn_local_account_data * mlad = (eb_msn_local_account_data *)account->protocol_local_account_data;
	return mlad->status;
}

static void eb_msn_set_current_state( eb_local_account * account, gint state )
{
	eb_msn_local_account_data * mlad = (eb_msn_local_account_data *)account->protocol_local_account_data;

	if(!account || !account->protocol_local_account_data)
	{
		g_warning("ACCOUNT state == NULL!!!!!!!!!");
		return;
	}
	switch(state) {
	case MSN_OFFLINE:
		if(account->connected)
		{
			msn_set_state(mlad->mc, msn_state_strings[state]);
			eb_msn_logout(account);
		}
		break;
	default:
		eb_debug(DBG_MSN, "Calling MSN_ChangeState as state: %i\n", state);
		if(!account->connected)
			eb_msn_login(account);
		else if(account->connected==1) // This is a stupid workaround to something that
                                                // directly contravenes the API spec and is
                                                // generally BAD. Please don't confess to having
                                                // written this, as I will have to kill you... :^)
			msn_set_state(mlad->mc, msn_state_strings[state]);
		break;
	}
	mlad->status=state;
}

static char * eb_msn_check_login(const char * user, const char * pass)
{
   if(strchr(user,'@') == NULL) {
      return strdup(_("MSN logins must have @domain.tld part."));
   }
   return NULL;
}

static void eb_msn_terminate_chat(eb_account * account )
{}

typedef struct {
	eb_local_account *ela;
	char *username;
	char *fname;
} authorize_cb_data;

static void eb_msn_authorize_callback( gpointer data, int response )
{
  char * username;
  eb_local_account *ela;
  authorize_cb_data *cb_data = (authorize_cb_data *)data;
  username = cb_data->username;
  ela = cb_data->ela;
  eb_msn_local_account_data * mlad = (eb_msn_local_account_data *)ela->protocol_local_account_data;

  eb_account *ac = find_account_with_ela(username, ela);

  if (!mlad) {
	  eb_debug(DBG_MSN, "leaving authorize_callback due to mlad==NULL\n");
	  return;
  }
  	    
  eb_debug(DBG_MSN,"entering authorize_callback\n");
  if(response) {
    if (ac == NULL) {
      ac = eb_msn_new_account(ela, username);
      add_dummy_contact(cb_data->fname, ac);
      msn_add_to_list(mlad->mc, "AL", username);
      edit_account_window_new(ac);
    }
    eb_debug(DBG_MSN, "User (%s) authorized - adding to allow list (AL)\n", username);
      
  } else {
     if (ac != NULL) {
        eb_debug(DBG_MSN, "User (%s) not authorized - removing account\n",username);
        remove_account(ac);
     }
     msn_add_to_list(mlad->mc, "BL", username);
  }
  msn_del_from_llist(waiting_auth_callbacks, (llist_data *)username);	  
}

static void eb_msn_filetrans_callback( gpointer data, int response )
{
  invitation_ftp * inv;
  char *filepath = g_new0(char, 1024);

  inv = (invitation_ftp *) data;

  if (inv->cancelled)
	  return;

  if(inv == NULL) {eb_debug(DBG_MSN, "inv==NULL\n");}
  else		  {eb_debug(DBG_MSN, "inv!=NULL, inv->cookie = %s\n",inv->cookie);}

  snprintf(filepath, 1023, "%s/%s", getenv("HOME"), inv->filename);

  if(response) {
    eb_debug(DBG_MSN, "accepting transfer\n");
    ay_do_file_selection_save(filepath, _("Save file as"), eb_msn_filetrans_accept, (void *)inv);
  }
  else {
    eb_debug(DBG_MSN, "rejecting transfer\n");
    msn_filetrans_reject(inv);
  }
}

static void eb_msn_netmeeting_callback( gpointer data, int response )
{
  invitation_voice * inv;

  inv = (invitation_voice *) data;

  if (inv->cancelled)
	  return;
  
  if(inv == NULL) {eb_debug(DBG_MSN, "inv==NULL\n");}
  else		  {eb_debug(DBG_MSN, "inv!=NULL, inv->cookie = %s\n",inv->cookie);}

  if(response) {
    eb_debug(DBG_MSN, "accepting netmeeting\n");
     msn_netmeeting_accept(inv);
  }
  else {
    eb_debug(DBG_MSN, "rejecting netmeeting\n");
    msn_netmeeting_reject(inv);
  }
}

static void eb_msn_filetrans_accept(const char * filename, void * inv_vd)
{
  invitation_ftp * inv=(invitation_ftp *)inv_vd;
  char label[1024];
  snprintf(label, 1024, "Receiving %s...", filename);
  
  int tag = ay_progress_bar_add(label, inv->filesize, eb_msn_filetrans_cancel, inv);
  
  transfer_window * t_win = new transfer_window;
  
  eb_debug(DBG_MSN, "Accepting now\n");
  t_win->inv = inv;
  t_win->window_tag = tag;
  msn_add_to_llist(transfer_windows, t_win);
    
  msn_filetrans_accept(inv, filename);

  eb_debug(DBG_MSN, "Accept done\n");
}

void ext_filetrans_success(invitation_ftp * inv) {
	char buf[1024];
	snprintf(buf, sizeof(buf), _("The file %s has been successfully transfered."), inv->filename);
	ay_do_info( "MSN File Transfer", buf );
	transfer_window * t_win = eb_find_window_by_inv(inv);
	if (t_win) {
          ay_activity_bar_remove(t_win->window_tag);
	  msn_del_from_llist(transfer_windows, t_win);
	}
}

void ext_filetrans_failed(invitation_ftp * inv, int err, const char * msg)
{
	char buf[1024];
        snprintf(buf, sizeof(buf), "File transfer failed: %s%s", msg, err?"\n\n(The file sender must have a public IP, and his firewall must allow TCP connections to port 6891.)":"");
		ay_do_warning( "MSN File Transfer", buf );
	transfer_window * t_win = eb_find_window_by_inv(inv);
	if (t_win) {
          ay_activity_bar_remove(t_win->window_tag);
	  msn_del_from_llist(transfer_windows, t_win);
	}
}

void ext_filetrans_progress(invitation_ftp * inv, const char * status, unsigned long recv, unsigned long total)
{
        int tag=-1;
	transfer_window * t_win = NULL;
	t_win = eb_find_window_by_inv(inv);
	if (t_win != NULL) {
	  tag = t_win->window_tag;
	  ay_activity_bar_update_label(tag, status);
          ay_progress_bar_update_progress(tag, recv);
        }
}

static void eb_msn_add_user(eb_account * account )
{
	eb_local_account *ela = account->ela;
	if(!ela) { eb_debug(DBG_MSN, "ea->ela is NULL !!\n"); return; }
	eb_msn_local_account_data *mlad = (eb_msn_local_account_data *)ela->protocol_local_account_data;

	mlad->msn_contacts = l_list_append(mlad->msn_contacts, account->handle);

	if (mlad->mc != NULL && !mlad->listsyncing) {
	  msn_del_from_list(mlad->mc, "BL", account->handle);
	  msn_add_to_list(mlad->mc, "FL", account->handle);
	  msn_add_to_list(mlad->mc, "AL", account->handle);
	  if (strcmp(account->account_contact->group->name,_("Buddies"))) {
		  eb_msn_real_change_group(ela, account, _("Buddies"), account->account_contact->group->name);
	  }
        }	
}

static void eb_msn_del_user(eb_account * account )
{
	eb_local_account *ela = account->ela;
	if(!ela) { eb_debug(DBG_MSN, "ea->ela is NULL !!\n"); return; }
	eb_msn_local_account_data *mlad = (eb_msn_local_account_data *)ela->protocol_local_account_data;
	mlad->msn_contacts = l_list_remove(mlad->msn_contacts, account->handle);
	if (mlad->mc != NULL) {
	  msn_del_from_list(mlad->mc, "FL", account->handle);
	  msn_del_from_list(mlad->mc, "AL", account->handle);
        }
}

static eb_account * eb_msn_new_account(eb_local_account *ela, const char * account )
{
	eb_account * ea = (eb_account *)g_new0(eb_account, 1);
	eb_msn_account_data * mad = (eb_msn_account_data *)g_new0( eb_msn_account_data, 1 );

	ea->protocol_account_data = mad;
	ea->ela = ela;
	strncpy(ea->handle, account, 255 );
	ea->service_id = SERVICE_INFO.protocol_id;
	mad->status = MSN_OFFLINE;

	return ea;
}

static void eb_msn_ignore_user(eb_account *ea)
{
	eb_local_account *ela = ea->ela;
	if(!ela) { eb_debug(DBG_MSN, "ea->ela is NULL !!\n"); return; }
	eb_msn_local_account_data *mlad = (eb_msn_local_account_data *)ela->protocol_local_account_data;

	if (!ea)
		return;
	
	eb_msn_change_group(ea, _("Ignore"));
	if (mlad->mc != NULL) {
		msn_del_from_list(mlad->mc, "AL", ea->handle);	
		msn_add_to_list(mlad->mc, "BL", ea->handle);
	}
}

static void eb_msn_unignore_user(eb_account *ea, const char *new_group)
{
	eb_local_account *ela = ea->ela;
	if(!ela) { eb_debug(DBG_MSN, "ea->ela is NULL !!\n"); return; }
	eb_msn_local_account_data *mlad = (eb_msn_local_account_data *)ela->protocol_local_account_data;

	if (!ea)
		return;
	eb_msn_change_group(ea, new_group);
	if (mlad->mc != NULL) {
		msn_del_from_list(mlad->mc, "BL", ea->handle);	
		msn_add_to_list(mlad->mc, "AL", ea->handle);
	}
}

static const char **eb_msn_get_status_pixmap( eb_account * account)
{
	eb_msn_account_data * mad = (eb_msn_account_data *)account->protocol_account_data;

	if (mad->status == MSN_ONLINE)
		return msn_online_xpm;
	else 
		return msn_away_xpm;
}

static gchar * eb_msn_get_status_string( eb_account * account )
{
	eb_msn_account_data * mad = (eb_msn_account_data *)account->protocol_account_data;
	return msn_status_strings[mad->status];
}

static void eb_msn_set_idle( eb_local_account * account, gint idle )
{
    if( idle >= 600 && eb_msn_get_current_state(account) == MSN_ONLINE )
    {
        if(account->status_menu)
        {
	    eb_set_active_menu_status(account->status_menu, MSN_IDLE);

        }
    }
}

static void eb_msn_set_away( eb_local_account * account, char * message, int away )
{
    if(away && message)
    {
        int state=MSN_AWAY;
        if(do_guess_away)
        {
          int pos=0;
          char * lmsg=msn_permstring(message);

          while(lmsg[pos]!='\0')
          { lmsg[pos]=tolower(lmsg[pos]); pos++; }

          if(strstr(lmsg, "be right back") || strstr(lmsg, "brb"))
          { state=MSN_BRB; }

          if(strstr(lmsg, "busy") || strstr(lmsg, "working"))
          { state=MSN_BUSY; }

          if(strstr(lmsg, "phone"))
          { state=MSN_PHONE; }

          if(strstr(lmsg, "eating") || strstr(lmsg, "breakfast") || strstr(lmsg, "lunch") || strstr(lmsg, "dinner"))
          { state=MSN_LUNCH; }

          delete lmsg;
        }

        if(account->status_menu)
        {
           eb_set_active_menu_status(account->status_menu, state);
        }
    }
    else
    {
        if(account->status_menu)
        {
	    eb_set_active_menu_status(account->status_menu, MSN_ONLINE);
        }

    }
}

static void eb_msn_send_chat_room_message( eb_chat_room * room, gchar * mess )
{
	message * msg=new message;
        if(strlen(mess)>1100)
        {
          char *begin = NULL;
	  char *remain = NULL;
	  char *lastspace = NULL;
	  
	  begin = (char *)malloc(1100*sizeof(char));
	  
	  strncpy(begin, mess, 1090);
	  
	  lastspace = strrchr(begin, ' ');
	  *lastspace = '\0';
	  
	  remain = (char *) malloc ((strlen(mess)-strlen(begin)+2)*sizeof(char));
	  remain = strdup(mess+strlen(begin)+1);
	  
	  eb_msn_send_chat_room_message(room, begin);
	  eb_msn_send_chat_room_message(room, remain);
	  
	  free(begin);
	  free(remain);
	  
	  return;
        }
	
	msg->body = g_strndup(mess, 1098);
	msg->font=NULL;
	msg->content=msn_permstring("text/plain; charset=UTF-8");

        /* I conform to this indentation scheme under protest :-) -Meredydd */
        msnconn * conn=(msnconn *)room->protocol_local_chat_room_data;

	if (conn) /* may be closed */
	        msn_send_IM(conn, NULL, msg); // simple, isn't it?

        delete msg;

        eb_chat_room_show_message(room, room->local_user->handle, mess);
}

static void eb_msn_join_chat_room( eb_chat_room * room )
{
	room->connected = TRUE;
}

static void eb_msn_leave_chat_room( eb_chat_room * room )
{
  if (!room || !room->protocol_local_chat_room_data) 
	  return; /* already cleaned by conn timeout */
  
  eb_debug(DBG_MSN,"Leaving chat_room associated with conn %d\n",
		  ((msnconn *)room->protocol_local_chat_room_data)->sock);
  eb_msn_clean_up_chat_room((msnconn *)room->protocol_local_chat_room_data);
  msn_clean_up((msnconn *)room->protocol_local_chat_room_data);
  room->protocol_local_chat_room_data=NULL; // (got cleaned up by the above line)
}

static int is_waiting_auth(char *name) 
{
  llist * l=waiting_auth_callbacks;

  while(l!=NULL)
  {
    char * n=(char *)l->data;
    if(!strcmp(n,name))
	    	return 1;
    l=l->next;
  }
  return 0;	
}

static int eb_msn_authorize_user( eb_local_account *ela, char * username, char * friendlyname )
{
  char dialog_message[1025];
  char *uname;
  eb_msn_local_account_data * mlad = (eb_msn_local_account_data *)ela->protocol_local_account_data;
  eb_debug(DBG_MSN,"entering authorize_user\n");
  /* that's not the right fix */
  if (strlen(friendlyname) > 254 || strlen (username) > 254) {
	  eb_debug(DBG_MSN, "refusing contact %s because its name is too long\n", username);
	  msn_add_to_list(mlad->mc, "BL", username);
	  return 0;
  }
  if(!is_waiting_auth(username)) {
	  authorize_cb_data *cbd = g_new0(authorize_cb_data, 1);
	  eb_debug(DBG_MSN, "** %s (%s) has added you to their list.\n", friendlyname, username);
	  snprintf(dialog_message, sizeof(dialog_message), _("%s, the MSN user %s (%s) would like to add you to their contact list.\n\nDo you want to allow them to see when you are online?"), 
			  ela->handle, friendlyname, username);
  	  uname = msn_permstring(username);
	  msn_add_to_llist(waiting_auth_callbacks, (llist_data *)uname);
	  cbd->username = uname;
	  cbd->ela = ela;
	  cbd->fname = strdup(friendlyname);
	  eb_do_dialog(dialog_message, _("Authorize MSN User"), eb_msn_authorize_callback, (gpointer)cbd );
	  return 1;
  } else return 0;	 
}

static eb_chat_room * eb_msn_make_chat_room( gchar * name, eb_local_account * account, int is_public )
{
	eb_chat_room * ecr = g_new0(eb_chat_room, 1);

        strncpy( ecr->room_name, name, 1024 );
	ecr->fellows = NULL;
	ecr->connected = FALSE;
	ecr->local_user = account;


        msn_new_SB( ((eb_msn_local_account_data *)account->protocol_local_account_data)->mc, ecr);


	return ecr;
}

static void eb_msn_send_invite( eb_local_account * account, eb_chat_room * room,
						  char * user, const char * message )
{
	if (!room->protocol_local_chat_room_data) {
		ay_do_warning( _("MSN Warning"), _("Cannot invite user: connection to the chatroom has been closed."));
		return;	
	}
	
        msn_invite_user( ((msnconn *)room->protocol_local_chat_room_data), user );
}

static void eb_msn_get_info( eb_local_account * receiver, eb_account * sender)
{
    gchar buff[1024];
    g_snprintf(buff, 1024, "http://members.msn.com/%s", sender->handle);
    open_url(NULL, buff);
}

void ext_got_friend(msnconn *conn, char *name, char *groups) 
{
	char *group = NULL, groupname[255];
	eb_local_account *ela = (eb_local_account *)conn->ext_data;
	if(!ela) return;
	eb_account *ea = find_account_with_ela(name, ela);
	eb_msn_local_account_data *mlad = (eb_msn_local_account_data *)ela->protocol_local_account_data;

	if (ea)
		return; /* we already know him */
	
	groupname[0]=0;
	/* FIXME Memory leak here */
	if(strstr(groups,","))
		group = strdup(strstr(groups,",")+1);
	else
		group = groups;
	
	if(strstr(group,","))
		group[ strstr(group,",")-group ] = 0;
	eb_debug(DBG_MSN,"got a friend %s, %s (all=%s)\n",name,group,groups);
	
	ea = eb_msn_new_account(ela, name);
	
	LList *walk = mlad->msn_grouplist;
	for (walk = mlad->msn_grouplist; walk && walk->data; walk=walk->next) {
		value_pair *grpinfo = (value_pair *)walk->data;
		if(grpinfo && !strcmp(grpinfo->value, group)) {
			strncpy(groupname, grpinfo->key, sizeof(groupname));
			eb_debug(DBG_MSN,"found group id %s: %s\n",group, groupname);
		}
	}
	if (groupname == NULL || !groupname[0] || !strcmp("~", groupname))
		strncpy(groupname,_("Buddies"), sizeof(groupname));
	if(!find_grouplist_by_name(groupname))
		add_group(groupname);
	add_unknown(ea);
	move_contact(groupname, ea->account_contact);
	update_contact_list();
	write_contact_list();
}

void ext_got_group(msnconn *conn, char *id, char *name) 
{
	char *eb_name = NULL;
	char *t = NULL;
	eb_local_account *ela = (eb_local_account *)conn->ext_data;
	if(!ela) return;
	eb_msn_local_account_data *mlad = (eb_msn_local_account_data *)ela->protocol_local_account_data;

	
	if (!strcmp(name,"~")) {
		eb_name = _("Buddies");
		t = value_pair_get_value(mlad->msn_grouplist, eb_name);
		if (!t) {
			mlad->msn_grouplist = value_pair_add (mlad->msn_grouplist, eb_name, id);
			eb_debug(DBG_MSN,"got group id %s, %s\n",id,eb_name);
		}
		else {
			free( t );
			t = NULL;
		}
	} 
	t = value_pair_get_value(mlad->msn_grouplist, name);
	if (!t || !strcmp("-1", t)) {
		mlad->msn_grouplist = value_pair_add (mlad->msn_grouplist, name, id);
		eb_debug(DBG_MSN,"got group id %s, %s\n",id,name);
	}
	if (t) {
		free(t);
		t = NULL;
	}
	
	if(strcmp(name,"~") && !find_grouplist_by_name(name)
	&& !group_mgmt_check_moved(name)) /* if we won't remove it in ten seconds */
		add_group(name);
}

typedef struct _movecb_data
{
	char oldgr[255];
	char newgr[255];
	char handle[255];
	eb_local_account *ela;
} movecb_data;

static int finish_group_move(movecb_data *tomove);

static void eb_msn_real_change_group(eb_local_account *ela, eb_account * ea, const char *old_group, const char *new_group)
{
	eb_msn_local_account_data *mlad = (eb_msn_local_account_data *)ela->protocol_local_account_data;
	
	char *oldid = NULL, *newid = NULL;
	const char *int_new_group = NULL, *int_old_group;
	if (!strcmp(_("Buddies"), new_group))
		int_new_group = "~";
	else
		int_new_group = new_group;
	if (!strcmp(_("Buddies"), old_group))
		int_old_group = "~";
	else
		int_old_group = old_group;
	if (!mlad->mc || mlad->listsyncing) /* not now */
		return;
	eb_debug(DBG_MSN,"moving %s from %s to %s\n", ea->handle, int_old_group, int_new_group);
	newid = value_pair_get_value(mlad->msn_grouplist, int_new_group);
	if (newid == NULL || !strcmp("-1",newid)) {
		movecb_data *tomove = g_new0(movecb_data, 1);
		if (newid == NULL) {
			msn_add_group(mlad->mc, (char *)int_new_group);
			ext_got_group(mlad->mc, "-1",(char *)int_new_group);
		}
		else
		{
			free( newid );
		}
		
		strncpy(tomove->handle, ea->handle, sizeof(tomove->handle));
		strncpy(tomove->newgr, int_new_group, sizeof(tomove->newgr));
		strncpy(tomove->oldgr, int_old_group, sizeof(tomove->oldgr));
		tomove->ela = ela;
		eb_timeout_add(1000, (eb_timeout_function)finish_group_move, (gpointer)tomove);
		return;
	} 
	oldid = value_pair_get_value(mlad->msn_grouplist, int_old_group);
	msn_change_group(mlad->mc, ea->handle, oldid, newid);
	
	if ( oldid != NULL )
		free( oldid );
		
	free( newid );
}

static void eb_msn_change_group(eb_account * ea, const char *new_group)
{
	eb_local_account *ela = ea->ela;
	if(!ela) { eb_debug(DBG_MSN, "ea->ela is NULL !!\n"); return; }
	if (ela)
		eb_msn_real_change_group(ela, ea, ea->account_contact->group->name, new_group);
}

static int finish_group_move(movecb_data *tomove) 
{
	char *ngroup = tomove->newgr;
	char *ogroup = tomove->oldgr;
	char *handle = tomove->handle;
	eb_local_account *ela = tomove->ela;
	
	eb_account *ea = find_account_with_ela(handle, ela);
	if (!ea) {eb_debug(DBG_MSN, "ea is NULL !!\n"); return 0;}
	if(!ela) { eb_debug(DBG_MSN, "ea->ela is NULL !!\n"); return 0; }
	eb_msn_local_account_data *mlad = (eb_msn_local_account_data *)ela->protocol_local_account_data;
	if (ela && ea && ogroup && ngroup) {
		char *id = value_pair_get_value(mlad->msn_grouplist, ngroup);
		if (id == NULL || !strcmp(id,"-1")) {
			eb_debug(DBG_MSN,"ID still %s\n",id);
			if ( id != NULL )
				free( id );
			return TRUE;
		}
		eb_debug(DBG_MSN,"Got ID %s\n",id);
		eb_msn_real_change_group(ela, ea,ogroup,ngroup);
		free( id );
		return FALSE;
	}
	return TRUE;
}

static void eb_msn_del_group(eb_local_account *ela, const char *group) 
{
	char *id = NULL;
	eb_msn_local_account_data *mlad = (eb_msn_local_account_data *)ela->protocol_local_account_data;

	if (!group || !strlen(group))
		return;
	
	id = value_pair_get_value(mlad->msn_grouplist, group);
	
	if (!id || !strcmp(id, "-1") || !strcmp(id, "0")) {
		eb_debug(DBG_MSN,"ID for group %s is %s,not deleting\n",group,id);
		if ( id != NULL )
			free ( id );
		return;
	}
	if (mlad->mc) {
		eb_debug(DBG_MSN,"ID for group %s is %s,deleting\n",group,id);
		msn_del_group(mlad->mc, id);
		mlad->msn_grouplist = value_pair_remove(mlad->msn_grouplist, group);
	} else {
		eb_debug(DBG_MSN,"ID for group %s is %s,not deleting because mlad->mc is null\n",group,id);
	}
	
	free( id );
}

static void eb_msn_add_group(eb_local_account *ela, const char *group) 
{
	char *id = NULL;
	eb_msn_local_account_data *mlad = (eb_msn_local_account_data *)ela->protocol_local_account_data;
	
	if (!group || !strlen(group) || !strcmp(group,_("Buddies")))
		return;
	
	id = value_pair_get_value(mlad->msn_grouplist, group);
	
	if (!id && mlad->mc) {
		msn_add_group(mlad->mc, (char *)group);
		ext_got_group(mlad->mc, "-1", (char *)group);
	}
	
	if ( id != NULL )
		free( id );
}

static void eb_msn_rename_group(eb_local_account *ela, const char *ogroup, const char *ngroup) 
{
	char *id = NULL;
	eb_msn_local_account_data *mlad = (eb_msn_local_account_data *)ela->protocol_local_account_data;
	
	if (!ogroup || !strlen(ogroup) || !strcmp(ogroup,_("Buddies")))
		return;
	
	id = value_pair_get_value(mlad->msn_grouplist, ogroup);
	
	if (id && strcmp("-1",id) && mlad->mc) {
		msn_rename_group(mlad->mc, id, (char *)ngroup);
		mlad->msn_grouplist = value_pair_remove(mlad->msn_grouplist, ogroup);
		mlad->msn_grouplist = value_pair_add (mlad->msn_grouplist, ngroup, id);
	}
	
	if ( id != NULL )
		free( id );
}

static input_list * eb_msn_get_prefs()
{
	return(NULL);
}

static void eb_msn_read_prefs_config(LList * values)
{}

static LList * eb_msn_write_prefs_config()
{
	return(NULL);
}

struct service_callbacks * query_callbacks()
{
	struct service_callbacks * sc;

	sc = g_new0( struct service_callbacks, 1 );

	sc->query_connected = eb_msn_query_connected;
	sc->login = eb_msn_login;
	sc->logout = eb_msn_logout;
	sc->send_im = eb_msn_send_im;
        sc->send_typing = eb_msn_send_typing;
        sc->send_cr_typing = eb_msn_send_cr_typing;
	sc->read_local_account_config = eb_msn_read_local_account_config;
	sc->write_local_config = eb_msn_write_local_config;
	sc->read_account_config = eb_msn_read_account_config;
	sc->get_states = eb_msn_get_states;
	sc->get_current_state = eb_msn_get_current_state;
	sc->set_current_state = eb_msn_set_current_state;
	sc->check_login = eb_msn_check_login;
	sc->add_user = eb_msn_add_user;
	sc->del_user = eb_msn_del_user;
	sc->new_account = eb_msn_new_account;
	sc->ignore_user = eb_msn_ignore_user;
	sc->unignore_user = eb_msn_unignore_user;
	sc->get_status_string = eb_msn_get_status_string;
	sc->get_status_pixmap = eb_msn_get_status_pixmap;
	sc->set_idle = eb_msn_set_idle;
	sc->set_away = eb_msn_set_away;
	sc->send_chat_room_message = eb_msn_send_chat_room_message;
	sc->join_chat_room = eb_msn_join_chat_room;
	sc->leave_chat_room = eb_msn_leave_chat_room;
	sc->make_chat_room = eb_msn_make_chat_room;
	sc->send_invite = eb_msn_send_invite;
	sc->terminate_chat = eb_msn_terminate_chat;
        sc->get_info = eb_msn_get_info;
	sc->send_file = eb_msn_send_file;

	sc->get_prefs = eb_msn_get_prefs;
	sc->read_prefs_config = eb_msn_read_prefs_config;
	sc->write_prefs_config = eb_msn_write_prefs_config;
	sc->add_importers = NULL;

        sc->get_color = eb_msn_get_color;
        sc->get_smileys = eb_msn_get_smileys;
        sc->change_group = eb_msn_change_group;
	sc->del_group = eb_msn_del_group;
	sc->add_group = eb_msn_add_group;
	sc->rename_group = eb_msn_rename_group;
	return sc;
}

static void eb_msn_incoming(void *data, int source, eb_input_condition condition)
{
        if(condition&EB_INPUT_EXCEPTION)
        {
          msn_handle_close(source);
        } else {
	  int nargs = 0;
	  char ** args = NULL;
	  llist * list = NULL;
	  list=msnconnections;
	  msnconn *conn = NULL;
	  
	  if(list==NULL) { return; }

	  while(1)
	  {
	    conn=(msnconn *)list->data;
	    if(conn->sock==source)
	    { break; }
	    list=list->next;
	    if(list==NULL)
	    { 
		return; } // not for us
	  }

	  if (conn == NULL) return;

	  if (conn->type != CONN_FTP) {
		  args = msn_read_line(conn, &nargs);
		  if (args == NULL && nargs==0 && conn->type != CONN_FTP) {
			  return; /* not yet */
		  }
		  /* if args == NULL && nargs==-1 msn_handle_incoming will fail */ 
	 	 msn_handle_incoming(conn, condition & EB_INPUT_READ, condition & EB_INPUT_WRITE,
			  args, nargs);
		 if (args) {
			 if(args[0]) {
				 delete [] args[0];
			 }
			 delete [] args;
		 }
	   } else
		  msn_handle_filetrans_incoming(conn, condition & EB_INPUT_READ, condition & EB_INPUT_WRITE);
         }
}


void ext_register_sock(msnconn *conn, int s, int reading, int writing)
{
  eb_debug(DBG_MSN, "Registering sock %i\n", s);
  if (conn->type == CONN_NS) {
     for(int a=0; a<20; a++)
    {
      if(conn->tags[a].fd==s) {eb_debug(DBG_MSN, "already registered"); return;}
    }
   for(int a=0; a<20; a++)
    {
      if(conn->tags[a].fd==-1)
      {
	conn->tags[a].tag_r = conn->tags[a].tag_w = -1;
	if(reading)
	{ conn->tags[a].tag_r = eb_input_add(s, EB_INPUT_READ, eb_msn_incoming, conn); }
	if(writing)
	{ conn->tags[a].tag_w = eb_input_add(s, EB_INPUT_WRITE, eb_msn_incoming, conn); }
	conn->tags[a].fd=s;
	return;
      }
    }
  } else {
    msnconn *parent = NULL;
    if (conn->type == CONN_FTP)
	    parent = find_nsconn_by_name(((authdata_FTP *)conn->auth)->username);
    else
	    parent = find_nsconn_by_name(((authdata_SB *)conn->auth)->username);
    if (parent) {
     for(int a=0; a<20; a++)
    {
      if(parent->tags[a].fd==s) {eb_debug(DBG_MSN, "already registered"); return;}
    }
      for(int a=0; a<20; a++)
      {
	if(parent->tags[a].fd==-1)
	{
	  parent->tags[a].tag_r = parent->tags[a].tag_w = -1;
	  if(reading)
	  { parent->tags[a].tag_r = eb_input_add(s, EB_INPUT_READ, eb_msn_incoming, conn); }
	  if(writing)
	  { parent->tags[a].tag_w = eb_input_add(s, EB_INPUT_WRITE, eb_msn_incoming, conn);}
	  parent->tags[a].fd=s;
	  eb_debug(DBG_MSN, "Added socket %d\n", a);
	  return;
	}
      }
    }
  }
}

void ext_unregister_sock(msnconn *conn, int s)
{
  eb_debug(DBG_MSN, "Unregistering sock %i\n", s);
  if (conn->type == CONN_NS) {	
    for(int a=0; a<20; a++)
    {
      if(conn->tags[a].fd==s)
      {
	eb_input_remove(conn->tags[a].tag_r);		
	eb_input_remove(conn->tags[a].tag_w);
	for(int b=a; b<19; b++)
	{
          conn->tags[b].fd = conn->tags[b+1].fd;
	  conn->tags[b].tag_r = conn->tags[b+1].tag_r;
	  conn->tags[b].tag_w = conn->tags[b+1].tag_w;
	}
	conn->tags[19].fd=-1;
	conn->tags[19].tag_r=-1;
	conn->tags[19].tag_w=-1;
      }
    }
  } else {
     msnconn *parent = NULL;
     if (conn->type == CONN_FTP)
	     parent = find_nsconn_by_name(((authdata_FTP *)conn->auth)->username);
     else
	     parent = find_nsconn_by_name(((authdata_SB *)conn->auth)->username);
     if (parent) {
       for(int a=0; a<20; a++)
       {
	 if(parent->tags[a].fd==s)
	 {
           eb_input_remove(parent->tags[a].tag_r);		
           eb_input_remove(parent->tags[a].tag_w);
	   eb_debug(DBG_MSN, "Unregistered sock %i\n", s);
	   for(int b=a; b<19; b++)
	   {
             parent->tags[b].fd = parent->tags[b+1].fd;
	     parent->tags[b].tag_r = parent->tags[b+1].tag_r;
	     parent->tags[b].tag_w = parent->tags[b+1].tag_w;
	   }
	   parent->tags[19].fd=-1;
	   parent->tags[19].tag_r=-1;
	   parent->tags[19].tag_w=-1;
	 }
       }     
     } else {
	   eb_debug(DBG_MSN, "can't find sock with username %s\n", ((authdata_FTP *)conn->auth)->username);
     }
  }
}

int ext_is_sock_registered(msnconn *conn, int s)
{
  eb_debug(DBG_MSN, "checking sock %i\n", s);
  for(int a=0; a<20; a++)
  {
    if(conn->tags[a].fd==s) {
        eb_debug(DBG_MSN, "Successful %i\n", s);
	return TRUE;
    }
  }
  return FALSE;
}

void ext_got_pong(msnconn *conn)
{
	eb_local_account *ela = (eb_local_account *)conn->ext_data;
	eb_msn_local_account_data * mlad = (eb_msn_local_account_data *)ela->protocol_local_account_data;
	mlad->waiting_ans = 0;	
}

void ext_got_friendlyname(msnconn * conn, char * friendlyname)
{
  eb_local_account * ela = NULL;
  char * local_account_name = NULL;
  eb_debug(DBG_MSN, "Your friendlyname is now: %s\n", friendlyname);
  local_account_name=((authdata_NS *)conn->auth)->username;
  ela = find_local_account_by_handle(local_account_name, SERVICE_INFO.protocol_id);
  eb_msn_local_account_data *mlad = (eb_msn_local_account_data *)ela->protocol_local_account_data;
  strncpy(ela->alias, friendlyname, 255);
  if(mlad->fname_pref[0]=='\0')
  { strncpy(mlad->fname_pref, friendlyname, MAX_PREF_LEN); }

  if(!ela->connected && !ela->connecting) {
	  eb_debug(DBG_MSN,"not connected, shouldn't get it\n");
	  ela->connected = 1;
	  close_conn(conn);
	  msn_clean_up(conn);
	  mlad->mc = NULL;
  }
}

void ext_got_info(msnconn * conn, syncinfo * info)
{
  LList *existing = get_all_accounts(SERVICE_INFO.protocol_id); 
  eb_debug(DBG_MSN, "Got the sync info!\n");
  
  char* local_account_name=((authdata_SB *)conn->auth)->username;
  eb_local_account *ela = find_local_account_by_handle(local_account_name, SERVICE_INFO.protocol_id);
  eb_msn_local_account_data *mlad = (eb_msn_local_account_data *)ela->protocol_local_account_data;
  if (ela == NULL) {
	  eb_debug(DBG_MSN, "ela is null ! :-s");
  } else {
	  eb_msn_connected(ela);
  }
  
  if(mlad->fname_pref[0]!='\0') { 
	  msn_set_friendlyname(conn, mlad->fname_pref); 
  }
  
  /* hack to check conn status */
  if (conncheck_handler == -1 && do_check_connection)
     conncheck_handler = eb_timeout_add(10000, (eb_timeout_function)checkconn, (gpointer)conn);

  for( ; existing != NULL && existing->data != NULL; existing = existing->next) {
	 char *cnt = (char*) existing->data;
	 eb_account *ea = find_account_with_ela(cnt, ela);
	 if (!ea)
		 ea = find_account_by_handle(cnt, SERVICE_INFO.protocol_id); /* get orphaned */
	 if (ea && strcmp(ea->account_contact->group->name, _("Ignore"))
	 && (ea->ela == ela || ea->ela == NULL)) {
		 if (info && !is_on_list(cnt,info->al)) {
			 eb_debug(DBG_MSN,"adding %s to al\n",cnt);
		     msn_add_to_list(mlad->mc, "AL", cnt);
		 }
		 if (info && !is_on_list(cnt,info->fl)) {
			 eb_debug(DBG_MSN,"adding %s to fl\n",cnt);
		     msn_add_to_list(mlad->mc, "FL", cnt);
		 }
	 }
  }

}

void ext_latest_serial(msnconn * conn, int serial)
{
  eb_debug(DBG_MSN, "The latest serial number is: %d\n", serial);
}

void ext_got_GTC(msnconn * conn, char c)
{
  eb_debug(DBG_MSN, "Your GTC value is now %c\n", c);
}

void ext_got_BLP(msnconn * conn, char c)
{
  eb_debug(DBG_MSN, "Your BLP value is now %cL\n", c);
}

int ext_new_RL_entry(msnconn * conn, char * username, char * friendlyname)
{
  eb_local_account *ela = (eb_local_account *)conn->ext_data;
  eb_debug(DBG_MSN, "%s (%s) has added you to their contact list.\nYou might want to add them to your Allow or Block list\n", username, friendlyname);
  return eb_msn_authorize_user(ela, username, friendlyname);
}

void ext_new_list_entry(msnconn * conn, char * list, char * username)
{
  eb_debug(DBG_MSN, "%s is now on your %s\n", username, list);
}

void ext_del_list_entry(msnconn * conn, char * list, char * username)
{
  eb_debug(DBG_MSN, "%s has been removed from your %s\n", username, list);
}

void ext_show_error(msnconn * conn, const char * msg)
{
	char *m = strdup(msg);
	ay_do_warning( "MSN Error", m );
	eb_debug(DBG_MSN, "MSN: Error: %s\n", m);
	free(m);
}

static int get_status_num(char *status)
{
	int count=0;
	for(count=0;count<=MSN_OFFLINE;count++)
		if(!strcmp(msn_state_strings[count], status))
			return(count);
	return(0);
}

void ext_buddy_set(msnconn * conn, char * buddy, char * friendlyname, char * status)
{
    eb_account *ea;
    eb_local_account *ela = (eb_local_account *)conn->ext_data;
    eb_msn_account_data *mad;
    /* UNUSED char *newHandle = NULL; */
    int state=0;
    state=get_status_num(status);
    eb_debug(DBG_MSN, "searching for %s in %s...", buddy, ela->handle);
    ea = find_account_with_ela(buddy, ela);
    if (ea) {
	eb_debug(DBG_MSN, "found\n");
        mad = (eb_msn_account_data *)ea->protocol_account_data;
	if ((do_rename_contacts && l_list_length(ea->account_contact->accounts) == 1)
	|| !strcmp(buddy, ea->account_contact->nick)) {	
	   rename_contact(ea->account_contact, friendlyname);	
	}
    } else {
	    eb_debug(DBG_MSN, "not found, creating new account\n");
	    ea = eb_msn_new_account(ela, buddy);
            mad = (eb_msn_account_data *)ea->protocol_account_data;
	    if(!find_grouplist_by_name(_("Buddies")))
		    add_group(_("Buddies"));
	    add_unknown_with_name(ea, friendlyname);
	    move_contact(_("Buddies"), ea->account_contact);
	    update_contact_list();
	    write_contact_list();
    }
    if ((state != MSN_OFFLINE) && (mad->status == MSN_OFFLINE))
		buddy_login(ea);
    else if ((state == MSN_OFFLINE) && (mad->status != MSN_OFFLINE))
		buddy_logoff(ea);

    if (mad->status != state) {	    
	    mad->status = state;
	    buddy_update_status_and_log(ea);
	    eb_debug(DBG_MSN, "Buddy->online=%i\n", ea->online);
	    eb_debug(DBG_MSN, "%s (%s) is now %s\n", friendlyname, buddy, status);
    }
}

void ext_buddy_offline(msnconn * conn, char * buddy)
{
	eb_local_account *ela = (eb_local_account *)conn->ext_data;
	eb_account * ea = (eb_account *)find_account_with_ela(buddy, ela);
	eb_msn_account_data * mad = NULL;

	eb_debug(DBG_MSN, "%s is now offline\n", buddy);
	if(ea)
	{
		mad =(eb_msn_account_data *)ea->protocol_account_data;
		mad->status=MSN_OFFLINE;
		buddy_logoff(ea);
		buddy_update_status(ea);
	}
}

void ext_got_SB(msnconn * conn, void * tag)
{
  eb_chat_room * ecr = (eb_chat_room *)tag;

  /* WARNING - This will work for now, cause I know how I implemented it,
  but beware - we're well putside the MSN lib's defined API - you really
  ought not to mix the two methods of sending IMs - don't mess with this
  unless you know EXACTLY what you are doing :-) )    -- Meredydd */
  if(tag==NULL)
  {
    // OK, this means it's a filetrans invite

    if(pending_invitations==NULL) { return; } // WTF is this SB FOR??!

    pending_invitation * pfs=(pending_invitation *)pending_invitations->data;

    msn_invite_user(conn, pfs->dest);

    return;
  }

  eb_msn_chatroom * emc = new eb_msn_chatroom;

  emc->conn=conn;
  emc->ecr=ecr;

  msn_add_to_llist(chatrooms, emc);

  ecr->protocol_local_chat_room_data=conn;

  eb_join_chat_room(ecr, TRUE);

  eb_msn_local_account_data *mlad = (eb_msn_local_account_data *)(ecr->local_user->protocol_local_account_data);
  eb_chat_room_buddy_arrive(ecr, mlad->fname_pref[0]!=0 ? mlad->fname_pref:((authdata_SB *)conn->auth)->username,  
		  		 ((authdata_SB *)conn->auth)->username);


  eb_debug(DBG_MSN, "Got switchboard connection\n");
}

void ext_user_joined(msnconn * conn, char * username, char * friendlyname, int is_initial)
{
  eb_chat_room * ecr;

  if((ecr=eb_msn_get_chat_room(conn))==NULL)
  {
    eb_debug(DBG_MSN, "It's not a group chat\n");
    if(msn_count_llist(conn->users)>1)
    {
      eb_debug(DBG_MSN, "making new chat\n");

      ecr = g_new0(eb_chat_room, 1);
      eb_msn_chatroom * emc = new eb_msn_chatroom;

      emc->conn=conn;
      emc->ecr=ecr;

      msn_add_to_llist(chatrooms, emc);
      
      char *roomname = next_chatroom_name();
      strncpy (ecr->room_name, roomname, sizeof(ecr->room_name));
      free(roomname);
      
      ecr->fellows = NULL;
      ecr->connected = FALSE;
      ecr->local_user = (eb_local_account *)conn->ext_data;
      ecr->protocol_local_chat_room_data=conn;

      eb_join_chat_room(ecr, TRUE);

      llist * l=conn->users;

      while(l!=NULL)
      {
	eb_account *acc = NULL;
	acc = find_account_with_ela(((char_data *)l->data)->c, ecr->local_user);
        eb_chat_room_buddy_arrive(ecr, acc?acc->account_contact->nick:((char_data *)l->data)->c, 
				       ((char_data *)l->data)->c);
        l=l->next;
      }

     eb_msn_local_account_data *mlad = (eb_msn_local_account_data *)(ecr->local_user->protocol_local_account_data);
     eb_chat_room_buddy_arrive(ecr, 	mlad->fname_pref[0]!=0?mlad->fname_pref:((authdata_SB *)conn->auth)->username,  
		      			((authdata_SB *)conn->auth)->username);
    } else {
      llist * l=pending_invitations;

      while(1)
      {
        if(l==NULL) { return; }
        pending_invitation * pfs=(pending_invitation *)l->data;

        eb_debug(DBG_MSN, "Checking %s against %s\n", pfs->dest, username);

        if(pfs->app == APP_FTP && !strcmp(pfs->dest, username))
        {
	  invitation_ftp * inv = msn_filetrans_send(conn, pfs->path);
	  char label[1024];
	  snprintf(label, 1024, "Sending %s...",inv->filename);
	  int tag = ay_progress_bar_add(label, inv->filesize, eb_msn_filetrans_cancel, inv);
	  transfer_window * t_win = new transfer_window;
	  
	  t_win->inv = inv;
	  t_win->window_tag = tag;
	  msn_add_to_llist(transfer_windows, t_win);
          msn_del_from_llist(pending_invitations, pfs);
          delete pfs;
          break;
        } else if (pfs->app == APP_NETMEETING && !strcmp(pfs->dest, username)) {
	  msn_invite_netmeeting(conn);
	  msn_del_from_llist(pending_invitations, pfs);
	  delete pfs;
	  break;
	}

        l=l->next;
      }
    }
  } else {
    eb_account *acc = NULL;
    acc = find_account_with_ela(username, ecr->local_user);	 
    eb_debug(DBG_MSN, "Ordinary chat arrival\n");
    eb_chat_room_buddy_arrive(ecr, acc ? acc->account_contact->nick:username, username);
  }

  eb_debug(DBG_MSN, "%s (%s) is now in the session\n", friendlyname, username);
}

void ext_user_left(msnconn * conn, char * username)
{
  eb_local_account *ela = (eb_local_account *)conn->ext_data;	
  eb_account * ea = find_account_with_ela(username, ela);
  eb_chat_room * ecr=eb_msn_get_chat_room(conn);

  if(ecr!=NULL)
  {
    eb_chat_room_buddy_leave(ecr, username);
  }
  else if(ea != NULL)
  {
     eb_update_status(ea, _("(closed window)"));
  }
  else if(ea!=NULL)
  {
    eb_update_status(ea, _("(closed window)"));
  }

  eb_debug(DBG_MSN, "%s has now left the session\n", username);
}

void ext_got_IM(msnconn * conn, char * username, char * friendlyname, message * msg)
{

    eb_chat_room * ecr=eb_msn_get_chat_room(conn);

    /* format message */
    eb_msn_format_message (msg);

    eb_account *sender = NULL;
    eb_account *ea;
    eb_msn_account_data *mad;
    eb_msn_local_account_data *mlad;
    /* UNUSED char *newHandle = NULL */
    char *local_account_name=NULL;
    char *lmess;
    eb_local_account *ela=NULL;

	lmess = strdup(msg->body);
    /* The username element is always valid, even if it's not an SB */
    local_account_name=((authdata_SB *)conn->auth)->username;
    ela = find_local_account_by_handle(local_account_name, SERVICE_INFO.protocol_id);
    if(!ela)
    {
        eb_debug(DBG_MSN, "Unable to find local account by handle: %s\n", local_account_name);
	return;
    }
    /* Do we need to set our state? */
    sender = find_account_with_ela(username, ela);
    if (sender == NULL) {
        eb_debug(DBG_MSN, "Cannot find sender: %s, calling AddHotmail\n", username);
    }
    if (sender == NULL) {
        eb_debug(DBG_MSN, "Still cannot find sender: %s, calling add_unknown\n", username);
        ea = (eb_account *)malloc(sizeof(eb_account));
        mad = g_new0(eb_msn_account_data, 1);
        strncpy(ea->handle, username, sizeof(ea->handle));
        ea->service_id = SERVICE_INFO.protocol_id;
        mad->status = MSN_ONLINE;
        ea->protocol_account_data = mad;
	ea->ela = ela;
        add_dummy_contact(friendlyname, ea);
        sender = ea;
    }
 
    if(ecr!=NULL)
    {
      if(sender->account_contact->nick)
         eb_chat_room_show_message(ecr, sender->account_contact->nick, lmess);
      else
         eb_chat_room_show_message(ecr, username, lmess);
      if(sender != NULL)
         eb_update_status(sender, NULL);

      g_free(lmess);
      return;
    }

    
    //FIXME: There has to be a cleaner way to do this
    if(!strcmp(username, "Hotmail") && (!lmess || !lmess[0]))
    {
	mlad = (eb_msn_local_account_data *)ela->protocol_local_account_data;
	eb_debug(DBG_MSN, "Setting our state to: %s\n", msn_state_strings[mlad->status]);
	msn_set_state(mlad->mc, msn_state_strings[mlad->status]);
	return;
    }

    eb_parse_incoming_message(ela, sender, lmess);
    if(sender != NULL)
       eb_update_status(sender, NULL);
    g_free(lmess);
}

void ext_IM_failed(msnconn * conn)
{
  printf("**************************************************\n");
  printf("ERROR:  Your last message failed to send correctly\n");
  printf("**************************************************\n");
}

void ext_filetrans_invite(msnconn * conn, char * from, char * friendlyname, invitation_ftp * inv)
{
  char dialog_message[1025];
  snprintf(dialog_message, sizeof(dialog_message), _("The MSN user %s (%s) would like to send you this file:\n\n   %s (%lu bytes).\n\nDo you want to accept this file ?"),
  	  friendlyname, from, inv->filename, inv->filesize);
  eb_debug(DBG_MSN, "got invitation : inv->filename:%s, inv->filesize:%lu\n",
  	 inv->filename,
	 inv->filesize);
  eb_do_dialog(dialog_message, _("Accept file"), eb_msn_filetrans_callback, (gpointer) inv );

}

void ext_start_netmeeting(char *ip)
{
	FILE *test=NULL;
	char buf[1024];
	int callto_supported = 0;
	
	test=popen("gnomemeeting --version 2>/dev/null","r");
	if (test==NULL) {
		ay_do_warning( _("GnomeMeeting Error"), _("Cannot run gnomemeeting: presence test failed.") );
		return;
	}
	fgets(buf, sizeof(buf), test);
	pclose(test);
	if(!strstr(buf,"GnomeMeeting") && !strstr(buf,"gnomemeeting")) {
		ay_do_warning( _("GnomeMeeting Error"), _("You do not have gnomemeeting installed or it isn't in your PATH.") );
		return;
	}	
	test=NULL;
	test=popen("gnomemeeting --help 2>&1","r");
	if (test==NULL) {
		ay_do_warning( _("GnomeMeeting Error"), _("Cannot run gnomemeeting: presence test failed.") );
		return;
	}
	while ((fgets(buf, sizeof(buf), test)) != NULL) {
		if(strstr(buf, "--callto") != NULL)
			callto_supported = 1;
		else if (strstr(buf, "--call") != NULL)
			callto_supported = 2;
	}
	pclose(test);	
	
	if (!callto_supported) {
		ay_do_warning( _("GnomeMeeting Error"), _("Your gnomemeeting version doesn't support --callto argument; You should update it.") );
		return;		
	}
	
	/* wait for three seconds because official clients starting 
	netmeeting get their stupid splashscreen */
	if (ip) /* send call */
		snprintf(buf,1024,"(sleep 3; gnomemeeting -c callto://%s) &",ip);
	else /* wait for the incoming call */
		snprintf(buf,1024,"gnomemeeting &");
	/* should check version... 0.85, for example doesn't handle -c without 
	my patch (bugzilla.gnome.org #108767) */
	system(buf);
}

void ext_netmeeting_invite(msnconn * conn, char * from, char * friendlyname, invitation_voice * inv)
{
  char dialog_message[1025];
  snprintf(dialog_message, sizeof(dialog_message), _("The MSN user %s (%s) would like to speak with you using (Gnome|Net)Meeting.\n\nDo you want to accept ?"),
  	  friendlyname, from);
  eb_debug(DBG_MSN, "got netmeeting invitation\n");
  eb_do_dialog(dialog_message, _("Accept invitation"), eb_msn_netmeeting_callback, (gpointer) inv );

}

void ext_initial_email(msnconn * conn, int unread_ibc, int unread_fold)
{
  char buf[1024];
  eb_local_account *ela = (eb_local_account *)conn->ext_data;
  eb_msn_local_account_data *mlad = (eb_msn_local_account_data *)ela->protocol_local_account_data;
  if(mlad->do_mail_notify == 0)
    return;

  if(unread_ibc==0 && (!mlad->do_mail_notify_folders || unread_fold==0)) { return; }

  snprintf(buf, 1024, "You have %d new %s in your Inbox",
    unread_ibc, (unread_ibc==1)?("message"):("messages"));

  if(mlad->do_mail_notify_folders)
  {
    int sl=strlen(buf);
    snprintf(buf+sl, 1024-sl, ", and %d in other folders", unread_fold);
  }

  ay_do_info( _("MSN Mail"), buf );
}

void ext_new_mail_arrived(msnconn * conn, char * from, char * subject) {
  char buf[1024];
  eb_local_account *ela = (eb_local_account *)conn->ext_data;
  eb_msn_local_account_data *mlad = (eb_msn_local_account_data *)ela->protocol_local_account_data;

  if(!mlad->do_mail_notify) { return; }

  if (!mlad->do_mail_notify_run_script) {
  	snprintf(buf, 1024, "New mail from %s: \"%s\"", from, subject);

	  ay_do_info( _("MSN Mail"), buf );
  } else {
	msn_new_mail_run_script(ela);
  }
}
void ext_typing_user(msnconn * conn, char * username, char * friendlyname)
{
  eb_local_account *ela = (eb_local_account *)conn->ext_data;
  eb_account * ea = find_account_with_ela(username, ela);
  if(ea != NULL && iGetLocalPref("do_typing_notify"))
     eb_update_status(ea, _("typing..."));
}

void ext_new_connection(msnconn * conn)
{
  if(conn->type==CONN_NS)
  { 
    // Were's connected to a NS, so lets sync the list.
    // Currently, no caching implemented (*thinks to self - how about $HOME/.everybuddy/contacts?*)
    // ---vance
    msn_sync_lists(conn, 0);
  }
}

void ext_closing_connection(msnconn * conn)
{
  char *local_account_name=NULL;
  eb_local_account *ela=NULL;
  eb_msn_local_account_data * mlad=NULL;
  llist *list = NULL;
  eb_chat_room * ecr=eb_msn_get_chat_room(conn);

  if(ecr!=NULL)
  {
    eb_msn_leave_chat_room(ecr);

    return;
  }

  if(conn->type==CONN_NS)
  {
    local_account_name=((authdata_NS *)conn->auth)->username;
    ela = find_local_account_by_handle(local_account_name, SERVICE_INFO.protocol_id);
    if(!ela)
    {
        eb_debug(DBG_MSN, "Unable to find local account by handle: %s\n", local_account_name);
	return;
    }
    mlad = (eb_msn_local_account_data *)ela->protocol_local_account_data;
    /* We're being called as part of an  msn_clean_up anyway, so make sure eb_msn_logout doesn't try to clean up */
    mlad->mc=NULL;
    eb_msn_logout(ela);	      
    ext_disable_conncheck();
  }
  
  list = conn->invitations_out;
  while (list) {
	  invitation *inv = (invitation *)list->data;
	  if (!inv)
		  break;
	  if(inv->app == APP_FTP) {
		invitation_ftp *iftp = (invitation_ftp *)inv;
		ext_filetrans_failed(iftp,0,"Remote host disconnected");  
	  }  
	  list=list->next;
  }
  list = conn->invitations_in;
  while (list) {
	  invitation *inv = (invitation *)list->data;
	  if (!inv)
		  break;
	  inv->cancelled=1;
	  list=list->next;
  }
  ext_unregister_sock(conn, conn->sock);
  eb_debug(DBG_MSN, "Closed connection with socket %d\n", conn->sock);
}

static void close_conn(msnconn *conn) {
	ext_closing_connection(conn);
}

void ext_changed_state(msnconn * conn, char * state)
{
  eb_debug(DBG_MSN, "Your state is now: %s\n", state);
}

static void ay_msn_connect_status(const char *msg, void *data)
{
  msnconn *conn = (msnconn *)data;
  
  if (conn->type == CONN_NS) {
	  char *handle = ((authdata_NS *)conn->auth)->username;
	  if (!handle) return;
	  eb_local_account *ela = find_local_account_by_handle(handle, SERVICE_INFO.protocol_id);
	  if (!ela) return;
	  eb_msn_local_account_data *mlad =
			(eb_msn_local_account_data *)ela->protocol_local_account_data;
	  if (!mlad) return;
	  ay_activity_bar_update_label(mlad->activity_tag, msg);
  }
}

int ext_async_socket(char *host, int port, void *cb, void *data)
{
  msnconn *conn = (msnconn *)data;
  
  int tag = ay_connect_host(host, port, cb, data, (void *)ay_msn_connect_status);
  
  if (conn->type == CONN_NS) {
	  char *handle = ((authdata_NS *)conn->auth)->username;
	  if (!handle) return -1;
	  eb_local_account *ela = find_local_account_by_handle(handle, SERVICE_INFO.protocol_id);
	  if (!ela) return -1;
	  eb_msn_local_account_data *mlad =
			(eb_msn_local_account_data *)ela->protocol_local_account_data;
	  if (!mlad) return -1;
	  mlad->connect_tag = tag;
  }
  
  return tag;
}

int ext_connect_socket(char * hostname, int port)
{
  struct sockaddr_in sa;
  struct hostent     *hp;
  int s;
  eb_debug(DBG_MSN,"Connecting to %s...\n", hostname);
  if ((hp= gethostbyname(hostname)) == NULL) { /* do we know the host's */
#ifndef __MINGW32__
    errno= ECONNREFUSED;                       /* address? */
#endif
    return(-1);                                /* no */
  }

  memset(&sa,0,sizeof(sa));
  memcpy((char *)&sa.sin_addr,hp->h_addr,hp->h_length);     /* set address */
  sa.sin_family= hp->h_addrtype;
  sa.sin_port= htons((u_short)port);

  if ((s= socket(hp->h_addrtype,SOCK_STREAM,0)) < 0)     /* get socket */
    return(-1);

#ifdef O_NONBLOCK
  fcntl(s, F_SETFL, O_NONBLOCK);
#endif

  if (connect(s,(struct sockaddr *)&sa,sizeof sa) < 0) /* connect */
  {
#ifndef __MINGW32__
    if(errno==EINPROGRESS || errno==EWOULDBLOCK)
    {
      struct pollfd pfd;
      pfd.fd=s;
      pfd.revents=0;
      pfd.events=POLLOUT;

      fcntl(s, F_SETFL, 0);
      if(poll(&pfd, 1, 7500)==1)
      {
        if(pfd.revents&POLLERR||pfd.revents&POLLHUP||pfd.revents&POLLNVAL)
        {
          eb_debug(DBG_MSN, "Error!\n");
          close(s);
          return -1;
        } else {
          eb_debug(DBG_MSN, "Connect went fine\n");
          sleep(2);
          return s;
        }
      }
    } else {
      fcntl(s, F_SETFL, 0);
#endif
      close(s);
      return -1;
#ifndef __MINGW32__
    }
#endif
  }
  sleep(1); // necessary?
  return(s);
}

int ext_server_socket(int port)
{
  int s;
  struct sockaddr_in addr;

  if((s=socket(AF_INET, SOCK_STREAM, 0))<0)
  {
    return -1;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sin_family=AF_INET;
  addr.sin_port=htons(port);

  if(bind(s, (sockaddr *)(&addr), sizeof(addr))<0 || listen(s, 1)<0)
  {
    close(s);
    return -1;
  }

  return s;
}

char * ext_get_IP(void)
{
  return get_local_addresses();
}

/*
char * msn_create_mail_initial_notify (int unread_ibc, int unread_fold)
{
	char * retval = NULL;

	retval = g_strdup_printf(_("<br>You have %d new message%s."),
			      unread_ibc + unread_fold,
			      unread_ibc + unread_fold > 1 ? _("s"):"");
	return retval;
}

char * msn_create_new_mail_notify (char * from, char * subject)
{
	char * retval = NULL;

	retval = g_strdup_printf(_("<br>You have a new message from %s ; its subject is \"%s\"."),
			      from,
			      subject);
	return retval;
}
*/

/*
** Name: eb_msn_format_message
** Purpose: format an MSN message following the X-MMS-IM-Format header
** Input: head - the headers ; msg - the message
** Output: the formatted message
*/
static void eb_msn_format_message (message * msg)
{
	char * retval       = NULL;

        if(msg->font==NULL) { return; }

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
	if (retval==NULL)
		retval = g_strdup (msg->body);

        g_free(msg->body);
        msg->body=retval;
}


static eb_chat_room * eb_msn_get_chat_room(msnconn * conn)
{
  llist * l=chatrooms;

  while(l!=NULL)
  {
    eb_msn_chatroom * emc=(eb_msn_chatroom *)l->data;


    if(emc->conn == conn) { eb_debug(DBG_MSN, "Found chatroom\n"); return emc->ecr; }

    eb_debug(DBG_MSN, "Checking conn with socket %d\n", emc->conn->sock);

    l=l->next;
  }
  eb_debug(DBG_MSN, "Not found chatroom\n");
  return NULL;
}

static void eb_msn_clean_up_chat_room(msnconn * conn)
{
  llist * l=chatrooms;

  while(l!=NULL)
  {
    eb_msn_chatroom * emc=(eb_msn_chatroom *)l->data;

    if(emc->conn == conn)
    {
      if(l->prev!=NULL)
      { l->prev->next=l->next; } else { chatrooms=l->next; }
      if(l->next!=NULL)
      { l->next->prev=l->prev; }
      return;
    }

    l=l->next;
  }
  return;
}

void ext_syncing_lists(msnconn *conn, int state) 
{
	eb_local_account *ela = (eb_local_account *)conn->ext_data;
	if(!ela) return;
	eb_msn_local_account_data *mlad = (eb_msn_local_account_data *)ela->protocol_local_account_data;
	
	mlad->listsyncing = state;	
}

static void invite_gnomemeeting(ebmCallbackData * data)
{
	ebmContactData *ecd = (ebmContactData *) data;
	struct contact *cont;
	eb_account *acc;
	eb_local_account *from;

	from = find_local_account_by_handle(ecd->local_account,SERVICE_INFO.protocol_id);
	if (!from) {
		  ay_do_warning( _("MSN Error"), _("Cannot find a valid local account to invite your contact.") );
		return;
	}
		
	eb_msn_local_account_data *mlad =
		(eb_msn_local_account_data *)from->protocol_local_account_data;
	
	acc = find_account_with_ela(ecd->remote_account, from);
	if (!acc) {
		cont = find_contact_by_nick(ecd->contact);
		if (!cont)
			return;
		acc = find_account_for_protocol(cont, SERVICE_INFO.protocol_id); /*FIXME*/
	}
	if(!acc) {
		  ay_do_warning( _("MSN Error"), _("Cannot find a valid remote account to invite your contact.") );
		return;
	}
	eb_debug(DBG_MSN,"inviting %s to GnomeMeeting via %s\n", acc->handle, ecd->local_account);

        llist * list;

        list=msnconnections;

        while(1)
        {
          msnconn * c;
          llist * users;

          if(list==NULL) { break ; }
          c=(msnconn *)list->data;
          if(c->type==CONN_NS) { list=list->next; continue; }
          users=c->users;
          // the below sends an invitation into this session if the only other participant is
          // the user we want to talk to
          if(users!=NULL && users->next==NULL && !strcmp(((char_data *)users->data)->c, acc->handle))
          {
	    	msn_invite_netmeeting(c);

            return;
          }

          list=list->next;
        }
        pending_invitation * pfs=new pending_invitation;
        pfs->dest=msn_permstring(acc->handle);
	pfs->app=APP_NETMEETING;
        msn_add_to_llist(pending_invitations, pfs);

	msn_new_SB(mlad->mc,NULL);
}

void msn_new_mail_run_script(eb_local_account *ela)
{
	char buf[1024];
	eb_msn_local_account_data *mlad = (eb_msn_local_account_data *)ela->protocol_local_account_data;
	if (!strstr(mlad->do_mail_notify_script_name," &")) {
		snprintf(buf, 1024, "(%s) &", mlad->do_mail_notify_script_name);
	} else {
		strncpy(buf, mlad->do_mail_notify_script_name, 1024);
	}
	system(buf);	
}
