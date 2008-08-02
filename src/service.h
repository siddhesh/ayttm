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
 * service.h
 * Header file for service bits.
 */

#ifndef __SERVICE_H__
#define __SERVICE_H__

#include "chat_room.h"
#include "input_list.h"


extern int NUM_SERVICES;
#define GET_SERVICE(x)	eb_services[x->service_id]
#define RUN_SERVICE(x)	GET_SERVICE(x).sc
#define CAN(x, service)	(RUN_SERVICE(x)->service != NULL)

struct service_callbacks {
	/*callback to determine if remote account is online*/
	int (*query_connected) (eb_account *account);

	/*callback that will establish connection to server, and set up
	  any timers and listeners necessary */
	void (*login) (eb_local_account *account);

	/*callback to disable any timers and disconnect from server*/
	void (*logout) (eb_local_account *account);

	/* send message from one account to another */
	void (*send_im) (eb_local_account *account_from,
			 eb_account *account_to,
			 char *message);

        /* send a typing notification - the return value is the number 
	   of seconds to wait before sending again - NULL if the service 
	   doesn't support it
        */
        int (*send_typing) (eb_local_account *account_from,
                             eb_account *account_to);
        int (*send_cr_typing) (eb_chat_room *chatroom);

	/* reads local account information from a file */
	eb_local_account * (*read_local_account_config) (LList * values);

	/* gets the configuration necessary to be written to a file */
	LList * (*write_local_config)( eb_local_account * account );

	/* reads contact information */
	eb_account * (*read_account_config) (eb_account *ea, LList * config);

	/*Returns the list of all possible states that the user can be in*/
	LList * (*get_states)();

	/*Retruns the index to the current state that the service is int*/
	int (*get_current_state)(eb_local_account * account);

	/*Sets the online status for the given account*/
	void (*set_current_state)(eb_local_account * account, int state);

	/*Checks login validity */
	char * (*check_login)(const char * login, const char * pass);

	/*Informs the service of an account that it needs to manage*/
	void (*add_user)(eb_account * account);

	/*
	 * Notifies the service that it doesn't need to track an account
	 * del_user should free any protocol_account_data it has created
	 */
	void (*del_user)(eb_account * account);

	/*Notifies the service that the user needs to be ignored*/
	void (*ignore_user)(eb_account * account);

	/*Notifies the service that the user is no longer ignored*/
	void (*unignore_user)(eb_account * account, const char *new_group);

	/*Notifies the service that a user's contact/nick name has changed */
	void (*change_user_name)(eb_account * account, const char * name);

	/*Notifies the service that an account's group has changed
	  can be used to inform the server of the group change.
	  Can be NULL if not implemented*/
	void (*change_group)(eb_account * account, const char *new_group);

	/* deletes group on server 
	   unspecified results if called with a non-empty group ! */
	void (*del_group)(eb_local_account *ela, const char *group);
	
	void (*add_group)(eb_local_account *ela, const char *group);
	
	void (*rename_group)(eb_local_account *ela, const char *old_group, const char *new_group);

	/*Informs the service of an account that it needs to manage*/
	int (*is_suitable)(eb_local_account *local, eb_account *remote);

	/*Creates a new account*/
	eb_account*(*new_account)(eb_local_account *ela, const char * account );

	/*This returns the string representing the status,
	  this will get used on the Contact List, if statuses
	  are not available, this should return the empty string */

	char *(*get_status_string)(eb_account * account);

	/*This returns the string representing the status,
	  this will get used on the Contact List, if statuses
	  are not available, this should return the empty string */

	const char **(*get_status_pixmap)(eb_account * account);

	/*set the idle time (set this to null if N/A)*/

	void (*set_idle)(eb_local_account * account, int idle );

	/* set an away message */

	void (*set_away)(eb_local_account * account, char * message, int away );

	/*send a message to a chat room*/

	void (*send_chat_room_message)(eb_chat_room * room, char * message);

	/*these are used to join and leave a chat room */

	void (*join_chat_room)(eb_chat_room * room);
	void (*leave_chat_room)(eb_chat_room * room);

	/*this it to create a new chat room*/

	eb_chat_room * (*make_chat_room)(char * name, eb_local_account * account, int is_public);

	/*this is to invite somebody into the chat room*/

	void (*send_invite)( eb_local_account * account, eb_chat_room * room,
				                         char * user, const char * message);

	void (*accept_invite)( eb_local_account * account, void * invitation );

	void (*decline_invite)( eb_local_account * account, void * inviatation );

	/*this is to send a file */

	void (*send_file)( eb_local_account * from, eb_account * to, char * file );

	/* gets called for each account a contact has when a chat windows is closed
	 * can be null if you don't need it
	 */

	void (*terminate_chat)(eb_account * account );

        /* called to request a users information */
        void(*get_info)(eb_local_account * account_from, eb_account * account_to);


	/* this is used so that the 'plugin' can add it's own proprietary
	 * config featurs into the prefs window
	 */

        input_list * (*get_prefs)();

	/*
	 * processes any service specific preferences
	 */

	void (*read_prefs_config) (LList * values);

	/*
	 * get the service specific preferences so they can be written
	 * to a file
	 */

	LList * (*write_prefs_config)();
	LList * (*add_importers)(LList *);


        /* This lot are used to retrieve various protocol-specific eye-candy.
           When these functions are called, their return values are NOT free()ed,
           so you can just return a static variable if you want */


        /* This requests a LList of protocol-specific smilies (type protocol_smiley, see
           smileys.h */
        LList * (*get_smileys)(void);

        /* This returns the color associated with this protocol (in HTML form,
           so for example AIM is "#000088" */
        char * (*get_color)(void);
	
	/* free an account */
	void (*free_account_data)(eb_account *account);
	
	int (*handle_url)(const char *url);
	
	void (*get_public_chatrooms)(eb_local_account *ela, 
					void (*public_chatroom_callback) (LList *list, void *data),
					void *data);
};

/*for every server you have the following: the name of the service,
  the server, port, the type of protocall used by the service, and a pointer
  to the service specific callbacks.
  */
enum service_capabilities {
	SERVICE_CAN_NOTHING      = 0,
	SERVICE_CAN_OFFLINEMSG   = 1 << 0,
	SERVICE_CAN_GROUPCHAT    = 1 << 1,
	SERVICE_CAN_FILETRANSFER = 1 << 2,
	SERVICE_CAN_ICONVERT     = 1 << 3,
	SERVICE_CAN_MULTIACCOUNT = 1 << 4,
	SERVICE_CAN_EVERYTHING   = ~0
};

struct service {
	char *name;
	int protocol_id;
	int capabilities;
	struct service_callbacks *sc;
};

#define can_offline_msg(x)	(x.capabilities & SERVICE_CAN_OFFLINEMSG)
#define can_group_chat(x)	(x.capabilities & SERVICE_CAN_GROUPCHAT)
#define can_file_transfer(x)	(x.capabilities & SERVICE_CAN_FILETRANSFER)
#define can_iconvert(x)		(x.capabilities & SERVICE_CAN_ICONVERT)
#define can_multiaccount(x)	(x.capabilities & SERVICE_CAN_MULTIACCOUNT)

#ifdef __cplusplus
extern "C" {
#endif

int add_service(struct service *Service_Info);
int get_service_id( const char * servicename );
char* get_service_name( int serviceid );

void load_modules();

#if defined(__MINGW32__) && defined(__IN_PLUGIN__)
__declspec(dllimport) struct service eb_services[];
#else
extern struct service eb_services[];
#endif

void add_idle_check();
void serv_touch_idle();
	
LList * get_service_list();

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*__SERVICE_H__*/
