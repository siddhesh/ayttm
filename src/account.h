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
 * account.h
 * file for account bits
 */

#ifndef __ACCOUNT_H__
#define __ACCOUNT_H__

#include "llist.h"

struct contact;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _grouplist
{
	char name[255];
	LList * members;
	void * list_item; /* GtkWidget */
	void * tree; /* GtkWidget */
	void * label; /* GtkWidget */
	int contacts_online;
	int contacts_shown;
} grouplist;

typedef struct local_account {
        int service_id;
        char *handle;
	char alias[255];
        int connected;
        int connecting;
	void * status_button; /* GtkWidget */
	LList * status_menu;
        void *protocol_local_account_data;
	int mgmt_flush_tag;
} eb_local_account;

typedef struct account {
        int service_id;
	eb_local_account * ela;
        char handle[255];
	struct contact * account_contact;
        void * protocol_account_data;
	void * list_item; /* GtkWidget */
	void * status; /* GtkWidget */
	void * pix; /* GtkWidget */
	int icon_handler;
	int online;
	int status_handler;
	struct _info_window * infowindow;
} eb_account;

int load_accounts();
int load_contacts();
void write_contact_list();
void write_account_list();
eb_account *dummy_account(char *handle, char *group, int serviceid);

#ifdef __cplusplus
}
#endif


#endif /* __ACCOUNT_H__*/
