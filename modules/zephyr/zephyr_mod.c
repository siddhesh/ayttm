/*****************************************************************************/
/*** zephyr_mod.c ************************************************************/
/*****************************************************************************/

/*

Implementation of Zephyr functionality

*/

/*** Changelog ***************************************************************/

/*

* Tue Mar 20 2001 Sourav K. Mandal <Sourav.Mandal@ikaran.com>
- "eb_zephyr_login" and "eb_zephyr_logout" now update the protocol
  status menu.

* Wed Feb 07 2001 Sourav K. Mandal <Sourav.Mandal@ikaran.com>
- Fixed handling of unknown contacts.

* Thu Feb 01 2001 Sourav K. Mandal <Sourav.Mandal@ikaran.com>
- Fixed bug that did not reset Zephyr state properly when returning from
  "set away."

* Tue Jan 30 2001 Sourav K. Mandal <Sourav.Mandal@ikaran.com>
- Removed "g_free(zid)" in "zephyr_info_data_cleanup", fixing inexplicable
  segfaults.  The calling routine already does this deallocation.
- Changed checking for immediate ack. notices to use Zephyr predicates.
- Minor improvements and code cleanups.

* Sat Jan 27 2001 Sourav K. Mandal <Sourav.Mandal@ikaran.com>
- Added new icon for hidden status.

* Fri Jan 26 2001 Sourav K. Mandal <Sourav.Mandal@ikaran.com>
- Re-did "zephyr_com_err" to not segfault.
  WARNING: now accesses Zephyr internals!
- Eliminated Zephyr user locating in "eb_zephyr_query_connected".
- Reduced default notice and buddy check intervals.

* Wed Jan 24 2001 Sourav K. Mandal <Sourav.Mandal@ikaran.com>
- Eliminated all dependence on Kerberos by: implementing "com_err"
  in-house, having the behavior of authentication macros contingent
  upon the existence of Kerberos, and making the user name depend on
  the existence of Kerberos.
- Added to prefs options to activate/deactivate authentication when making
  location requests and when sending or receiving notices.

* Mon Jan 15 2001 Sourav K. Mandal <Sourav.Mandal@ikaran.com>
- Fixed freakin' status non-display bug once and for all.  Did not know
  "buddy_log{in,off}" doesn't do anything if you already set the
  "account->online" to the appropriate state.
- Made location flushing optional, through prefs.

* Sun Jan 14 2001 Sourav K. Mandal <Sourav.Mandal@ikaran.com>
- Ported to CVS
- Implemented local prefs handling
- Set up "zephyr_msg_encode" routine for future HTML->Zephyr encoding
  algorithm

* Sat Jan 13 2001 Sourav K. Mandal <Sourav.Mandal@ikaran.com>
- Implemented asynchronous aggregate location checking per suggestion by:
  Zachary M. Loafman <zml@zml.net>

* Fri Jan 12 2001 Sourav K. Mandal <Sourav.Mandal@ikaran.com>
- Miscellaneous bug fixes
- Removed local "ignore" checking

* Tue Jan 09 2001 Sourav K. Mandal <Sourav.Mandal@ikaran.com>
- Replaced string funcs with length-limited versions for security
- Replaced sketchy string ops with GLib GString routines
- Implemented Zephyr message decoder; currently only translates nulls and
  filters out non-ASCII characters and signatures.
- Revamped contact checker due to performance problems

* Mon Jan 08 2001 Sourav K. Mandal <Sourav.Mandal@ikaran.com>
- Contact info window implemented
- "Away" and "Hidden" handling improved
- Non-update of status window fixed with use of "buddy_log{in,off}"

* Sat Jan 06 2001 Sourav K. Mandal <Sourav.Mandal@ikaran.com>
- So-so working state
- Code cleanups

* Wed Jan 03 2001 Sourav K. Mandal <Sourav.Mandal@ikaran.com>
- First incarnation
 
*/

/*****************************************************************************/
/*** Begin conditional compile ***********************************************/
/*****************************************************************************/


/*****************************************************************************/
/*** Include files ***********************************************************/
/*****************************************************************************/

unsigned int module_version() {return CORE_VERSION;}

#include "intl.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <gtk/gtk.h>
#include <gtk/gtkmain.h>

#include <zephyr/zephyr.h>
#include "service.h"
#include "value_pair.h"
#include "util.h"
#include "message_parse.h"
#include "status.h"
#include "info_window.h"
#include "gtk_eb_html.h"
#include "plugin_api.h"
#include "smileys.h"

#include "zephyr_mod.h"

#include "pixmaps/zephyr_online.xpm"
#include "pixmaps/zephyr_hidden.xpm"
#include "pixmaps/zephyr_offline.xpm"

#define plugin_type zephyr_LTX_plugin_type
#define SERVICE_INFO zephyr_LTX_SERVICE_INFO
#define module_version zephyr_LTX_module_version

/*  Module Exports */
PLUGIN_TYPE plugin_type=PLUGIN_SERVICE;
struct service SERVICE_INFO = { "Zephyr", -1, FALSE, FALSE, FALSE, TRUE, NULL };
/* End Module Exports */

static char *eb_zephyr_get_color(void) { static char color[]="#ff4444"; return color; }


/*****************************************************************************/
/*** Constant values *********************************************************/
/*****************************************************************************/

enum {
  ZEPHYR_ONLINE,
  ZEPHYR_AWAY,
  ZEPHYR_HIDDEN,
  ZEPHYR_OFFLINE,
} ;

gchar *zephyr_status_strings[] = { 
  "", 
  "Away", 
  "Hidden", 
  "Offline", 
} ;


/* Enable/disable debugging messages */
#define ZEPHYR_DEBUG 0
/* #define ZEPHYR_DEBUG 1 */

/* For zwgc */
#define Z_DEFAULT_FORMAT \
"To: @b($recipient) at $date $time\nFrom: $1 <$sender>\n$2"

/* How often should we check for notices? */
#define ZEPHYR_DEF_NOT_CHK_INT 250L /* milliseconds */

/* How often should we check on contacts? */
#define ZEPHYR_DEF_CON_CHK_INT 20000L /* milliseconds */

/* Should we flush locs on log{in,off}? */
#define ZEPHYR_DEF_FLUSH_LOCS TRUE

/* Should we send authenticated location requests? */
#define ZEPHYR_DEF_AUTH_LOC_REQ TRUE

/* Should we send authenticated messages? */
#define ZEPHYR_DEF_AUTH_MSG_OUT TRUE

/* Should we check authenticity of incoming messages? */
#define ZEPHYR_DEF_AUTH_MSG_IN TRUE

/*****************************************************************************/
/*** Structures **************************************************************/
/*****************************************************************************/

typedef struct _eb_zephyr_account_data {
	gint status ;
} eb_zephyr_account_data ;

typedef struct _zephyr_info_data {
  gchar *name ;
  gint nlocs ;
  ZLocations_t *locs ;
} zephyr_info_data ;

typedef struct _eb_zephyr_local_account_data {
  gchar *screen_name ; /* useless */
  gchar *password ; /* useless */
  gint status ;
  gushort port ;
  gint not_chk_tag ;
  gint con_chk_tag ;
  gchar *away_msg ;
} eb_zephyr_local_account_data ;

/* Slavishly copied from "zephyr_err.c" */

struct error_table {
  char const * const * msgs ;
  long base ;
  int n_msgs ;
} ;

struct et_list {
  struct et_list *next ;
  const struct error_table *table ;
} ;

/*****************************************************************************/
/*** Global variables ********************************************************/
/*****************************************************************************/

