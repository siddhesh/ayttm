/*
 * Ayttm 
 *
 * Copyright (C) 2003, the Ayttm team
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
 * contact_util.h - utility functions for contact handling
 *
 */

#ifndef __CONTACT_UTIL_H__
#define __CONTACT_UTIL_H__

#include "llist.h"
#include "account.h"
#include "contact.h"

/* Find methods */
grouplist * find_grouplist_by_name(const char *name);
struct contact * find_contact_in_group_by_nick(const char *nick, grouplist *group);
struct contact * find_contact_by_nick(const char *nick);
struct contact * find_contact_by_handle( char * handle );
eb_account * find_account_by_handle(const char *handle, const eb_local_account * ela);
eb_account * find_account_by_handle_and_service(const char * handle, int service_id);

/* Add methods */
grouplist * add_group(const char *group_name);
struct contact * add_contact(const char *contact_name, const char *group_name, int default_service);
struct contact * add_contact_with_group(const char *contact_name, grouplist *group, int default_service);
struct contact * add_dummy_contact(const char *contact_name, eb_account *ea, eb_local_account *ela);
eb_account * add_account(const char *handle, struct contact *contact, eb_local_account *ela);

/* Destroy methods */
void destroy_account(eb_account * account);
void destroy_contact(struct contact * contact);
void destroy_group(grouplist * group);
void clean_up_dummies(void);

/* Remove methods */
void remove_account(eb_account * account);
void remove_contact(struct contact * contact);
void remove_group(grouplist * group);

/* Rename/move methods */
void rename_group(grouplist * group, const char * new_name);
void rename_contact(struct contact * contact, const char * new_name);
void move_contact(struct contact * contact, grouplist * new_group);
void move_account(eb_account * account, struct contact * new_contact);

/* util methods */
int contact_cmp(const void *ct_a, const void *ct_b);
int account_cmp(const void *a, const void *b);
int group_cmp(const void *a, const void *b);
LList * get_group_names( void );
LList * get_group_contact_names( grouplist * group );

/* contacts file */
void write_contacts_to_fh(FILE *fp);
void load_contacts_from_fh(FILE *fp);

#endif

