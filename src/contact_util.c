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
 * contact_util.c - utility functions for contact handling
 *
 */

#include "intl.h"
#include <stdlib.h>
#include <string.h>
#include "service.h"
#include "offline_queue_mgmt.h"
#include "contact_util.h"

static LList *groups = NULL;
extern LList *accounts;

static LList * account_hash[27]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static int account_hash_length=27;

/*
 * Warning: Not portable across character sets
 */
static int isalpha(char c)
{
	return (c>='a' && c<='z') || (c>='A' && c<='Z');
}

static int hash(const char * s)
{
	int i;
	if(!s)
		return -1;

	if(isalpha(s[0]))
		i = s[0]-(s[0]>'a'?'a':'A');
	else
		i = account_hash_length-1;

	if(isalpha(s[1]))
		i += s[1]-(s[1]>'a'?'a':'A');
	else
		i += account_hash_length-1;

	return i%account_hash_length;

}

static void add_to_hash(eb_account * ea)
{
	int i = hash(ea->handle);
	if(i<0)
		return;
	account_hash[i] = l_list_prepend(account_hash[i], ea);
}

static void remove_from_hash(eb_account * ea)
{
	int i = hash(ea->handle);
	if(i<0)
		return;
	account_hash[i] = l_list_remove(account_hash[i], ea);
}

static eb_account * find_in_hash(const char * handle, const eb_local_account * ela)
{
	int i = hash(handle);
	LList * l;
	if(i<0)
		return NULL;

	for(l = account_hash[i]; l; l = l_list_next(l)) {
		eb_account * ea = l->data;
		if(!strcmp(ea->handle, handle) && (!ela || ea->ela == ela))
			return ea;
	}
	return NULL;
}

#define ONLINE(ela) (ela->connected || ela->connecting)
/* compares two contact names */
static int contact_cmp(const void * a, const void * b)
{
	const struct contact *ca=a, *cb=b;
	return strcasecmp(ca->nick, cb->nick);
}

grouplist * find_grouplist_by_name(const char *name)
{
	LList *l;

	if(name == NULL)
		return NULL;

	for(l = groups; l; l=l_list_next(l) )
		if(!strcasecmp(((grouplist *)l->data)->name, name))
			return l->data;

	return NULL;
}

struct contact * find_contact_in_group_by_nick(const char *nick, grouplist *group)
{
	LList *l;

	if(nick == NULL || group == NULL)
		return NULL;

	for(l = group->members; l; l=l_list_next(l))
		if(!strcasecmp(((struct contact *)l->data)->nick, nick))
			return l->data;

	return NULL;
}

struct contact * find_contact_by_nick(const char *nick)
{
	LList *l;
	struct contact *contact=NULL;

	if(nick == NULL)
		return NULL;

	for(l=groups; l; l=l_list_next(l))
		if((contact = find_contact_in_group_by_nick(nick, l->data)) != NULL)
			return contact;

	return NULL;
}

struct contact * find_contact_by_handle( char * handle )
{
	eb_account * account = find_in_hash(handle, NULL);
	if(account)
		return account->account_contact;
	else
		return NULL;
	/*
	LList *l1, *l2, *l3;

	if (handle == NULL) 
		return NULL;

	for(l1 = groups; l1; l1=l_list_next(l1) ) {
		for(l2 = ((grouplist*)l1->data)->members; l2; l2=l_list_next(l2) ) {
			for(l3 = ((struct contact*)l2->data)->accounts; l3; l3=l_list_next(l3)) {
				eb_account * account = (eb_account*)l3->data;
				if(!strcmp(account->handle, handle))
					return l2->data;
			}
		}
	}
	return NULL;
	*/
}

eb_account * find_account_by_handle(const char *handle, const eb_local_account * ela)
{

	return find_in_hash(handle, ela);
	/*
	LList *l1, *l2, *l3;

	for(l1 = groups; l1; l1=l_list_next(l1))
		for(l2 = ((grouplist *)l1->data)->members; l2; l2=l_list_next(l2))
			for(l3 = ((struct contact *)l2->data)->accounts; l3; l3=l_list_next(l3)) {
				eb_account * ea = l3->data;
				if(ea->ela == ela && !strcmp(ea->handle, handle))
					return ea;
			}
	return NULL;
	*/
}
		
grouplist * add_group(const char *group_name)
{
	LList *node = NULL;
	grouplist *gl;
	
	if(group_name == NULL || *group_name=='\0')
		return NULL;

	gl = calloc(1, sizeof(grouplist));
	strncpy(gl->name, group_name, sizeof(gl->name)-1);

	groups = l_list_append( groups, gl );

	for( node = accounts; node; node = l_list_next(node) ) {
		eb_local_account *ela = node->data;
		if (CAN(ela, add_group)) {
			if (ONLINE(ela))
				RUN_SERVICE(ela)->add_group(ela, group_name);
			else
				group_mgmt_queue_add(ela, NULL, MGMT_GRP_ADD, group_name);
		}
	}

	return gl;
}

struct contact * add_contact_with_group(const char *contact_name, grouplist *group)
{
	struct contact *contact;

	if(!group || !contact_name)
		return NULL;

	contact = calloc(1, sizeof(struct contact));
	strncpy(contact->nick, contact_name, sizeof(contact->nick)-1);
	
	group->members = l_list_insert_sorted(group->members, contact, contact_cmp );

	contact->group = group;

	return contact;
}

struct contact * add_contact(const char *contact_name, const char *group_name)
{
	grouplist *group;
	
	if(contact_name == NULL || *contact_name=='\0')
		return NULL;

	if(group_name == NULL || *group_name=='\0')
		group_name = _("Unknown");

	group = find_grouplist_by_name(group_name);