/* Pixmap stuff */
static gboolean zephyr_pixmaps_flag = FALSE ;
static GdkPixmap *zephyr_pixmaps[ZEPHYR_OFFLINE + 1] ;
static GdkBitmap *zephyr_bitmaps[ZEPHYR_OFFLINE + 1] ;

/* Buddies */
static GList *zephyr_buddies = NULL ;

/* Prefs */
static input_list *zephyr_prefs = NULL ;
static gboolean zephyr_flush_locs = ZEPHYR_DEF_FLUSH_LOCS ;
static gboolean zephyr_auth_loc_req = ZEPHYR_DEF_AUTH_LOC_REQ ;
static gboolean zephyr_auth_msg_out = ZEPHYR_DEF_AUTH_MSG_OUT ;
static gboolean zephyr_auth_msg_in = ZEPHYR_DEF_AUTH_MSG_IN ;
static gchar zephyr_not_chk_int[256] ;
static gchar zephyr_con_chk_int[256] ;

/* From util.h */
extern GtkWidget *statuswindow ;

/*****************************************************************************/
/*** Macros ******************************************************************/
/*****************************************************************************/

/* Convenience macros for buddy account data */
#define ZEPHYR_STATUS \
        (((eb_zephyr_account_data *) \
        (account->protocol_account_data))->status)
#define ZEPHYR_IGNORE \
        (((eb_zephyr_account_data *) \
        (account->protocol_account_data))->ignore)

/* Convenience macros for local account data */
#define ZEPHYR_LOCAL_STATUS \
        (((eb_zephyr_local_account_data *) \
        (account->protocol_local_account_data))->status)
#define ZEPHYR_LOCAL_PORT \
        (((eb_zephyr_local_account_data *) \
        (account->protocol_local_account_data))->port)
#define ZEPHYR_LOCAL_NOT_TAG \
        (((eb_zephyr_local_account_data *) \
        (account->protocol_local_account_data))->not_chk_tag)
#define ZEPHYR_LOCAL_CON_TAG \
        (((eb_zephyr_local_account_data *) \
        (account->protocol_local_account_data))->con_chk_tag)
#define ZEPHYR_LOCAL_AWAY_MSG \
        (((eb_zephyr_local_account_data *) \
        (account->protocol_local_account_data))->away_msg)

/* Convenience macros for prefs */
#define ZEPHYR_NOT_CHK_INT (strtoul(zephyr_not_chk_int, NULL, 10))
#define ZEPHYR_CON_CHK_INT (strtoul(zephyr_con_chk_int, NULL, 10))
#ifdef HAVE_KRB4
#  define ZEPHYR_AUTH(flag) ((flag) ? (ZAUTH) : (ZNOAUTH))
#  define ZEPHYR_AUTH_CHK(flag) (flag)
#  define ZEPHYR_SENDER (ZGetSender())
#else
#  define ZEPHYR_AUTH(flag) (ZNOAUTH)
#  define ZEPHYR_AUTH_CHK(flag) (FALSE)
#  define ZEPHYR_SENDER (account->handle)
#endif

/* Use our own "com_err" function */
#define com_err zephyr_com_err

/*****************************************************************************/
/*** Function prototypes *****************************************************/
/*****************************************************************************/

/* For some reason, these prototypes are not in zephyr/zephyr.h */
Code_t ZUnsetLocation() ;
Code_t ZSetLocation(char *exposure) ;
Code_t ZGetLocations(ZLocations_t location[], int *numloc) ;

/* Steal this from status.c */
void update_status_message(gchar *message ) ;

/* Prototypes for interface related functions */
gboolean eb_zephyr_query_connected(eb_account *account) ;
void eb_zephyr_login(eb_local_account *account) ;
void eb_zephyr_logout(eb_local_account *account) ;
void eb_zephyr_send_im(eb_local_account *account_from, 
                       eb_account *account_to,
                       gchar *message) ;
eb_local_account *eb_zephyr_read_local_account_config(GList *values) ;
GList *eb_zephyr_write_local_config(eb_local_account *account) ;
eb_account *eb_zephyr_read_account_config(GList *config, 
                                          struct contact *contact) ;
GList *eb_zephyr_get_states(void) ;
gint eb_zephyr_get_current_state(eb_local_account *account) ;
void eb_zephyr_set_current_state(eb_local_account *account, gint state) ;
char * eb_zephyr_check_login(char * user, char * pass);
void eb_zephyr_add_user(eb_account *account) ;
void eb_zephyr_del_user(eb_account *account) ;
eb_account *eb_zephyr_new_account(gchar *account) ;
gchar *eb_zephyr_get_status_string(eb_account *account) ;
void eb_zephyr_get_status_pixmap(eb_account *account, GdkPixmap **pm, 
                                 GdkBitmap **bm) ;
void eb_zephyr_set_away(eb_local_account *account, gchar *message) ;
void eb_zephyr_get_info(eb_local_account *account_from, 
                        eb_account *account_to) ;
input_list *eb_zephyr_get_prefs(void) ;			
void eb_zephyr_read_prefs_config(GList *values) ;
GList *eb_zephyr_write_prefs_config(void) ;

/* Prototypes for internal utility functions */
void zephyr_init_prefs(void) ;
void zephyr_init_pixmaps(void) ;
gint zephyr_check_notices(gpointer data) ;
void zephyr_handle_notices(eb_local_account *account) ;
void zephyr_handle_control_notices(ZNotice_t notice, eb_account *account_to) ;
void zephyr_handle_message_notices(eb_local_account *account, 
                                   ZNotice_t notice,
                                   struct sockaddr_in from) ;
void zephyr_handle_location_notices(ZNotice_t notice) ;
gint zephyr_check_all_buddies(gpointer data) ;
void zephyr_reset_buddies(void) ;
void zephyr_locations_cb(eb_account *account, gboolean new_online) ;
void zephyr_get_info_data(gchar *name, zephyr_info_data *zid) ;
void zephyr_info_update(info_window *iw) ;
void zephyr_info_data_cleanup(info_window *iw) ;
gchar *zephyr_msg_decode(gchar *message, guint msg_siz) ;
gchar *zephyr_msg_encode(gchar *message) ;
void zephyr_com_err(gchar *whoami, glong status, gchar *desc) ;

/*****************************************************************************/
/*** Function definitions ****************************************************/
/*****************************************************************************/

void zephyr_com_err(gchar *whoami, glong status, gchar *desc) {

  extern struct et_list *_et_list ; /* from "zephyr_err.c */
  glong offset = status - ERROR_TABLE_BASE_zeph ;
  GString *msg = NULL ;
 
  if((offset < 0) || (offset >= _et_list->table->n_msgs)) {
	fprintf(stderr, "everybuddy: zephyr_com_err: Invalid msg offset %ld\n",
		    offset) ;
	return ;
  }

  msg = g_string_sized_new(256) ;
  msg = g_string_assign(msg, "") ;
  
  msg = g_string_append(msg, whoami) ;
  msg = g_string_append(msg, ": ") ;
  
  msg = g_string_append(msg, (*((_et_list->table->msgs) + offset))) ;
  msg = g_string_append(msg, " ") ;
  msg = g_string_append(msg, desc) ;
  msg = g_string_append(msg, "\n") ;
  
  fprintf(stderr, "%s", msg->str) ;
  fflush(stderr) ;
  
  g_string_free(msg, TRUE) ;
  
}

/*****************************************************************************/

struct service_callbacks *query_callbacks(void) {
	
  struct service_callbacks *sc ;

  sc = g_new0(struct service_callbacks, 1) ;
	
