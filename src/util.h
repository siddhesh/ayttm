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
 *  util.h
 *  generic useful stuff
 *
 */

#ifndef __UTIL_H__
#define __UTIL_H__

#ifndef __MINGW32__
# include <sys/types.h>
# include <unistd.h>
#else
# include <io.h>
# include <process.h>
#endif

#include <glib.h>	/* for llist_to_glist and glist_to_llist */
#include "contact.h"


#if !defined(FALSE) && !defined(TRUE)
enum {FALSE, TRUE};
#endif
/* forward declarations */
struct _eb_chat_room;

#ifdef __cplusplus
extern "C" {
#endif

/* TODO check heder in sound.c */
int clean_pid(void * dummy);
char * get_local_addresses();


#ifdef __cplusplus
}
#endif

enum
{
  TOKEN_NORMAL,
  TOKEN_HTTP,
  TOKEN_FTP,
  TOKEN_EMAIL,
  TOKEN_CUSTOM

};

#ifdef __cplusplus
extern "C" {
#endif

char * convert_eol (char * input);
char * linkify( char * input );

char * escape_string(const char * input);
char * unescape_string(const char * input);

eb_local_account * find_suitable_local_account_for_remote( eb_account * ea, eb_local_account *preferred );
eb_local_account * find_suitable_local_account( eb_local_account * first,
						int second );
eb_local_account * find_local_account_for_remote( eb_account *remote, int online );
eb_account * can_offline_message( struct contact * con );

eb_account * find_suitable_remote_account( eb_account * first,
					   struct contact * rest );
eb_account * find_suitable_file_transfer_account( eb_account * first,
						  struct contact * rest );
struct _eb_chat_room * find_chat_room_by_id( char * id );
struct _eb_chat_room * find_chat_room_by_name( char * name, int service_id );
LList * find_chatrooms_with_remote_account(eb_account *remote);
grouplist * find_grouplist_by_nick(char * nick);
grouplist * find_grouplist_by_name(char * name);
struct contact * find_contact_by_handle( char * handle );
struct contact * find_contact_by_nick( const char * nick);
struct contact * find_contact_in_group_by_nick( char * nick, grouplist *gl );
eb_account * find_account_by_handle( const char * handle, int type );
eb_account * find_account_with_ela( const char * handle, eb_local_account *ela );
eb_local_account * find_local_account_by_handle( char * handle, int type );
void strip_html(char * text);
int remove_account( eb_account * a );
void remove_contact( struct contact * c );
void rename_contact( struct contact * c, char *newname);
void remove_group( grouplist * g );
void add_group( char * name );
void rename_group( grouplist *g, char * new_name );
struct contact * add_new_contact( char * group, char * con, int type );
struct contact * add_dummy_contact( char * con, eb_account * account );
void clean_up_dummies(void);
void add_unknown( eb_account * ea );
void add_unknown_with_name( eb_account * ea, char * name );
void add_account( char * contact, eb_account * account );
void add_account_silent( char * contact, eb_account * account );
struct contact * move_account( struct contact * c, eb_account *account );
void move_contact( char * group, struct contact * c);
void invite_dialog( eb_local_account * ela, char * user, char * chat_room,
		    void * id );
void make_safe_filename(char *buff, char *name, char *group);
int connected_local_accounts(void);

pid_t create_lock_file(char* fname);
void delete_lock_file(char* fname);
void eb_generic_menu_function(void *add_button, void * userdata);
LList * get_groups();
void rename_nick_log(char *oldgroup, char *oldnick, char *newgroup, char *newnick);

eb_account *find_account_for_protocol(struct contact *c, int service);
GList * llist_to_glist(LList * l, int free_old);
/* free_old will free the old list after converting */
LList * glist_to_llist(GList * g, int free_old);

int send_url(const char *url);
int eb_send_message (const char *to, const char *msg, int service);
LList * ay_save_account_information(int service_id);
void ay_restore_account_information(LList *saved);

#ifdef __cplusplus
} /* extern "C" */
#endif

#ifndef NAME_MAX
#define NAME_MAX 4096
#endif

#endif

void ay_dump_cts(void);
void ay_dump_elas(void);