	if(group == NULL)
		group = add_group(group_name);

	return add_contact_with_group(contact_name, group);
}

eb_account * add_account(const char *handle, struct contact *contact, eb_local_account *ela)
{
	eb_account *account;

	if(handle == NULL || *handle=='\0' || contact==NULL)
		return NULL;

	account = calloc(1, sizeof(eb_account));
	account->service_id = ela->service_id;
	account->ela = ela;
	account->account_contact = contact;

	if(CAN(ela, add_user)) {
		if (ONLINE(ela))
			RUN_SERVICE(ela)->add_user(account);
		else
			contact_mgmt_queue_add(account, MGMT_ADD, contact->group->name);
	}

	contact->accounts = l_list_append(contact->accounts, account);
	add_to_hash(account);

	return account;
}

void destroy_account(eb_account * account)
{
	RUN_SERVICE(account)->free_account_data(account);
	free(account);
}

void destroy_contact(struct contact * contact)
{
	while(contact->accounts) {
		destroy_account(contact->accounts->data);
		contact->accounts = l_list_remove_link(contact->accounts, contact->accounts);
	}

	free(contact);
}

void destroy_group(grouplist * group)
{
	while(group->members) {
		destroy_contact(group->members->data);
		group->members = l_list_remove_link(group->members, group->members);
	}

	free(group);
}

void remove_account(eb_account * account)
{
	RUN_SERVICE(account)->del_user(account);
	account->account_contact->accounts = l_list_remove(account->account_contact->accounts, account);
	remove_from_hash(account);
	destroy_account(account);
}

void remove_contact(struct contact * contact)
{
	contact->group->members = l_list_remove(contact->group->members, contact);
	while(contact->accounts) {
		remove_account(contact->accounts->data);
		contact->accounts = l_list_remove_link(contact->accounts, contact->accounts);
	}
	destroy_contact(contact);
}

void remove_group(grouplist * group)
{
	LList *l;

	groups = l_list_remove(groups, group);

	while(group->members) {
		remove_contact(group->members->data);
		group->members = l_list_remove_link(group->members, group->members);
	}

	for(l=accounts; l; l=l_list_next(l)) {
		eb_local_account * ela = l->data;
		if(CAN(ela, del_group)) {
			if (ONLINE(ela))
				RUN_SERVICE(ela)->del_group(ela, group->name);
			else
				group_mgmt_queue_add(ela, group->name, MGMT_GRP_DEL, NULL);
		}
	}

	destroy_group(group);
}

static void handle_group_change(eb_account *ea, const char *og, const char *ng)
{
	/* if the groups are same, do nothing */
	if(!strcasecmp(ng, og))
		return;

	/* adding to ignore */
	if(!strcmp(ng, _("Ignore")) && CAN(ea, ignore_user))
		RUN_SERVICE(ea)->ignore_user(ea);

	/* remove from ignore */
	else if(!strcmp(og, _("Ignore")) && CAN(ea, unignore_user))
		RUN_SERVICE(ea)->unignore_user(ea, ng);
		
	/* just your regular group change */
	else if(CAN(ea, change_group)) {
		if (ONLINE(ea->ela))
			RUN_SERVICE(ea)->change_group(ea, ng);
		else
			contact_mgmt_queue_add(ea, MGMT_MOV, ng);
	}

}

void rename_group(grouplist * group, const char * new_name)
{
	LList *l;

	if(new_name == NULL)
		return;
	
	for(l=accounts; l; l=l_list_next(l)) {
		eb_local_account * ela = l->data;
		if(CAN(ela, rename_group)) {
			if (ONLINE(ela))
				RUN_SERVICE(ela)->rename_group(ela, group->name, new_name);
			else
				group_mgmt_queue_add(ela, NULL, MGMT_GRP_REN, new_name);
		}
	}

	strncpy(group->name, new_name, sizeof(group->name)-1);
}

void rename_contact(struct contact * contact, const char * new_name)
{
	LList *l;

	if(new_name == NULL)
		return;

	for(l=contact->accounts; l; l=l_list_next(l)) {
		eb_account * ea = l->data;
		if(CAN(ea, change_user_name)) {
			if (ONLINE(ea->ela))
				RUN_SERVICE(ea)->change_user_name(ea, new_name);
			else
				contact_mgmt_queue_add(ea, MGMT_REN, new_name);
		}
	}
}

void move_contact(struct contact * contact, grouplist * new_group)
{
	LList *l;

	for(l=contact->accounts; l; l=l_list_next(l))
		handle_group_change(l->data, contact->group->name, new_group->name);

	contact->group->members = l_list_remove(contact->group->members, contact);
	contact->group = new_group;
	new_group->members = l_list_insert_sorted(new_group->members, contact, contact_cmp);
}

void move_account(eb_account * account, struct contact * new_contact)
{
	struct contact * old_contact = account->account_contact;

	handle_group_change(account, old_contact->group->name, new_contact->group->name);

	old_contact->accounts = l_list_remove(old_contact->accounts, account);
	account->account_contact = new_contact;
	new_contact->accounts = l_list_append(new_contact->accounts, account);
}

LList * get_group_names( void )
{
	LList *g=NULL, *g2;
	for(g2 = groups; g2; g2=l_list_next(g2))
		g = l_list_insert_sorted(g, ((grouplist *)g2->data)->name, 
				(LListCompFunc)strcasecmp);

	return g;
}

LList * get_group_contact_names( grouplist * group )
{
	LList *g=NULL, *g2;
	for(g2 = group->members; g2; g2=l_list_next(g2))
		g = l_list_insert_sorted(g, ((struct contact *)g2->data)->nick,
				(LListCompFunc)strcasecmp);

	return g;
}