  sc->query_connected = eb_zephyr_query_connected ;
  sc->login = eb_zephyr_login ;
  sc->logout = eb_zephyr_logout ;
  sc->send_im = eb_zephyr_send_im ;
  sc->read_local_account_config = eb_zephyr_read_local_account_config ;
  sc->write_local_config = eb_zephyr_write_local_config ;
  sc->read_account_config = eb_zephyr_read_account_config ;
  sc->get_states = eb_zephyr_get_states ;
  sc->get_current_state = eb_zephyr_get_current_state ;
  sc->set_current_state = eb_zephyr_set_current_state ;
  sc->check_login = eb_zephyr_check_login;
  sc->add_user = eb_zephyr_add_user ;
  sc->del_user = eb_zephyr_del_user ;
  sc->new_account = eb_zephyr_new_account ;
  sc->get_status_string = eb_zephyr_get_status_string ;
  sc->get_status_pixmap = eb_zephyr_get_status_pixmap ;
  sc->set_idle = NULL;
  sc->set_away = eb_zephyr_set_away ;
  sc->send_chat_room_message =  NULL ;
  sc->join_chat_room = NULL ;
  sc->leave_chat_room = NULL ;
  sc->make_chat_room = NULL ;
  sc->send_invite = NULL ;
  sc->terminate_chat = NULL ;
  sc->get_info = eb_zephyr_get_info ;
  sc->get_prefs = eb_zephyr_get_prefs ;
  sc->read_prefs_config = eb_zephyr_read_prefs_config ;
  sc->write_prefs_config = eb_zephyr_write_prefs_config ;
  sc->add_importers = NULL;

  sc->get_color=eb_zephyr_get_color;
  sc->get_smileys=eb_default_smileys;
  
  zephyr_init_prefs() ;
	
  return sc ;
}

/*****************************************************************************/

void zephyr_init_prefs(void) {

  input_list *prefs = NULL ;
  
  /* Initialize Zephyr prefs */
  g_snprintf(zephyr_not_chk_int, 255, "%ld", ZEPHYR_DEF_NOT_CHK_INT) ;
  g_snprintf(zephyr_con_chk_int, 255, "%ld", ZEPHYR_DEF_CON_CHK_INT) ;
  /* Create list */
  zephyr_prefs = g_new0(input_list, 1) ;
  prefs = zephyr_prefs ;
  /* Flush locs */
  prefs->widget.checkbox.value = &zephyr_flush_locs ;
  prefs->widget.checkbox.name = "zephyr_flush_locs ";
  prefs->widget.checkbox.label = _("Flush pre-existing Zephyr locations") ;
  prefs->type = EB_INPUT_CHECKBOX ;
  /* Location request authentication */
  prefs->next = g_new0(input_list, 1) ;
  prefs = prefs->next ;
  prefs->widget.checkbox.value = &zephyr_auth_loc_req ;
  prefs->widget.checkbox.name = "zephyr_auth_loc_req ";
  prefs->widget.checkbox.label = _("Send authenticated buddy location requests") ;
  prefs->type = EB_INPUT_CHECKBOX ;
  /* Outgoing message authentication */
  prefs->next = g_new0(input_list, 1) ;
  prefs = prefs->next ;
  prefs->widget.checkbox.value = &zephyr_auth_msg_out ;
  prefs->widget.checkbox.name = "zephyr_auth_msg_out ";
  prefs->widget.checkbox.label = _("Send authenticated messages") ;
  prefs->type = EB_INPUT_CHECKBOX ;
  /* Incoming message authentication */
  prefs->next = g_new0(input_list, 1) ;
  prefs = prefs->next ;
  prefs->widget.checkbox.value = &zephyr_auth_msg_in ;
  prefs->widget.checkbox.name = "zephyr_auth_msg_in ";
  prefs->widget.checkbox.label = _("Authenticate incoming messages") ;
  prefs->type = EB_INPUT_CHECKBOX ;
 /* Notice check interval */
  prefs->next = g_new0(input_list, 1) ;
  prefs = prefs->next ;
  prefs->widget.entry.value = zephyr_not_chk_int ;
  prefs->widget.entry.name = "zephyr_not_chk_int ";
  prefs->widget.entry.label = _("Notice chk. int. (ms):") ;
  prefs->type = EB_INPUT_ENTRY ;
  /* Contact check interval */
  prefs->next = g_new0(input_list, 1) ;
  prefs = prefs->next ;
  prefs->widget.entry.value = zephyr_con_chk_int ;
  prefs->widget.entry.name = "zephyr_con_chk_int ";
  prefs->widget.entry.label = _("Buddy chk. int. (ms):") ;
  prefs->type = EB_INPUT_ENTRY ;

}

/*****************************************************************************/

gboolean eb_zephyr_query_connected(eb_account *account) {
  gboolean status = FALSE ;
  
  /* eb_zephyr_add_user already does redundancy checks */
  eb_zephyr_add_user(account) ;
  
  status = account->online && (ZEPHYR_STATUS != ZEPHYR_OFFLINE) ;
  
#if ZEPHYR_DEBUG
  fprintf(stderr, "DEBUG: eb_zephyr_query_connected: %s = %d\n",
          account->handle, status) ;
#endif
  
  return status ;
  
}

/*****************************************************************************/

void eb_zephyr_login(eb_local_account *account) {
  Code_t status ;
  ZSubscription_t sub ;
  

  /* Initialize library */
#if ZEPHYR_DEBUG
  fprintf(stderr, "DEBUG: eb_zephyr_login: Initializing Zephyr\n") ;
#endif
  status = ZInitialize() ;
  if(status != ZERR_NONE) {
    fprintf(stderr, 
            "everybuddy: eb_zephyr_login: Can't log in; got status %ld\n",
            (long int) status) ;
    return ;
  }

#if ZEPHYR_DEBUG
  com_err("DEBUG: com_err TEST", ZERR_PKTLEN, 
          "while TESTING com_err with status ZERR_PKTLEN") ;
  com_err("DEBUG: com_err TEST", ZERR_NOPORT, 
          "while TESTING com_err with status ZERR_NOPORT") ;
  com_err("DEBUG: com_err TEST", ZERR_SERVNAK, 
          "while TESTING com_err with status ZERR_SERVNAK") ;
  com_err("DEBUG: com_err TEST", ZERR_EOF, 
          "while TESTING com_err with status ZERR_EOF") ;
#endif
  
  /* Open a port */
#if ZEPHYR_DEBUG
  fprintf(stderr, "DEBUG: eb_zephyr_login: Opening Zephyr port\n") ;
#endif
  ZEPHYR_LOCAL_PORT = 0 ;
  status = ZOpenPort(&ZEPHYR_LOCAL_PORT) ;
  if(status != ZERR_NONE) {
    com_err("everybuddy: eb_zephyr_login", status,
            "while opening Zephyr port") ;
    return ;
  }
  
  /* Flush locations */
  if(zephyr_flush_locs) {
#if ZEPHYR_DEBUG
    fprintf(stderr, "DEBUG: eb_zephyr_login: Flushing Zephyr locations\n") ;
#endif
    status = ZFlushMyLocations() ;
    if(status != ZERR_NONE)
      com_err("everybuddy: eb_zephyr_login", status,
              "while flushing pre-existing Zephyr locations") ;

  }


  /* Register with Zephyr servers */
#if ZEPHYR_DEBUG
  fprintf(stderr, "DEBUG: eb_zephyr_login: Setting Zephyr location\n") ;
#endif
  status = ZSetLocation(EXPOSE_REALMVIS) ;
  if(status != ZERR_NONE) {
    com_err("everybuddy: eb_zephyr_login", status,
            "while registering with Zephyr servers") ;
    return ;
  }
  
  /* Flush existing subscriptions */
#if ZEPHYR_DEBUG
  fprintf(stderr, "DEBUG: eb_zephyr_login: Cancelling Zephyr subscriptions\n") ;
#endif
  status = ZCancelSubscriptions(0) ;
  if(status != ZERR_NONE) {
    com_err("everybuddy: eb_zephyr_login", status, 
            "while canceling subscriptions") ;
    return ;
  }
  
  /* Subscribe to personal messages */
#if ZEPHYR_DEBUG
  fprintf(stderr, "DEBUG: eb_zephyr_login: Setting Zephyr subscriptions\n") ;
#endif
  sub.zsub_class = "MESSAGE" ;
  sub.zsub_classinst = "PERSONAL" ;
  sub.zsub_recipient = ZEPHYR_SENDER ;
  status = ZSubscribeToSansDefaults(&sub, 1, 0) ;
  if(status != ZERR_NONE) {
    com_err("everybuddy: eb_zephyr_login", status,
            "while subscribing") ;
    return ;
  }
  
  /* Set up periodic message checking in gtk_main.
     If performance is too sluggish, this functionality maybe
     placed in a separate thread. */
#if ZEPHYR_DEBUG
  fprintf(stderr, "DEBUG: eb_zephyr_login: Adding notice checker\n") ;
#endif
  ZEPHYR_LOCAL_NOT_TAG = 0 ;
  ZEPHYR_LOCAL_NOT_TAG = eb_timeout_add(ZEPHYR_NOT_CHK_INT,
                                         zephyr_check_notices,
                                         account) ;

  /* Set up periodic contact checking in gtk_main.
     If performance is too sluggish, this functionality maybe
     placed in a separate thread. */
#if ZEPHYR_DEBUG
  fprintf(stderr, "DEBUG: eb_zephyr_login: Adding contact checker\n") ;
#endif
  ZEPHYR_LOCAL_CON_TAG = 0 ;
  ZEPHYR_LOCAL_CON_TAG = eb_timeout_add(ZEPHYR_CON_CHK_INT,
                                         zephyr_check_all_buddies,
                                         account) ;

  /* If everything works ... */
#if ZEPHYR_DEBUG
  fprintf(stderr, "DEBUG: eb_zephyr_login: Indicating online status to EB\n") ;
#endif
  account->connected = TRUE ;
  ZEPHYR_LOCAL_STATUS = ZEPHYR_ONLINE ;
  if(account->status_menu) {
    GTK_CHECK_MENU_ITEM(g_slist_nth(account->status_menu, 
                          ZEPHYR_OFFLINE)->data)->active = 0 ;
    GTK_CHECK_MENU_ITEM(g_slist_nth(account->status_menu, 
                          ZEPHYR_ONLINE)->data)->active = 1 ;
  }
  
  /* Check contacts right off */
  zephyr_check_all_buddies(account) ;

}

