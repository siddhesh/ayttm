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
static grouplist *dummy_group = NULL;

class account_hash {
	LList * _hash[27];
	int hash_length;

	int isalpha(char c)
	{
		return (c>='a' && c<='z') || (c>='A' && c<='Z');
	}

	int hash();

	public:
	account_hash(): hash_length(27)
	{
		_hash={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	}
	void add(eb_account * ea);
	void remove(eb_account *ea);
	eb_account * find(const char * handle, const eb_local_account *ela=NULL);
	eb_account * find(const char * handle, int service_id);
	~account_hash()
	{
		for(int i=0; i<hash_length; i++) {
			while(_hash[i]) {
				free(_hash[i]->data);
				_hash[i] = l_list_remove(_hash[i], _hash[i]);
			}
		}
	}
};

static account_hash hash;


int account_hash::hash(const char * s)
{
	int i;
	if(!s)
		return -1;

	if(isalpha(s[0]))
		i = s[0]-(s[0]>'a'?'a':'A');
	else
		i = hash_length-1;

	if(isalpha(s[1]))
		i += s[1]-(s[1]>'a'?'a':'A');
	else
		i += hash_length-1;

	return i%hash_length;
}

void account_hash::add(eb_account * ea)
{
	int i = hash(ea->handle);
	if(i<0)
		return;
	_hash[i] = l_list_prepend(_hash[i], ea);
}

void account_hash::remove(eb_account * ea)
{
	int i = hash(ea->handle);
	if(i<0)
		return;
	_hash[i] = l_list_remove(_hash[i], ea);
}

eb_account * account_hash::find(const char * handle, int service_id)
{
	int i = hash(handle);
	LList * l;
	if(i<0)
		return NULL;

	for(l = _hash[i]; l; l = l_list_next(l)) {
		eb_account * ea = l->data;
		if(!strcmp(ea->handle, handle) && ea->service_id == service_id)
			return ea;
	}
	return NULL;
}

eb_account * account_hash::find(const char * handle, const eb_local_account * ela=NULL)
{
	int i = hash(handle);
	LList * l;
	if(i<0)
		return NULL;

	for(l = _hash[i]; l; l = l_list_next(l)) {
		eb_account * ea = l->data;
		if(!strcmp(ea->handle, handle) && (!ela || ea->ela == ela))
			return ea;
	}
	return NULL;
}

#define ONLINE(ela) (ela->connected || ela->connecting)
/**
 * Contact comparator
 * @param	a	the first contact
 * @param	b	the second contact
 *
 * @return	0	if contacts have the same name
 * 		>0	if a > b
 * 		<0	if b > a
 */
int contact_cmp(const void *a, const void *b)
{
	const struct contact *ca=a, *cb=b;
	return strcasecmp(ca->nick, cb->nick);
}

/**
 * Account comparator
 * @param	a	the first account
 * @param	b	the second account
 *
 * @return	0	if accounts have equal sort positioning
 * 		>0	if a > b
 * 		<0	if b > a
 */
int account_cmp(const void *a, const void *b)
{
	eb_account *ca=(eb_account *)a, *cb=(eb_account *)b;
	if(ca->priority == cb->priority)
		return strcasecmp(ca->handle, cb->handle);
	else
		return ca->priority - cb->priority;
}
	
/**
 * Group comparator
 * @param	a	the first group
 * @param	b	the second group
 *
 * @return	0	if groups have the same name
 * 		>0	if a > b
 * 		<0	if b > a
 */
int group_cmp(const void *a, const void *b)
{
	const grouplist *ga=a, *gb=b;
	return strcasecmp(ga->name, gb->name);
}


/**
 * Return a group given its name
 * @param	name	the group's name
 *
 * @return	the group or NULL if not found
 */
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

/**
 * Return a contact from a group given its nickname
 * @param	nick	the contact's nick name
 * @param	group	the group in which to search
 *
 * @return	the contact or NULL if not found
 */
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

/**
 * Return a contact from any group given its nickname
 * @param	nick	the contact's nick name
 *
 * @return	the contact or NULL if not found
 */
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

/**
 * Return a contact given the handle of one of its accounts
 * @param	handle	the handle of an account of this contact
 *
 * @return	the contact or NULL if not found
 */
struct contact * find_contact_by_handle( char * handle )
{
	eb_account * account = hash.find(handle);
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

/**
 * Return an account given its handle and associated local account
 * @param	handle	the handle of the account
 * @param	ela	the local account associated with this account
 *
 * @return	the account or NULL if not found
 */
eb_account * find_account_by_handle(const char *handle, const eb_local_account * ela)
{

	return hash.find(handle, ela);
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

/**
 * Return an account given its handle and service id
 * @param	handle		the handle of the account
 * @param	service_id	the service_id of the account
 *
 * @return	the account or NULL if not found
 */
eb_account * find_account_by_handle(const char *handle, int service_id)
{

	return hash.find(handle, service_id);
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

/**
 * Add a new group with the given name and return it
 * @param	group_name	the name of the new group
 *
 * @return	the group or NULL if group_name is NULL or empty
 */
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

static struct contact * create_contact(const char *con, int type)
{
	struct contact * c = calloc(1, sizeof(struct contact));
	if (con != NULL) 
		strncpy(c->nick, con, sizeof(c->nick));

	c->default_chatb = c->default_filetransb = type;

	c->icon_handler = -1;

	return( c );
}


/**
 * Add a new contact with the given name to a group and return it
 * @param	contact_name	the name of the new contact
 * @param	group		the group to add the contact to
 * @param	default_service	the default service to use when communicating with this contact
 *
 * @return	the contact or NULL if group or contact_name is NULL
 */
struct contact * add_contact_with_group(const char *contact_name, grouplist *group, int default_service)
{
	struct contact *contact;

	if(!group || !contact_name)
		return NULL;

	contact = create_contact(contact_name, default_service);
	
	group->members = l_list_insert_sorted(group->members, contact, contact_cmp );

	contact->group = group;

	return contact;
}

/**
 * Add a new contact with the given name to a group, given its name, and return it
 * @param	contact_name	the name of the new contact
 * @param	group_name	the name of the group to add the contact to
 * @param	default_service	the default service to use when communicating with this contact
 *
 * @return	the contact or NULL if group_name or contact_name is NULL or empty
 */
struct contact * add_contact(const char *contact_name, const char *group_name, int default_service)
{
	grouplist *group;
	
	if(contact_name == NULL || *contact_name=='\0')
		return NULL;

	if(group_name == NULL || *group_name=='\0')
		group_name = _("Unknown");

	group = find_grouplist_by_name(group_name);

	if(group == NULL)
		group = add_group(group_name);

	return add_contact_with_group(contact_name, group, default_service);
}

/**
 * Add a new account to a dummy contact.  Will not add on the server
 * @param	contact_name	the contact name to add the account to
 * @param	ea		the account to be added
 * @param	ela		the local account that this account is associated with
 *
 * @return	the contact or NULL if parameters are invalid
 */
struct contact * add_dummy_contact(const char *contact_name, eb_account *ea, eb_local_account *ela)
{
	struct contact *c = create_contact(contact_name, ea->service_id);

	c->accounts = l_list_prepend(c->accounts, ea);
	ea->account_contact = c;
	ea->icon_handler = ea->status_handler = -1;

	if(!dummy_group) {
		dummy_group = calloc(1, sizeof(grouplist));
		/* don't translate this string */
		snprintf(dummy_group->name, sizeof(dummy_group->name),
				"__Ayttm_Dummy_Group__%d__", rand());
	}

	dummy_group->members = l_list_prepend(dummy_group->members, c);
	c->group = dummy_group;
	c->online = 1;
	return c;
}

/**
 * Clean up the dummy group
 */
void clean_up_dummies(void)
{
	if(dummy_group)
		destroy_group(dummy_group);
}

/**
 * Add a new account to a contact attached to a local account.  Also adds the account on the server.
 * @param	handle		the handle of the new account
 * @param	contact		the contact to add the account to
 * @param	ela		the local account that this account is associated with
 *
 * @return	the account or NULL if handle or contact is NULL or empty
 */
eb_account * add_account(const char *handle, struct contact *contact, eb_local_account *ela)
{
	eb_account *account;

	if(handle == NULL || *handle=='\0' || contact==NULL)
		return NULL;

	account = calloc(1, sizeof(eb_account));
	strncpy(account->handle, handle, sizeof(account->handle)-1);
	account->service_id = ela->service_id;
	account->ela = ela;
	account->account_contact = contact;

	if(CAN(ela, add_user)) {
		if (ONLINE(ela))
			RUN_SERVICE(ela)->add_user(account);
		else
			contact_mgmt_queue_add(account, MGMT_ADD, contact->group->name);
	}

	contact->accounts = l_list_insert_sorted(contact->accounts, account, account_cmp);
	hash.add(account);

	return account;
}

/**
 * Destroy all data associated with an account.  Call this to free an account's memory
 * @param	account		the account to destroy
 */
void destroy_account(eb_account *account)
{
	RUN_SERVICE(account)->free_account_data(account);
	free(account);
}

/**
 * Destroy all data associated with a contact, including all accounts under it.
 * Call this to free a contact's memory
 * @param	contact		the contact to destroy
 */
void destroy_contact(struct contact *contact)
{
	while(contact->accounts) {
		destroy_account(contact->accounts->data);
		contact->accounts = l_list_remove_link(contact->accounts, contact->accounts);
	}

	free(contact);
}

/**
 * Destroy all data associated with a group, including all contacts and accounts under it.
 * Call this to free a group's memory
 * @param	group		the group to destroy
 */
void destroy_group(grouplist *group)
{
	while(group->members) {
		destroy_contact(group->members->data);
		group->members = l_list_remove_link(group->members, group->members);
	}

	free(group);
}

/**
 * Remove an account from your account list.  This will destroy the account after removing it.
 * @param	account		The account to remove
 */
void remove_account(eb_account *account)
{
	RUN_SERVICE(account)->del_user(account);
	account->account_contact->accounts = l_list_remove(account->account_contact->accounts, account);
	hash.remove(account);
	destroy_account(account);
}

/**
 * Remove a contact from your contact list.  This will destroy the contact after removing it.
 * @param	contact		The contact to remove
 */
void remove_contact(struct contact *contact)
{
	contact->group->members = l_list_remove(contact->group->members, contact);
	while(contact->accounts) {
		remove_account(contact->accounts->data);
		contact->accounts = l_list_remove_link(contact->accounts, contact->accounts);
	}
	destroy_contact(contact);
}

/**
 * Remove a group from your group list.  This will destroy the group after removing it.
 * @param	group		The group to remove
 */
void remove_group(grouplist *group)
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

/**
 * Rename a group
 * @param	group		The group that we need to rename
 * @param	new_name	The new name of the group
 */
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

/**
 * Rename a contact
 * @param	contact		The contact that we need to rename
 * @param	new_name	The new name of the contact
 */
void rename_contact(struct contact * contact, const char * new_name)
{
	LList *l;

	if(new_name == NULL)
		return;

	if(!strcmp(contact->nick, new_name))
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

	strncpy(contact->nick, new_name, sizeof(contact->nick)-1);
}

/**
 * Move a contact to a new group.  This takes care of calling ignore/unignore user
 * depending on group names
 * @param	contact		The contact that we need to move
 * @param	new_group	The group to move the contact to
 */
void move_contact(struct contact * contact, grouplist * new_group)
{
	LList *l;

	for(l=contact->accounts; l; l=l_list_next(l))
		handle_group_change(l->data, contact->group->name, new_group->name);

	contact->group->members = l_list_remove(contact->group->members, contact);
	contact->group = new_group;
	new_group->members = l_list_insert_sorted(new_group->members, contact, contact_cmp);
}

/**
 * Move an account to a new contact.  This takes care of calling ignore/unignore user
 * depending on group names
 * @param	account		The account that we need to move
 * @param	new_contact	The contact to move the account to
 */
void move_account(eb_account * account, struct contact * new_contact)
{
	struct contact * old_contact = account->account_contact;

	handle_group_change(account, old_contact->group->name, new_contact->group->name);

	old_contact->accounts = l_list_remove(old_contact->accounts, account);
	account->account_contact = new_contact;
	new_contact->accounts = l_list_append(new_contact->accounts, account);
}

/**
 * Returns a new LList of all group names in case insensitive sorted order
 * List must be freed by caller
 *
 * @return	a sorted list of group names
 */
LList * get_group_names( void )
{
	LList *g=NULL, *g2;
	for(g2 = groups; g2; g2=l_list_next(g2))
		g = l_list_insert_sorted(g, ((grouplist *)g2->data)->name, 
				(LListCompFunc)strcasecmp);

	return g;
}

/**
 * Returns a new LList of all contact names in a group in case insensitive sorted order
 * List must be freed by caller
 * @param	group		The group whose contacts are to be returned
 *
 * @return	a sorted list of contact names
 */
LList * get_group_contact_names( grouplist * group )
{
	LList *g=NULL, *g2;
	for(g2 = group->members; g2; g2=l_list_next(g2))
		g = l_list_insert_sorted(g, ((struct contact *)g2->data)->nick,
				(LListCompFunc)strcasecmp);

	return g;
}

/**
 * Writes the entire contact list to file
 * @param	fp	A file pointer to write to
 */
void write_contacts_to_fh(FILE *fp)
{
	LList *lg, *lc, *la;

	for(lg=groups; lg; lg=l_list_next(lg) ) {
		fprintf(fp,
				"<GROUP>\n"
				"\tNAME=\"%s\"\n",
				((grouplist*)lg->data)->name);
		for(lc = ((grouplist*)lg->data)->members; lc; lc=l_list_next(lc) ) {
			struct contact *c = lc->data;
			char *strbuf = NULL;
			fprintf(fp, 
					"\t<CONTACT>\n"
					"\t\tNAME=\"%s\"\n"
					"\t\tDEFAULT_PROTOCOL=\"%s\"\n"
					"\t\tLANGUAGE=\"%s\"\n",
					c->nick, eb_services[c->default_chatb].name, c->language);
			strbuf = escape_string(c->trigger.param);
			fprintf(fp, 
					"\t\tTRIGGER_TYPE=\"%s\"\n"
					"\t\tTRIGGER_ACTION=\"%s\"\n"
					"\t\tTRIGGER_PARAM=\"%s\"\n",
					get_trigger_type_text(c->trigger.type), 
					get_trigger_action_text(c->trigger.action),
					strbuf);
			free (strbuf);
			
			fprintf(fp, 
					"\t\tGPG_KEY=\"%s\"\n"
					"\t\tGPG_CRYPT=\"%d\"\n"
					"\t\tGPG_SIGN=\"%d\"\n"
					(c->gpg_key!=NULL)?c->gpg_key:"",
					c->gpg_do_encryption, c->gpg_do_signature);
			
			for(ga = c->accounts; ga; ga=l_list_next(ga)) {
				eb_account *ea = ga->data;
				fprintf( fp, "\t\t<ACCOUNT %s>\n"
						"\t\t\tNAME=\"%s\"\n"
						"\t\t\tLOCAL_ACCOUNT=\"%s\"\n"
					     "\t\t</ACCOUNT>\n",
					 eb_services[ea->service_id].name,
					 ea->handle,
					 ea->ela ? ea->ela->handle:"");	 

			}
			fprintf( fp, "\t</CONTACT>\n" );
		}
		fprintf( fp, "</GROUP>\n" );
	}
}

/**
 * Loads the contacts from file
 * @param	fp	A file pointer to read from
 */
void load_contacts_from_fh(FILE *fp)
{
	extern int contactparse();
	extern FILE *contactin;
	LList *cts = NULL;
	
	contactin = fp;

	contactparse();

	fclose(fp);
	
	if (groups)
		update_contact_list();	

	return 1;
}