/*****************************************************************************/

void eb_zephyr_logout(eb_local_account *account) {
  Code_t status ;
  
  /* Show all Zephyr accounts as offline */
  zephyr_reset_buddies() ;
 
  /* Setting account to offline */
#if ZEPHYR_DEBUG
  fprintf(stderr, "DEBUG: eb_zephyr_logout: Indicating offline to EB\n") ;
#endif
  ZEPHYR_LOCAL_STATUS = ZEPHYR_OFFLINE ;
  account->connected = FALSE ;
  if(account->status_menu) {
    GTK_CHECK_MENU_ITEM(g_slist_nth(account->status_menu, 
                          ZEPHYR_ONLINE)->data)->active = 0 ;
    GTK_CHECK_MENU_ITEM(g_slist_nth(account->status_menu, 
                          ZEPHYR_OFFLINE)->data)->active = 1 ;
    
  }
  
  /* Stop contact checking */
#if ZEPHYR_DEBUG
  fprintf(stderr, "DEBUG: eb_zephyr_logout: Removing contact checker\n") ;
#endif
  if(ZEPHYR_LOCAL_CON_TAG) eb_timeout_remove(ZEPHYR_LOCAL_CON_TAG) ;
  
  /* Stop notice checking */
#if ZEPHYR_DEBUG
  fprintf(stderr, "DEBUG: eb_zephyr_logout: Removing notice checker\n") ;
#endif
  if(ZEPHYR_LOCAL_NOT_TAG) eb_timeout_remove(ZEPHYR_LOCAL_NOT_TAG) ;
  
  /* Cancel subscriptions */
#if ZEPHYR_DEBUG
  fprintf(stderr, "DEBUG: eb_zephyr_logout: Cancelling subscriptions\n") ;
#endif
  status = ZCancelSubscriptions(0) ;
  if(status != ZERR_NONE) {
    com_err("everybuddy: eb_zephyr_logout", status,
            "while canceling subscriptions") ;
    return ;
  }
  
  /* Deregister with Zephyr servers */
#if ZEPHYR_DEBUG
  fprintf(stderr, "DEBUG: eb_zephyr_logout: Unsetting location\n") ;
#endif
  status = ZUnsetLocation() ;
  if(status != ZERR_NONE) {
    com_err("everybuddy: eb_zephyr_logout", status,
            "while deregistering with Zephyr servers") ;
    return ;
  }
  
  /* Flush locations */
  if(zephyr_flush_locs) {
#if ZEPHYR_DEBUG
    fprintf(stderr, "DEBUG: eb_zephyr_logout: Flushing Zephyr locations\n") ;
#endif
    status = ZFlushMyLocations() ;
    if(status != ZERR_NONE)
      com_err("everybuddy: eb_zephyr_logout", status,
              "while flushing locations") ;
  }
  
  /* Closing Zephyr network port */
#if ZEPHYR_DEBUG
  fprintf(stderr, "DEBUG: eb_zephyr_logout: Closing Zephyr port\n") ;
#endif
  status = ZClosePort() ;
  if(status != ZERR_NONE) {
    com_err("everybuddy: eb_zephyr_login", status,
            "while closing Zephyr port") ;
    return ;
  }
  
}

/*****************************************************************************/

void eb_zephyr_send_im(eb_local_account *account_from, 
                       eb_account *account_to,
			                 gchar *message) {
  Code_t status ;
  ZNotice_t notice, retnotice ;
  gchar *nmsg = NULL ;
  guint msg_siz = 0 ;
  
  /* Zero the notice before writing data to it */
  bzero((char *) &notice, sizeof(notice)) ;
  
  /* Encode message */
  nmsg = zephyr_msg_encode(message) ;
  msg_siz = strlen(nmsg) + 1 ;
  
  /* Compose notice */
#if ZEPHYR_DEBUG
  fprintf(stderr, "DEBUG: eb_zephyr_send_im: Composing Zephyrgram\n") ;
#endif
  notice.z_kind = ACKED ;
  notice.z_port = 0 ; /* Library will fill in */
  notice.z_class = "MESSAGE" ;
  notice.z_class_inst = "PERSONAL" ;
  notice.z_opcode = "" ;
  notice.z_sender = (char *) NULL ; /* Library will fill in */
  notice.z_recipient = account_to->handle ;
  notice.z_default_format = Z_DEFAULT_FORMAT ;
  notice.z_message = nmsg ;
  notice.z_message_len = msg_siz ;
  
  /* Send notice */
#if ZEPHYR_DEBUG
  fprintf(stderr, "DEBUG: eb_zephyr_send_im: Sending Zephyrgram\n") ;
#endif
  status = ZSendNotice(&notice, ZEPHYR_AUTH(zephyr_auth_msg_out)) ;
  if(status != ZERR_NONE)
    com_err("everybuddy: eb_zephyr_send_im", status,
            "while sending notice") ;
            
  
  status = ZIfNotice(&retnotice, (struct sockaddr_in *) NULL, 
                     ZCompareUIDPred, (char *) (&(notice.z_uid))) ;
  if(status != ZERR_NONE) {
    com_err("everybuddy: eb_zephyr_send_im", status,
            "while receiving acknowledgement") ;
    ZFreeNotice(&retnotice) ;
  }
  else
    zephyr_handle_control_notices(retnotice, account_to) ;
           
  ZFreeNotice(&notice) ;
} 


/*****************************************************************************/

gchar *zephyr_msg_encode(gchar *message) {
  gchar *nmsg = NULL ;
  guint msg_siz = 0 ;
  
  if(!message) return NULL ;
  
  /* Prepend newline for compatibility w/ other Zephyr clients */
  msg_siz = strlen(message) + 2 ;
  nmsg = (gchar *) calloc(msg_siz, 1) ;
  strncpy(nmsg, "\n", msg_siz) ;
  strncat(nmsg, message, msg_siz - 1) ;
  
  return nmsg ;
  
}

/*****************************************************************************/

eb_local_account *eb_zephyr_read_local_account_config(GList *values) {
  gchar *buf;
  eb_local_account *ela = g_new0(eb_local_account, 1) ;
  eb_zephyr_local_account_data *zla = g_new0(eb_zephyr_local_account_data, 1) ;

#if ZEPHYR_DEBUG
  fprintf(stderr, "DEBUG: eb_zephyr_read_local_account_config: \n") ;
#endif
#ifdef HAVE_KRB4
  buf = ZGetSender() ;
  ela->handle = strdup(buf) ;
#else
  ela->handle = value_pair_get_value(values, "SCREEN_NAME") ;
#endif
  strncpy(ela->alias, ela->handle, 255) ;
  /* g_free(buf) ; */  /* Error!  What am I not understanding? */
  zla->password = strdup("") ;
  zla->status = ZEPHYR_OFFLINE ;
  zla->port = 0 ;
  zla->not_chk_tag = 0 ;
  zla->con_chk_tag = 0 ;
  zla->away_msg = NULL ;
  
  ela->service_id = SERVICE_INFO.protocol_id ;
  ela->protocol_local_account_data = zla ;
  
  return ela ;
}

/*****************************************************************************/

GList *eb_zephyr_write_local_config(eb_local_account *account) {
  GList *config = NULL ;
  value_pair *pair = NULL ;

#if ZEPHYR_DEBUG
  fprintf(stderr, "DEBUG: eb_zephyr_write_local_config: \n") ;
#endif

  pair = g_new0(value_pair, 1) ;
  g_snprintf(pair->key, 255, "SCREEN_NAME") ;
  g_snprintf(pair->value, 255, "%s", ZEPHYR_SENDER) ;
  config = g_list_append(config, pair) ;

  /* Is this thing necessary? */
  pair = g_new0(value_pair, 1) ;
  g_snprintf(pair->key, 255, "PASSWORD") ;
  pair->value[0] = '\0' ;
  config = g_list_append(config, pair) ;

  return config ;
}

/*****************************************************************************/

eb_account *eb_zephyr_read_account_config(GList *config, 
                                          struct contact *contact) {
  eb_account *ea = g_new0(eb_account, 1) ;
  eb_zephyr_account_data *za = g_new(eb_zephyr_account_data, 1) ;
  
  za->status = ZEPHYR_OFFLINE ;
  
  strncpy(ea->handle, value_pair_get_value(config, "NAME"), 255) ;
#if ZEPHYR_DEBUG
  fprintf(stderr, 
          "DEBUG: eb_zephyr_read_account_config: Got account \"%s\"\n", 
          ea->handle) ;
#endif
  ea->service_id = SERVICE_INFO.protocol_id ;
  ea->list_item = NULL ;
  ea->online = FALSE ;
  ea->status = NULL ;
  ea->pix = NULL ;
  ea->icon_handler = -1 ;
  ea->status_handler = -1 ;
  
  ea->account_contact = contact ;
  ea->protocol_account_data = za;
  
  eb_zephyr_add_user(ea) ;
  
  return ea ;
}

/*****************************************************************************/

GList *eb_zephyr_get_states(void) {
  GList *states = NULL ;
  
#if ZEPHYR_DEBUG
  fprintf(stderr, "DEBUG: eb_zephyr_get_states: \n") ;
#endif
  states = g_list_append(states, "Online") ;
  states = g_list_append(states, "Away") ;
  states = g_list_append(states, "Hidden") ;
  states = g_list_append(states, "Offline") ;
  
  return states ;
}

char * eb_zephyr_check_login(char * user, char * pass)
{
	return NULL;
}


/*****************************************************************************/

gint eb_zephyr_get_current_state(eb_local_account *account) {
#if ZEPHYR_DEBUG
  fprintf(stderr, "DEBUG: eb_zephyr_get_current_state: State = %d\n",
          ZEPHYR_LOCAL_STATUS) ;
#endif
  return ZEPHYR_LOCAL_STATUS ;
}

/*****************************************************************************/

void eb_zephyr_set_current_state(eb_local_account *account, gint state) {
  Code_t status ;
  gint cur_state = eb_zephyr_get_current_state(account) ;
  
#if ZEPHYR_DEBUG
  fprintf(stderr, "DEBUG: eb_zephyr_set_current_state: From %d to %d\n", 
          cur_state, state) ;
#endif

  switch(state) {
    
    case ZEPHYR_ONLINE :
    case ZEPHYR_AWAY :
      if(cur_state == ZEPHYR_OFFLINE)
        eb_zephyr_login(account) ;
      else if(cur_state == ZEPHYR_HIDDEN) {
        status = ZSetVariable("exposure", EXPOSE_REALMVIS) ;
        if(status != ZERR_NONE) {
          com_err("everybuddy: eb_zephyr_set_current_state", status,
                  "while setting \"exposure\" to EXPOSE_REALMVIS") ;
          break ;
        }
        status = ZSetLocation(EXPOSE_REALMVIS) ;
        if(status != ZERR_NONE) {
          com_err("everybuddy: eb_zephyr_set_current_state", status,
                  "while notifying server of exposure EXPOSE_REALMVIS") ;
          break ;
        }
      }
      ZEPHYR_LOCAL_STATUS = state ;
      break ;
      
    case ZEPHYR_HIDDEN : 
      if(cur_state == ZEPHYR_OFFLINE) 
        eb_zephyr_login(account) ;
      status = ZSetVariable("exposure", EXPOSE_OPSTAFF) ;
      if(status != ZERR_NONE) {
          com_err("everybuddy: eb_zephyr_set_current_state", status,
                  "while setting \"exposure\" to EXPOSE_OPSTAFF") ;
        break ;
      }
      status = ZSetLocation(EXPOSE_OPSTAFF) ;
      if(status != ZERR_NONE) {
          com_err("everybuddy: eb_zephyr_set_current_state", status,
                  "while notifying server of exposure EXPOSE_OPSTAFF") ;
        break ;
      }
      ZEPHYR_LOCAL_STATUS = state ;
      break ;
    
    case ZEPHYR_OFFLINE : 
      if(cur_state != ZEPHYR_OFFLINE)
        eb_zephyr_logout(account) ;
      ZEPHYR_LOCAL_STATUS = state ;
      break ;
    
    default :
      fprintf(stderr,
              "everybuddy: eb_zephyr_set_current_state: Unknown state %d\n",
              state) ;
      break ;
  }
}

/*****************************************************************************/

void eb_zephyr_add_user(eb_account *account) {
#if ZEPHYR_DEBUG
  fprintf(stderr, "DEBUG: eb_zephyr_add_user: Adding user %s\n", 
          account->handle) ;
#endif
  if(!g_list_find(zephyr_buddies, account->handle))
    zephyr_buddies = g_list_append(zephyr_buddies, 
                                    account->handle) ;
}

/*****************************************************************************/

void eb_zephyr_del_user(eb_account *account) {
#if ZEPHYR_DEBUG
  fprintf(stderr, "DEBUG: eb_zephyr_del_user: Deleting user %s\n", 
          account->handle) ;
#endif
  g_list_remove(zephyr_buddies, account->handle) ;
}

/*****************************************************************************/

eb_account *eb_zephyr_new_account(gchar *account) {
  eb_account *ea = g_new0(eb_account, 1) ;
  eb_zephyr_account_data *za = g_new0(eb_zephyr_account_data, 1) ;
  
#if ZEPHYR_DEBUG
  fprintf(stderr, "DEBUG: eb_zephyr_new_account: User \"%s\"\n", 
          account) ;
#endif

  strncpy(ea->handle, account, 255) ;
  ea->service_id = SERVICE_INFO.protocol_id ;
  
  za->status = ZEPHYR_OFFLINE ;
  
  ea->protocol_account_data = za ;
  
  return ea ;
  
}

/*****************************************************************************/

gchar *eb_zephyr_get_status_string(eb_account *account) {
#if ZEPHYR_DEBUG
  fprintf(stderr, 
          "DEBUG: eb_zephyr_get_status_string: For user %s = %d, \"%s\"\n", 
          account->handle, ZEPHYR_STATUS, 
          zephyr_status_strings[ZEPHYR_STATUS]) ;
#endif
  return zephyr_status_strings[ZEPHYR_STATUS] ;
}

/*****************************************************************************/

void zephyr_init_pixmaps(void) {
  gint i ;
  gchar **xpm ;

  for(i = ZEPHYR_ONLINE ; i <= ZEPHYR_OFFLINE ; i++) {
#if ZEPHYR_DEBUG
  fprintf(stderr, "DEBUG: zephyr_init_pixmaps: i = %d\n", i) ;
#endif
    switch(i) {
      case ZEPHYR_ONLINE :
        xpm = zephyr_online_xpm ;
        break;
	  case ZEPHYR_HIDDEN :
        xpm = zephyr_hidden_xpm ;
        break;
      default:
        xpm = zephyr_offline_xpm ;
        break ;   
    }     
    zephyr_pixmaps[i] = gdk_pixmap_create_from_xpm_d(statuswindow->window,
                                                     &zephyr_bitmaps[i],
                                                     NULL, xpm) ;
  }
  
  zephyr_pixmaps_flag = TRUE ;
}

/*****************************************************************************/

void eb_zephyr_get_status_pixmap(eb_account *account, GdkPixmap **pm, 
                                 GdkBitmap **bm) {
#if ZEPHYR_DEBUG
  fprintf(stderr, "DEBUG: zephyr_get_status_pixmaps: For user %s = %d\n",
          account->handle, ZEPHYR_STATUS) ;
#endif

  if(!zephyr_pixmaps_flag) zephyr_init_pixmaps() ;
  
  *pm = zephyr_pixmaps[ZEPHYR_STATUS] ;
  *bm = zephyr_bitmaps[ZEPHYR_STATUS] ;
#if ZEPHYR_DEBUG
  fprintf(stderr, "DEBUG: zephyr_get_status_pixmaps: pm = %d, bm = %d\n",
          (int) pm, (int) bm) ;
#endif
}

/*****************************************************************************/

void eb_zephyr_set_away(eb_local_account *account, gchar *message) {

#if ZEPHYR_DEBUG
  fprintf(stderr, "DEBUG: eb_zephyr_set_away:\n") ;
#endif

  if(message) {
	ZEPHYR_LOCAL_AWAY_MSG = message ;
    if(account->status_menu) {
      gtk_check_menu_item_set_active(
        GTK_CHECK_MENU_ITEM(g_slist_nth(account->status_menu,
                            ZEPHYR_AWAY)->data), 
        TRUE
      ) ;
    }
  }
  else {
    if(account->status_menu) {
      gtk_check_menu_item_set_active(
        GTK_CHECK_MENU_ITEM(g_slist_nth(account->status_menu, 
                            ZEPHYR_ONLINE)->data), 
        TRUE
      ) ;
	}
  }
}

/*****************************************************************************/

void eb_zephyr_get_info(eb_local_account *account_from, 
                        eb_account *account_to) {
#if ZEPHYR_DEBUG
  fprintf(stderr, "DEBUG: eb_zephyr_get_info:  For user %s\n",
          account_to->handle) ;
#endif
  
  if(!account_to->infowindow) {
    account_to->infowindow = eb_info_window_new(account_from, account_to) ;
    account_to->infowindow->cleanup = zephyr_info_data_cleanup ;
    gtk_widget_show(account_to->infowindow->window) ;
  }
  
  if(account_to->infowindow->info_type == -1
     || account_to->infowindow->info_data == NULL) {
    if(account_to->infowindow->info_data == NULL) {
      account_to->infowindow->info_data = g_new0(zephyr_info_data, 1) ; 
      account_to->infowindow->cleanup = zephyr_info_data_cleanup ;
    }
    account_to->infowindow->info_type = SERVICE_INFO.protocol_id ;
  }

  zephyr_get_info_data(account_to->handle, 
                       (zephyr_info_data *) 
                       account_to->infowindow->info_data) ;
  zephyr_info_update(account_to->infowindow) ;
}

/*****************************************************************************/

void zephyr_get_info_data(gchar *name, zephyr_info_data *zid) {
  gint i ;
  Code_t status ;
  
  zid->name = strdup(name) ;
  zid->nlocs = 0 ;
  
  /* Get location info */
  status = ZLocateUser(zid->name, &(zid->nlocs), 
                       ZEPHYR_AUTH(zephyr_auth_loc_req)) ;
  if(status != ZERR_NONE)
    com_err("everybuddy: zephyr_get_info_data", status,
            "while requesting locations") ;
   
  zid->locs = g_new0(ZLocations_t, zid->nlocs) ;
  status = ZGetLocations(zid->locs, &(zid->nlocs)) ;
  if(status != ZERR_NONE) {
     com_err("everybuddy: zephyr_get_info_data", status,
             "while getting locations") ;
     return ;
  }
  
  /* Make duplicate of info before flushing */
  for( i = 0 ; i < zid->nlocs ; i++ ) {
    zid->locs[i].host = strdup(zid->locs[i].host) ;
    zid->locs[i].tty = strdup(zid->locs[i].tty) ;
    zid->locs[i].time = strdup(zid->locs[i].time) ;
  }
  
  /* Flush collected info */
  status = ZFlushLocations() ;
  if(status != ZERR_NONE)
    com_err("everybuddy: zephyr_get_info_data", status,
            "while flushing location results") ;
         
}

/*****************************************************************************/

void zephyr_info_update(info_window *iw) {
  gchar buf[512] ;
  guint blen = 511 ;
  gint i = 0 ;
  GString *msg = NULL ;
  zephyr_info_data *zid = (zephyr_info_data *) iw->info_data ;
  
  clear_info_window(iw) ;

  msg = g_string_new(_("[<B>TTY</B>]@<B>Host</B> (<B>Time</B>)<HR>")) ;
  
  for(i = 0 ; i < zid->nlocs ; i++) {
#if ZEPHYR_DEBUG
	fprintf(stderr, "DEBUG: zephyr_info_update: [%s]@%s (%s)\n",
		    zid->locs[i].tty, zid->locs[i].host, zid->locs[i].time) ;
#endif
	g_snprintf(buf, blen, "[%s]@%s (%s)<HR>", 
               zid->locs[i].tty, zid->locs[i].host, zid->locs[i].time) ;
	msg = g_string_append(msg, buf) ;
  }
  gtk_eb_html_add(GTK_SCTEXT(iw->info), msg->str, 0, 0, 0) ;
  g_string_free(msg, TRUE) ;
  
  gtk_adjustment_set_value(
    gtk_scrolled_window_get_vadjustment(
	  GTK_SCROLLED_WINDOW(iw->scrollwindow)
	), 0) ;

}

/*****************************************************************************/

void zephyr_info_data_cleanup(info_window *iw) {
  
  gint i = 0 ;
  zephyr_info_data *zid = (zephyr_info_data *) iw->info_data ;
  
#if ZEPHYR_DEBUG
  fprintf(stderr, "DEBUG: zephyr_info_data_cleanup\n") ;
#endif

  if(zid->name) g_free(zid->name) ;
  
  /* Clean up duplicate info */
  for(i = 0 ; i < zid->nlocs ; i++) {
    if(zid->locs[i].host) g_free(zid->locs[i].host) ;
    if(zid->locs[i].tty) g_free(zid->locs[i].tty) ;
    if(zid->locs[i].time) g_free(zid->locs[i].time) ;
  }
  
}

/*****************************************************************************/

gint zephyr_check_notices(gpointer data) {
  gint qlen = 0 ;

  qlen = ZPending() ;
  if(qlen > 0) {
#if ZEPHYR_DEBUG
    fprintf(stderr, "DEBUG: zephyr_check_notices: qlen = %d\n", qlen) ;
#endif
    zephyr_handle_notices(((eb_local_account *) data)) ;
  }
  else if(qlen == -1)
    fprintf(stderr, "everybuddy: zephyr_check_notices: ZPending: %s\n",
            strerror(errno)) ;
   
  return TRUE ;
}

/*****************************************************************************/

void zephyr_handle_notices(eb_local_account *account) {
  ZNotice_t notice ;
  Code_t status ;
  struct sockaddr_in from ;
    
  while(ZQLength() != 0) {
    
    status = ZReceiveNotice(&notice, &from) ;
    if(status != ZERR_NONE) {
      com_err("everybuddy: zephyr_handle_notices", status,
              "while taking message from queue") ;
      ZFreeNotice(&notice) ;
      continue ;
    }
    
#if ZEPHYR_DEBUG
  fprintf(stderr, 
          "DEBUG: zephyr_handle_notices: Notice from %s, length %d, kind %d\n",
          notice.z_sender, notice.z_message_len, notice.z_kind) ;
#endif
    
    /* Sort notices */
    
    /* Control */
    if(notice.z_kind > ACKED) /* Non-robust conditional? */
      zephyr_handle_control_notices(notice, NULL) ;
    
    /* Messages */
    else if((!strcasecmp(notice.z_class, "MESSAGE")) 
             && (notice.z_message_len > 2)) 
	  zephyr_handle_message_notices(account, notice, from) ;

    /* Zero length messages, like zwrite pings */
    else if(!strcasecmp(notice.z_class, "MESSAGE"))
      ZFreeNotice(&notice) ;
    
   /* Locations */
    else if((!strcasecmp(notice.z_class, "USER_LOCATE")) 
             && (!strcasecmp(notice.z_opcode, "LOCATE")))
      zephyr_handle_location_notices(notice) ;
    
    /* Unknown */
    else {
      fprintf(stderr, 
            "everybuddy: zephyr_handle_notices: Unknown notice type\n") ;
      ZFreeNotice(&notice) ;
    }

  } /* end loop */
}

/*****************************************************************************/

void zephyr_handle_message_notices(eb_local_account *account, 
                                   ZNotice_t notice,
                                   struct sockaddr_in from) {
  
  eb_account *account_r ;
  GString *message = NULL ;
  gchar *z_message = NULL ;
  Code_t status = 0 ;
  gint nlocs = 0 ;
  
  account_r = find_account_by_handle(notice.z_sender, SERVICE_INFO.protocol_id) ;
    
  /* Handle unknown contact */
  if(!account_r) {
    account_r = eb_zephyr_new_account(notice.z_sender) ;
    add_unknown(account_r) ;
    update_contact_list (); /* This should be inside add_unknown ! */
    status = ZLocateUser(notice.z_sender, &nlocs, 
                         ZEPHYR_AUTH(zephyr_auth_loc_req)) ;
    if(status != ZERR_NONE)
      com_err("everybuddy: zephyr_handle_message_notices", status,
              "while requesting locations") ;
    status = ZFlushLocations() ;
    if(status != ZERR_NONE)
      com_err("everybuddy: zephyr_handle_message_notices", status,
              "while flushing location results") ;
    if(nlocs) {
      ((eb_zephyr_account_data *) (account_r->protocol_account_data))->status =
        ZEPHYR_ONLINE ;
      buddy_login(account_r) ;
    }
  }
  
  /* denote hidden -- handles known and unknown users */
  if(!(account_r->online)) {
    ((eb_zephyr_account_data *) (account_r->protocol_account_data))->status =
      ZEPHYR_HIDDEN ;
    buddy_login(account_r) ;
  }
  
  message = g_string_sized_new(1024) ;
  message = g_string_assign(message, "") ;
  if(ZEPHYR_AUTH_CHK(zephyr_auth_msg_in)) {
    status = ZCheckAuthentication(&notice, &from) ;
    switch(status) {
      case ZAUTH_NO :
        message = g_string_append(message, 
                                  _("<HR><B>UNAUTHENTIC</B><BR><HR>")) ;
        break ;
      case ZAUTH_FAILED :
        message = g_string_append(message,
                                  _("<HR><B>FORGED?</B><BR><HR>")) ;
        break ;
      default :
        break ;
    }
  }
    
  z_message = zephyr_msg_decode(notice.z_message, notice.z_message_len) ;
  if(!z_message) {
    ZFreeNotice(&notice) ;
    return ;
  }
  
  message = g_string_append(message, z_message) ;
  g_free(z_message) ;
  z_message = strdup(message->str) ;
  g_string_free(message, TRUE) ;
 
#if ZEPHYR_DEBUG
  fprintf(stderr, "DEBUG: zephyr_handle_message_notices: %s\n",
          z_message) ;
#endif
  ZFreeNotice(&notice) ;
  eb_parse_incoming_message(account, account_r, z_message) ;

  if((ZEPHYR_LOCAL_STATUS == ZEPHYR_AWAY) 
     && ZEPHYR_LOCAL_AWAY_MSG
     /* To prevent infinite looping */
     && strcmp(account_r->handle, account->handle)) {
#if ZEPHYR_DEBUG
    fprintf(
      stderr, 
     "DEBUG: zephyr_handle_message_notices: Sending away message to %s\n", 
      notice.z_sender
    ) ;
#endif
    eb_zephyr_send_im(account, account_r, ZEPHYR_LOCAL_AWAY_MSG) ;
  }
  
}

/*****************************************************************************/

gchar *zephyr_msg_decode(gchar *message, guint msg_siz) {
  guint i = 0 ;
  GString *nmsg = NULL ; 
  gchar *nmsg_ptr = NULL ;
  
#if ZEPHYR_DEBUG
  fprintf(stderr, "DEBUG: zephyr_msg_decode: %s\n",
          message) ;
#endif

  if((!message) || (msg_siz < 3)) return NULL ;
  
  nmsg = g_string_sized_new(1024) ;
  nmsg = g_string_assign(nmsg, "") ;
  
  for(i = 0 ; i < msg_siz ; i++) {
    if(message[i] == '\0')
      nmsg = g_string_append_c(nmsg, '\n') ;
    else if(isascii(message[i]))
      nmsg = g_string_append_c(nmsg, message[i]) ;
  }
    
  /* Clip trailing newline */
  i = strlen(nmsg->str) - 1 ;
  nmsg->str[i] = '\0' ;
  
  /* To discard signature headings to messages: */
  /* First, find first newline, and discard stuff before that. */
  i = strcspn(nmsg->str, "\n") ;
  if(i) nmsg = g_string_erase(nmsg, 0, i) ;
  
  /* Then, remove clump of newlines at beginning. */
  i = strspn(nmsg->str, "\n") ;
  if(i) nmsg = g_string_erase(nmsg, 0, i) ;
  
  nmsg_ptr = strdup(nmsg->str) ;
  g_string_free(nmsg, TRUE) ;
  return(nmsg_ptr) ;

}

/*****************************************************************************/

void zephyr_handle_control_notices(ZNotice_t notice, eb_account *account) {
  gchar buf[256] ;
  
#if ZEPHYR_DEBUG
  fprintf(stderr, "DEBUG: zephyr_handle_control_notices: z_kind = %d\n",
          notice.z_kind) ;
#endif
  switch(notice.z_kind) {
    case HMACK : 
      update_status_message("[Zephyr] HM OK") ;
      break ;
    case SERVACK : 
      strncpy(buf, "[Zephyr] Server: ", 256) ;
      strncat(buf, notice.z_message, 256) ;
      update_status_message(buf) ;
      /* Handle hidden contact logging off, the bastard */
      if(account) {
        if(account->online 
           && (!strcmp(notice.z_message, ZSRVACK_NOTSENT))) {
          ZEPHYR_STATUS = ZEPHYR_OFFLINE ;
          buddy_logoff(account) ;
        }
      }
      break ;
    case SERVNAK : 
      update_status_message(_("[Zephyr] Server error!")) ;
      break ;
    case CLIENTACK : 
      update_status_message(_("[Zephyr] Client OK")) ;
      break ;
    default :
      break ;
  }
  
  ZFreeNotice(&notice) ;
}

/*****************************************************************************/

void zephyr_handle_location_notices(ZNotice_t notice) {
  
  Code_t status = ZERR_NONE ;
  guint nlocs = 0 ;
  eb_account *account = NULL ;
  gchar *user = NULL ;
  
#if ZEPHYR_DEBUG
  fprintf(stderr, "DEBUG: zephyr_handle_location_notices: User %s\n",
          notice.z_sender) ;
#endif
  
  status = ZParseLocations(&notice, (ZAsyncLocateData_t *) NULL, 
                           &nlocs, &user) ;
  ZFreeNotice(&notice) ;
  if(status != ZERR_NONE) {
    com_err("everybuddy: zephyr_handle_location_notices", status,
            "while parsing locations") ;
    return ;
  }
  
  status = ZFlushLocations() ;
  if(status != ZERR_NONE)
    com_err("everybuddy: zephyr_handle_location_notices", status,
            "while flushing locations info") ;
  
  account = find_account_by_handle(user, SERVICE_INFO.protocol_id) ;
  if(user) g_free(user) ;
  
  if(!account) return ;
  
  if(nlocs == -1) return ;
  
  if(nlocs > 0)
    zephyr_locations_cb(account, TRUE) ;
  else
    zephyr_locations_cb(account, FALSE) ;
  
}

/*****************************************************************************/

void zephyr_locations_cb(eb_account *account, gboolean new_online) {

  gboolean old_online = FALSE ;
  
#if ZEPHYR_DEBUG
  fprintf(stderr, "DEBUG: zephyr_locations_cb: Checking user %s = %d\n",
          account->handle, new_online) ;
#endif
  
  old_online = account->online ;
  if(new_online) {
    ZEPHYR_STATUS = ZEPHYR_ONLINE ;
    if(new_online != old_online) buddy_login(account) ;
  }
  else if(ZEPHYR_STATUS != ZEPHYR_HIDDEN) {
    ZEPHYR_STATUS = ZEPHYR_OFFLINE ;
    if(new_online != old_online) buddy_logoff(account) ;
  }
  /* buddy_update_status(account) ; */
  
}

/*****************************************************************************/

gint zephyr_check_all_buddies(gpointer data) {
  GList *cur = NULL ;
  Code_t status = 0 ;
  gchar *handle = NULL ;
  ZAsyncLocateData_t ald ;
  
  ald.user = NULL ;
  memset(&(ald.uid), 0, sizeof(ZUnique_Id_t)) ; 
  ald.version = NULL ;

  cur = g_list_first(zephyr_buddies) ;
  while(cur) {
    handle = (gchar *) cur->data ;
#if ZEPHYR_DEBUG
    fprintf(stderr, "DEBUG: zephyr_check_all_buddies: Checking user %s\n",
            handle) ;
#endif
    status = ZRequestLocations(handle, &ald, UNSAFE, 
                               ZEPHYR_AUTH(zephyr_auth_loc_req)) ;
    ZFreeALD(&ald) ;
    if(status != ZERR_NONE)
      com_err("everybuddy: zephyr_check_all_buddies", status,
              "while requesting locations") ;
    cur = g_list_next(cur) ;
  }
  
  return TRUE ;
  
}

/*****************************************************************************/

void zephyr_reset_buddies(void) {
 GList *cur = NULL ;
 eb_account *account ;
 gchar *handle ;
 
 cur = g_list_first(zephyr_buddies) ; 
 
 while(cur) {
   handle = (gchar *) cur->data ;
#if ZEPHYR_DEBUG
   fprintf(stderr, "DEBUG: zephyr_reset_buddies: Resetting user %s\n",
           handle) ;
#endif
   
   account = find_account_by_handle(handle, SERVICE_INFO.protocol_id) ;
   if(!account) continue ;
   
   if(account->online || (ZEPHYR_STATUS = ZEPHYR_HIDDEN)) {
     ZEPHYR_STATUS = ZEPHYR_OFFLINE ;
     buddy_logoff(account) ;
   }
   
   cur = g_list_next(cur) ;
 }
 
}

/*****************************************************************************/

input_list *eb_zephyr_get_prefs(void) {
  return zephyr_prefs ;
}

/*****************************************************************************/

void eb_zephyr_read_prefs_config(GList *values) {
  gchar *text = NULL ;
  
  text = value_pair_get_value(values, "flush_locs") ;
  if(text) zephyr_flush_locs = atoi(text) ;
  
  text = value_pair_get_value(values, "auth_loc_reqs") ;
  if(text) zephyr_auth_loc_req = atoi(text) ;
  
  text = value_pair_get_value(values, "auth_msg_out") ;
  if(text) zephyr_auth_msg_out = atoi(text) ;
  
  text = value_pair_get_value(values, "auth_msg_in") ;
  if(text) zephyr_auth_msg_in = atoi(text) ;
  
  text = value_pair_get_value(values, "not_chk_int") ;
  if(text) strncpy(zephyr_not_chk_int, text, 255) ;
    
  text = value_pair_get_value(values, "con_chk_int") ;
  if(text) strncpy(zephyr_con_chk_int, text, 255) ;
  
}

/*****************************************************************************/

GList *eb_zephyr_write_prefs_config(void) {
  GList *config = NULL ;
  gchar buf[4] ;
  
  g_snprintf(buf, 3, "%d", zephyr_flush_locs) ;
  config = value_pair_add(config, "flush_locs", buf) ;
  
  config = value_pair_add(config, "auth_loc_reqs", buf) ;
  config = value_pair_add(config, "auth_msg_out", buf) ;
  config = value_pair_add(config, "auth_msg_in", buf) ;
  config = value_pair_add(config, "not_chk_int", zephyr_not_chk_int) ;
  config = value_pair_add(config, "con_chk_int", zephyr_con_chk_int) ;
  
  return config ;
}

/*****************************************************************************/
/*** End conditional compile *************************************************/
/*****************************************************************************/

