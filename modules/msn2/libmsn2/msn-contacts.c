/*
 * Ayttm
 *
 * Copyright (C) 2009, the Ayttm team
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "msn-contacts.h"
#include "msn-ext.h"
#include "msn-soap.h"
#include "msn-http.h"
#include "msn-account.h"
#include "msn-util.h"

#define MSN_PRIVACY_ALLOW "AL"
#define MSN_PRIVACY_BLOCK "BL"
#define MAX_ADL_CONTACTS 150
/* Should not exceed 7500. Keeping a buffer of 100 more just in case */
#define MAX_ADL_SIZE 7600

#define __DEBUG__ 0

/* 
 * Real Programmers don't need xml parsers ;) 
 * Ok ok, this is on the TODO list
 */
static void _get_next_tag_chunk(char **start, char **end, const char *tag)
{
	char start_tag[64], end_tag[64];

	*start = *end;

	snprintf(start_tag, sizeof(start_tag), "<%s>", tag);
	snprintf(end_tag, sizeof(end_tag), "</%s>", tag);

	*start = strstr(*start, start_tag);
	if (!*start)
		return;

	*end = strstr(*start, end_tag);
	if (!*end)
		return;

	*start += strlen(tag) + 2;
	**end = '\0';
	(*end)++;
}

typedef struct {
	char *domain;
	char *name;
	unsigned int mask;
	int type;
} adl;

int _cmp_domains(const void *d1, const void *d2)
{
	return (strcmp(((adl *)d1)->domain, ((adl *)d2)->domain)
		&& ((adl *)d1)->type > ((adl *)d2)->type);
}

void msn_buddies_send_adl(MsnAccount *ma, LList *in, int initial, int add)
{
	char buf[MAX_ADL_SIZE];
	char bufsize[5];
	LList *l = NULL;
	int count = 0;
	int init = 0;
	int offset = 0;
	char *cur_domain = NULL;
	int cur_type = 0;

	/* Sort the list */
	while (in) {
		unsigned int mask = 0;
		adl *newadl;

		MsnBuddy *b = (MsnBuddy *)in->data;

		mask = b->list & ~(MSN_BUDDY_REVERSE | MSN_BUDDY_PENDING);

		if (mask) {
			char *domain = NULL;

			newadl = m_new0(adl, 1);

			domain = strchr(b->passport, '@');
			*domain = '\0';
			domain++;

			newadl->domain = strdup(domain);
			newadl->name = strdup(b->passport);
			newadl->type = b->type;
			newadl->mask = mask;

			*(domain - 1) = '@';

			l = l_list_insert_sorted(l, newadl, _cmp_domains);
		}

		in = l_list_next(in);
	}

	while (l) {
		adl *a = (adl *)l->data;

		if (!init) {
			init = 1;
			sprintf(buf, "<ml l=\"1\">");
			offset = strlen(buf);
		}

		if (!cur_domain) {
			snprintf(buf + offset, sizeof(buf) - offset,
				"<d n=\"%s\">", a->domain);
			cur_domain = a->domain;
			cur_type = a->type;
			offset += strlen(buf + offset);
		}

		if ((count < MAX_ADL_CONTACTS - 1
				&& !strcmp(cur_domain, a->domain))) {
			if (!initial || cur_type == MSN_BUDDY_PASSPORT)
				snprintf(buf + offset, sizeof(buf) - offset,
					"<c n=\"%s\" l=\"%u\" t=\"%d\"/>",
					a->name, a->mask, cur_type);
			else
				snprintf(buf + offset, sizeof(buf) - offset,
					"<c n=\"%s\"/>", a->name);
		} else if (cur_type != a->type) {
			snprintf(buf + offset, sizeof(buf) - offset,
				"</d></ml>");

			snprintf(bufsize, sizeof(bufsize), "%d",
				(int)strlen(buf));

			if (!initial || cur_type == MSN_BUDDY_PASSPORT)
				msn_message_send(ma->ns_connection, buf,
					MSN_COMMAND_ADL, bufsize);
			else
				msn_message_send(ma->ns_connection, buf,
					MSN_COMMAND_FQY, bufsize);

			buf[0] = '\0';
			offset = 0;
			count = 0;

			offset = snprintf(buf, sizeof(buf), "<ml><d n=\"%s\">",
				a->domain);

			cur_domain = a->domain;
			cur_type = a->type;

			if (!initial || cur_type == MSN_BUDDY_PASSPORT)
				snprintf(buf + offset, sizeof(buf) - offset,
					"<c n=\"%s\" l=\"%u\" t=\"%d\"/>",
					a->name, a->mask, cur_type);
			else
				snprintf(buf + offset, sizeof(buf) - offset,
					"<c n=\"%s\"/>", a->name);
		} else {
			offset +=
				snprintf(buf + offset, sizeof(buf) - offset,
				"</d><d n=\"%s\">", a->domain);

			if (!initial || cur_type == MSN_BUDDY_PASSPORT)
				snprintf(buf + offset, sizeof(buf) - offset,
					"<c n=\"%s\" l=\"%u\" t=\"%d\"/>",
					a->name, a->mask, cur_type);
			else
				snprintf(buf + offset, sizeof(buf) - offset,
					"<c n=\"%s\"/>", a->name);

			cur_domain = a->domain;
		}

		offset += strlen(buf + offset);

		count++;

		l = l_list_next(l);
	}

	if (count) {
		snprintf(buf + offset, sizeof(buf) - offset, "</d></ml>");
		snprintf(bufsize, sizeof(bufsize), "%d", (int)strlen(buf));
		if (!initial || cur_type == MSN_BUDDY_PASSPORT)
			msn_message_send(ma->ns_connection, buf,
				MSN_COMMAND_ADL, bufsize);
		else
			msn_message_send(ma->ns_connection, buf,
				MSN_COMMAND_FQY, bufsize);
	}

	if (initial)
		ext_msn_contacts_synced(ma);
}

static void msn_ab_response(MsnAccount *ma, char *data, int len, void *cbdata)
{
	char *offset = data;

	char *chunk;
	char *blp_offset;
	char *contact_enc;

	/* Pick groups */
	_get_next_tag_chunk(&chunk, &offset, "groups");

	if (!chunk)
		goto ret;

	while (chunk) {
		char *in_offset = chunk;

		/* The guid */
		_get_next_tag_chunk(&chunk, &in_offset, "groupId");
		if (!chunk)
			break;

		MsnGroup *newgrp = m_new0(MsnGroup, 1);

		newgrp->guid = strdup(chunk);

		/* Group name */
		_get_next_tag_chunk(&chunk, &in_offset, "name");
		if (!chunk)
			break;

		newgrp->name = strdup(chunk);

		ma->groups = l_list_append(ma->groups, newgrp);

		chunk = in_offset;
	}

	_get_next_tag_chunk(&chunk, &offset, "contacts");
	if (!chunk)
		goto ret;

	/* HACK! Get BLP. We don't seem to need GTC anymore */
	blp_offset = strstr(chunk, "MSN.IM.BLP");
	if (blp_offset) {
		blp_offset = strstr(blp_offset, "<Value>");
		blp_offset += 7;
		ma->blp = blp_offset[0] - '0';
	}

	msn_message_send(ma->ns_connection, NULL, MSN_COMMAND_BLP,
		ma->blp ? MSN_PRIVACY_ALLOW : MSN_PRIVACY_BLOCK);

	while (chunk) {
		LList *l = NULL;
		MsnBuddy *cur_buddy = NULL;
		MsnGroup *cur_group = NULL;
		char *in_offset = chunk, *in_chunk = chunk;
		char *contact_id = NULL;

		_get_next_tag_chunk(&in_chunk, &in_offset, "Contact");
		if (!in_chunk)
			break;

		/* So that we can get to the next tag easily */
		chunk = in_offset;

		in_offset = in_chunk;

		_get_next_tag_chunk(&in_chunk, &in_offset, "contactId");

		if (in_chunk)
			contact_id = in_chunk;

		/* Check if this is an email messenger member */
		_get_next_tag_chunk(&in_chunk, &in_offset, "contactEmailType");

		if (in_chunk && !strcmp(in_chunk, "Messenger2")) {
			_get_next_tag_chunk(&in_chunk, &in_offset, "email");
			l = ma->buddies;

			while (l) {
				MsnBuddy *b = (MsnBuddy *)l->data;

				if (!strcmp(b->passport, in_chunk)) {
					cur_buddy = b;
					break;
				}

				l = l_list_next(l);
			}
		}

		/* Check if there is a group entry for this */
		_get_next_tag_chunk(&in_chunk, &in_offset, "guid");

		if (in_chunk) {
			l = ma->groups;

			while (l) {
				MsnGroup *g = (MsnGroup *)l->data;

				if (!strcmp(g->guid, in_chunk)) {
					cur_group = g;
					break;
				}

				l = l_list_next(l);
			}
		}

		/* Get the Passport Name */
		_get_next_tag_chunk(&in_chunk, &in_offset, "passportName");

		if (in_chunk) {
			l = ma->buddies;

			while (l) {
				MsnBuddy *b = (MsnBuddy *)l->data;

				if (!strcmp(b->passport, in_chunk)) {
					cur_buddy = b;
					break;
				}

				l = l_list_next(l);
			}
		}

		/* Get display name only if the buddy exists in our membership list */

		if (cur_buddy) {
			cur_buddy->contact_id = strdup(contact_id);
			cur_buddy->group = cur_group;

			_get_next_tag_chunk(&in_chunk, &in_offset,
				"displayName");

			if (in_chunk)
				cur_buddy->friendlyname = strdup(in_chunk);
			else
				cur_buddy->friendlyname =
					strdup(cur_buddy->passport);

#if __DEBUG__
			printf("Got info for %s (Name :: %s, Group :: %s, Contact ID :: %s)\n", cur_buddy->passport, cur_buddy->friendlyname, cur_buddy->group ? cur_buddy->group->name : "(*None*)", cur_buddy->contact_id);
#endif
		}
	}

 ret:
	msn_buddies_send_adl(ma, ma->buddies, 1, 0);

	/* Send your friendly name */
	contact_enc = msn_urlencode(ma->friendlyname);

	msn_message_send(ma->ns_connection, NULL, MSN_COMMAND_PRP, "MFN",
		contact_enc);

	free(contact_enc);
}

void msn_download_address_book(MsnAccount *ma)
{
	char *url = strdup("https://contacts.msn.com/abservice/abservice.asmx");
	char *soap_action =
		"http://www.msn.com/webservices/AddressBook/ABFindAll";

	char *ab_request =
		msn_soap_build_request(MSN_CONTACT_LIST_REQUEST,
		ma->contact_ticket);

	msn_http_request(ma, MSN_HTTP_POST, soap_action, url, ab_request,
		msn_ab_response, NULL, NULL);

	free(url);
	free(ab_request);
}

static void msn_ab_create_response(MsnAccount *ma, char *data, int len,
	void *cbdata)
{
	/* Give it another shot */
	msn_sync_contacts(ma);
}

static void msn_membership_response(MsnAccount *ma, char *data, int len,
	void *cbdata)
{
	char *offset = data;
	MsnBuddyType cur_type = -1;

	/* Does not have an address book. Create one. */
	if (strstr(data, "ABDoesNotExist")) {
		char *url =
			strdup
			("https://contacts.msn.com/abservice/abservice.asmx");
		char *soap_action =
			"http://www.msn.com/webservices/AddressBook/ABAdd";

		char *ab_request =
			msn_soap_build_request(MSN_CREATE_ADDRESS_BOOK,
			ma->contact_ticket, ma->passport);

		msn_http_request(ma, MSN_HTTP_POST, soap_action, url,
			ab_request, msn_ab_create_response, NULL, NULL);

		free(url);
		free(ab_request);

		return;
	}

	while (offset) {
		char *chunk;

		/* Pick the list */
		_get_next_tag_chunk(&chunk, &offset, "MemberRole");
		if (!chunk)
			break;

#if __DEBUG__
		printf("Processing List :: %s\n", chunk);
#endif

		if (!strcmp(chunk, "Forward"))
			cur_type = MSN_BUDDY_FORWARD;
		else if (!strcmp(chunk, "Allow"))
			cur_type = MSN_BUDDY_ALLOW | MSN_BUDDY_FORWARD;
		else if (!strcmp(chunk, "Reverse"))
			cur_type = MSN_BUDDY_REVERSE;
		else if (!strcmp(chunk, "Block"))
			cur_type = MSN_BUDDY_BLOCK;
		else if (!strcmp(chunk, "Pending"))
			cur_type = MSN_BUDDY_PENDING;
		else
			continue;	/* We don't really care about anything else */

		/* Now Pick members in the list */
		_get_next_tag_chunk(&chunk, &offset, "Members");
		if (!chunk)
			continue;

		while (chunk) {
			int type = 0;
			char *in_offset = chunk;
			const char *nick_tag = NULL;
			MsnBuddy *bud = NULL;
			LList *l = ma->buddies;

			_get_next_tag_chunk(&chunk, &in_offset, "Type");
			if (!chunk)
				break;

			if (!strcmp(chunk, "Passport")) {
				nick_tag = "PassportName";
				type = MSN_BUDDY_PASSPORT;
			} else if (!strcmp(chunk, "Email")) {
				nick_tag = "Email";
				type = MSN_BUDDY_EMAIL;
			}

			_get_next_tag_chunk(&chunk, &in_offset, nick_tag);
			if (!chunk)
				break;

			/* If buddy already exists, update the type */
			while (l) {
				MsnBuddy *b = (MsnBuddy *)l->data;

				if (!strcmp(b->passport, chunk)) {
					bud = b;
					bud->list |= cur_type;
					break;
				}

				l = l_list_next(l);
			}

			if (!bud) {
				bud = m_new0(MsnBuddy, 1);
				bud->status = MSN_STATE_OFFLINE;
				bud->passport = strdup(chunk);
				bud->list = cur_type;
				bud->type = type;
				ma->buddies = l_list_append(ma->buddies, bud);
			}

			chunk = in_offset;
		}
	}

#if __DEBUG__
	{
		LList *l = ma->buddies;

		while (l) {
			printf("%s\t%d\t%d\n", ((MsnBuddy *)l->data)->passport,
				((MsnBuddy *)l->data)->type,
				((MsnBuddy *)l->data)->list);
			l = l_list_next(l);
		}
	}
#endif

	/* Download contact list. This will give us group info, friendlyname and nick */
	msn_download_address_book(ma);
}

void msn_sync_contacts(MsnAccount *ma)
{
	char *url =
		strdup
		("https://contacts.msn.com/abservice/SharingService.asmx");
	char *soap_action =
		"http://www.msn.com/webservices/AddressBook/FindMembership";

	char *ab_request =
		msn_soap_build_request(MSN_MEMBERSHIP_LIST_REQUEST,
		ma->contact_ticket);

	msn_http_request(ma, MSN_HTTP_POST, soap_action, url, ab_request,
		msn_membership_response, NULL, NULL);

	free(url);
	free(ab_request);
}

static int _is_passport_member(const char *email)
{
	char *offset = strchr(email, '@');

	if (!offset)
		return 0;

	offset++;

	/* TODO Add all possible msn domains here */
	if (!strncmp(offset, "hotmail", 7)
		|| !strncmp(offset, "msn", 3)
		|| !strncmp(offset, "live", 4)
		)
		return 1;
	else
		return 0;
}

/* Membership List Management */

static const char *PASSPORT_MEMBER_INFO =
	"<Member xsi:type=\"PassportMember\" "
	"xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">"
	"<Type>Passport</Type>"
	"<State>Accepted</State>" "<PassportName>%s</PassportName>" "</Member>";

static const char *EMAIL_MEMBER_INFO =
	"<Member xsi:type=\"EmailMember\">"
	"<Type>Email</Type>"
	"<State>Accepted</State>" "<Email>%s</Email>" "</Member>";

typedef void (*MsnMembershipCallback) (MsnAccount *ma, int error, void *data);

typedef struct {
	int add;
	const char *scenario;
	const char *list;
	void *data;
	MsnMembershipCallback callback;
} MsnMembershipData;

static void msn_membership_update_response(MsnAccount *ma, char *data, int len,
	void *cbdata)
{
	MsnMembershipData *mmbr = cbdata;
	char *offset = NULL;

	if (mmbr->add)
		offset = strstr(data, "<AddMemberResponse");
	else
		offset = strstr(data, "<DeleteMemberResponse");

	if (mmbr->callback)
		mmbr->callback(ma, offset ? 0 : 1, mmbr->data);

	free(mmbr);
}

static void msn_membership_list_update(MsnAccount *ma, MsnBuddy *bud,
	MsnMembershipData *data)
{
	const char *action;
	char soap_action[512];
	char *url =
		strdup
		("https://contacts.msn.com/abservice/SharingService.asmx");
	char passport_tag[512];

	if (data->add)
		action = "AddMember";
	else
		action = "DeleteMember";

	snprintf(soap_action, sizeof(soap_action),
		"http://www.msn.com/webservices/AddressBook/%s", action);

	if (bud->type == MSN_BUDDY_PASSPORT)
		snprintf(passport_tag, sizeof(passport_tag),
			PASSPORT_MEMBER_INFO, bud->passport);
	else
		snprintf(passport_tag, sizeof(passport_tag), EMAIL_MEMBER_INFO,
			bud->passport);

	char *ab_request = msn_soap_build_request(MSN_MEMBERSHIP_LIST_MODIFY,
		data->scenario,
		ma->contact_ticket,
		action,
		data->list,
		passport_tag,
		action);

	msn_http_request(ma, MSN_HTTP_POST, soap_action, url, ab_request,
		msn_membership_update_response, NULL, data);

	free(url);
	free(ab_request);
}

/* Remove from Pending list */
static void msn_remove_pending_response(MsnAccount *ma, int error, void *cbdata)
{
	MsnBuddy *bud = cbdata;

	if (error) {
		fprintf(stderr, "Removal failed\n");
		return;
	}

	ma->buddies = l_list_remove(ma->buddies, bud);
	free(bud);
}

void msn_buddy_remove_pending(MsnAccount *ma, MsnBuddy *bud)
{
	MsnMembershipData *mmbr = m_new0(MsnMembershipData, 1);

	mmbr->add = 0;
	mmbr->scenario = "ContactMsgrAPI";
	mmbr->list = "Pending";
	mmbr->callback = msn_remove_pending_response;
	mmbr->data = bud;

	msn_membership_list_update(ma, bud, mmbr);
}

/* Unblock a Buddy */
static void msn_unblock_response(MsnAccount *ma, int error, void *cbdata)
{
	MsnBuddy *bud = cbdata;

	ext_buddy_unblock_response(ma, error, bud);
	bud->list = MSN_BUDDY_ALLOW;
}

void msn_buddy_unblock(MsnAccount *ma, MsnBuddy *bud)
{
	MsnMembershipData *mmbr = m_new0(MsnMembershipData, 1);

	mmbr->add = 1;
	mmbr->scenario = "BlockUnblock";
	mmbr->list = "Allow";
	mmbr->callback = msn_unblock_response;
	mmbr->data = bud;

	msn_membership_list_update(ma, bud, mmbr);
}

/* Block a buddy */
static void msn_block_response(MsnAccount *ma, int error, void *cbdata)
{
	MsnBuddy *bud = cbdata;

	ext_buddy_block_response(ma, error, bud);
	bud->list = MSN_BUDDY_BLOCK;
}

void msn_buddy_block(MsnAccount *ma, MsnBuddy *bud)
{
	MsnMembershipData *mmbr = m_new0(MsnMembershipData, 1);

	mmbr->add = 1;
	mmbr->scenario = "BlockUnblock";
	mmbr->list = "Block";
	mmbr->callback = msn_block_response;
	mmbr->data = bud;

	msn_membership_list_update(ma, bud, mmbr);
}

/* Group Management */
typedef void (*GroupCallback) (MsnAccount *ma, MsnGroup *group, void *data);

typedef struct {
	char *name;
	GroupCallback callback;
	void *data;
} GroupCallbackData;

/* Add group */
static void msn_group_add_response(MsnAccount *ma, char *data, int len,
	void *cbdata)
{
	char *offset;
	char *end;
	GroupCallbackData *d = cbdata;
	MsnGroup *group = NULL;

	if ((offset = strstr(data, "<guid>"))) {
		group = m_new0(MsnGroup, 1);

		offset += 6;

		end = strstr(offset, "</guid>");
		*end = '\0';

		group->name = d->name;
		group->guid = strdup(offset);

		ma->groups = l_list_append(ma->groups, group);

	} else {
		offset = strstr(data, "<faultstring>");

		if (offset) {
			offset += 13;

			end = strstr(data, "</faultstring>");
			*end = '\0';

		}

		ext_group_add_failed(ma, d->name, offset);

		free(d->name);
	}

	if (d->callback)
		d->callback(ma, group, d->data);

	free(d);
}

static void msn_group_add_with_cb(MsnAccount *ma, const char *groupname,
	GroupCallback callback, void *data)
{
	GroupCallbackData *d = m_new0(GroupCallbackData, 1);

	char *url = strdup("https://contacts.msn.com/abservice/abservice.asmx");
	char *soap_action =
		"http://www.msn.com/webservices/AddressBook/ABGroupAdd";

	d->callback = callback;
	d->data = data;
	d->name = strdup(groupname);

	char *grp_request = msn_soap_build_request(MSN_GROUP_ADD_REQUEST,
		ma->contact_ticket,
		groupname);

	msn_http_request(ma, MSN_HTTP_POST, soap_action, url, grp_request,
		msn_group_add_response, NULL, d);

	free(url);
	free(grp_request);
}

void msn_group_add(MsnAccount *ma, const char *groupname)
{
	msn_group_add_with_cb(ma, groupname, NULL, NULL);
}

/* Delete group */
static void msn_group_del_response(MsnAccount *ma, char *data, int len,
	void *cbdata)
{
	MsnGroup *group = cbdata;
	LList *l = ma->buddies;

	if (!strstr(data, "<ABGroupDeleteResponse")) {
		fprintf(stderr, "Group Update failed. Response ::\n%s\n", data);
		return;
	}

	ma->groups = l_list_remove(ma->groups, group);

	while (l) {
		MsnBuddy *bud = l->data;
		if (bud->group == group)
			bud->group = NULL;

		l = l_list_next(l);
	}

	free(group->name);
	free(group->guid);
	free(group);
}

void msn_group_del(MsnAccount *ma, MsnGroup *group)
{
	char *url = strdup("https://contacts.msn.com/abservice/abservice.asmx");
	char *soap_action =
		"http://www.msn.com/webservices/AddressBook/ABGroupDelete";

	char *grp_request = msn_soap_build_request(MSN_GROUP_DEL_REQUEST,
		ma->contact_ticket,
		group->guid);

	msn_http_request(ma, MSN_HTTP_POST, soap_action, url, grp_request,
		msn_group_del_response, NULL, group);

	free(url);
	free(grp_request);
}

/* Modify Group */
static void msn_group_mod_response(MsnAccount *ma, char *data, int len,
	void *cbdata)
{
	if (!strstr(data, "<ABGroupUpdateResponse"))
		fprintf(stderr, "Group Update failed. Response ::\n%s\n", data);
}

void msn_group_mod(MsnAccount *ma, MsnGroup *group, const char *groupname)
{
	char *url = strdup("https://contacts.msn.com/abservice/abservice.asmx");
	char *soap_action =
		"http://www.msn.com/webservices/AddressBook/ABGroupUpdate";

	char *grp_request = msn_soap_build_request(MSN_GROUP_MOD_REQUEST,
		ma->contact_ticket,
		group->guid,
		groupname);

	free(group->name);
	group->name = strdup(groupname);

	msn_http_request(ma, MSN_HTTP_POST, soap_action, url, grp_request,
		msn_group_mod_response, NULL, group);

	free(url);
	free(grp_request);
}

/* Contact group management */
static void msn_group_buddy_remove_response(MsnAccount *ma, char *data, int len,
	void *cbdata)
{
	MsnBuddy *bud = cbdata;

	if (!strstr(data, "<ABGroupContactDeleteResponse"))
		ext_buddy_group_remove_failed(ma, bud, bud->group);
}

void msn_remove_buddy_from_group(MsnAccount *ma, MsnBuddy *bud)
{
	char *url = strdup("https://contacts.msn.com/abservice/abservice.asmx");
	char *soap_action =
		"http://www.msn.com/webservices/AddressBook/ABGroupContactDelete";

	char *ab_request = msn_soap_build_request(MSN_GROUP_CONTACT_REQUEST,
		ma->contact_ticket,
		"Delete",
		bud->group->guid,
		bud->contact_id,
		"Delete");

	msn_http_request(ma, MSN_HTTP_POST, soap_action, url, ab_request,
		msn_group_buddy_remove_response, NULL, bud);

	free(url);
	free(ab_request);
}

static void msn_group_buddy_add_response(MsnAccount *ma, char *data, int len,
	void *cbdata)
{
	MsnBuddy *bud = cbdata;

	if (!strstr(data, "<guid>"))
		ext_buddy_group_add_failed(ma, bud, bud->group);
}

void msn_add_buddy_to_group(MsnAccount *ma, MsnBuddy *bud, MsnGroup *newgrp)
{
	char *url = strdup("https://contacts.msn.com/abservice/abservice.asmx");
	char *soap_action =
		"http://www.msn.com/webservices/AddressBook/ABGroupContactAdd";

	char *ab_request = msn_soap_build_request(MSN_GROUP_CONTACT_REQUEST,
		ma->contact_ticket,
		"Add",
		newgrp->guid,
		bud->contact_id,
		"Add");

	bud->group = newgrp;

	msn_http_request(ma, MSN_HTTP_POST, soap_action, url, ab_request,
		msn_group_buddy_add_response, NULL, bud);

	free(url);
	free(ab_request);
}

void msn_change_buddy_group(MsnAccount *ma, MsnBuddy *bud, MsnGroup *newgrp)
{
	if (bud->group)
		msn_remove_buddy_from_group(ma, bud);
	msn_add_buddy_to_group(ma, bud, newgrp);
}

/* Add a contact */
static const char *PASSPORT_CONTACT_INFO =
	"<contactType>LivePending</contactType>"
	"<passportName>%s</passportName>"
	"<isMessengerUser>true</isMessengerUser>"
	"<MessengerMemberInfo>"
	"<DisplayName>%s</DisplayName>" "</MessengerMemberInfo>";

static const char *EMAIL_CONTACT_INFO =
	"<emails>"
	"<ContactEmail>"
	"<contactEmailType>Messenger2</contactEmailType>"
	"<email>%s</email>"
	"<isMessengerEnabled>true</isMessengerEnabled>"
	"<Capability>0</Capability>"
	"<MessengerEnabledExternally>false</MessengerEnabledExternally>"
	"<propertiesChanged/>" "</ContactEmail>" "</emails>";

static void msn_contact_add_response(MsnAccount *ma, char *data, int len,
	void *cbdata)
{
	char *start = NULL, *offset = NULL;
	MsnBuddy *bud = cbdata;

	if ((start = strstr(data, "<guid>"))) {
		start += 6;
		offset = strstr(start, "</guid>");
		if (offset) {
			LList *l = NULL;

			*offset = '\0';

			bud->contact_id = strdup(start);

			if (bud->group)
				msn_add_buddy_to_group(ma, bud, bud->group);

			bud->list &= ~MSN_BUDDY_PENDING;
			bud->list |= MSN_BUDDY_ALLOW;
			bud->list |= MSN_BUDDY_FORWARD;

			l = l_list_append(l, bud);

			ma->buddies = l_list_append(ma->buddies, bud);

			ext_buddy_added(ma, bud);
			msn_buddies_send_adl(ma, l, 0, 1);

			l_list_free(l);
		}
	}
}

static void msn_buddy_allow_response(MsnAccount *ma, int error, void *cbdata)
{
	/* TODO Check for validity of response */

	char *url = strdup("https://contacts.msn.com/abservice/abservice.asmx");
	char *soap_action =
		"http://www.msn.com/webservices/AddressBook/ABContactAdd";
	MsnBuddy *bud = cbdata;

	char passport_tag[512];

	if (bud->type == MSN_BUDDY_PASSPORT)
		snprintf(passport_tag, sizeof(passport_tag),
			PASSPORT_CONTACT_INFO, bud->passport,
			bud->friendlyname);
	else
		snprintf(passport_tag, sizeof(passport_tag), EMAIL_CONTACT_INFO,
			bud->passport);

	char *ab_request = msn_soap_build_request(MSN_CONTACT_ADD_REQUEST,
		"ContactSave",
		ma->contact_ticket,
		passport_tag,
		bud->friendlyname);

	msn_http_request(ma, MSN_HTTP_POST, soap_action, url, ab_request,
		msn_contact_add_response, NULL, bud);

	free(url);
	free(ab_request);
}

void msn_buddy_allow(MsnAccount *ma, MsnBuddy *bud)
{
	MsnMembershipData *mmbr;

	/* Why allow again? */
	if (bud->list & MSN_BUDDY_ALLOW)
		return;

	mmbr = m_new0(MsnMembershipData, 1);

	mmbr->add = 1;
	mmbr->scenario = "ContactMsgrAPI";
	mmbr->list = "Allow";
	mmbr->callback = msn_buddy_allow_response;
	mmbr->data = bud;

	msn_membership_list_update(ma, bud, mmbr);
}

static void msn_buddy_add_to_group(MsnAccount *ma, MsnGroup *group, void *data)
{
	MsnBuddy *bud = data;

	if (!group) {
		ext_buddy_add_failed(ma, bud->passport, bud->friendlyname);
		return;
	}

	bud->group = group;

	msn_buddy_allow(ma, bud);
}

void msn_buddy_add(MsnAccount *ma, const char *passport,
	const char *friendlyname, const char *grp)
{
	MsnGroup *group = NULL;
	LList *groups = ma->groups;

	MsnBuddy *buddy = m_new0(MsnBuddy, 1);

	if (_is_passport_member(passport))
		buddy->type = MSN_BUDDY_PASSPORT;
	else
		buddy->type = MSN_BUDDY_EMAIL;

	buddy->passport = strdup(passport);
	buddy->friendlyname = strdup(friendlyname);

	while (groups) {
		group = groups->data;

		if (!strcmp(grp, group->name))
			break;

		groups = l_list_next(groups);
	}

	if (!groups)
		msn_group_add_with_cb(ma, grp, msn_buddy_add_to_group, buddy);
	else
		msn_buddy_add_to_group(ma, group, buddy);
}

/* Remove buddy */
static void msn_buddy_rml(MsnAccount *ma, MsnBuddy *bud)
{
	char buf[255];
	int size = 0;
	char bufsize[4];
	char *sep = strchr(bud->passport, '@');

	*sep = '\0';
	size = snprintf(buf, sizeof(buf),
		"<ml><d n=\"%s\"><c n=\"%s\" t=\"%d\" l=\"%d\"/></d></ml>",
		sep + 1, bud->passport, 1,
		(bud->list & (MSN_BUDDY_ALLOW | MSN_BUDDY_FORWARD |
				MSN_BUDDY_BLOCK)));

	sprintf(bufsize, "%d", size);

	msn_message_send(ma->ns_connection, buf, MSN_COMMAND_RML, bufsize);
}

static void msn_contact_remove_response(MsnAccount *ma, char *data, int len,
	void *cbdata)
{
	MsnBuddy *bud = cbdata;
	char *offset;

	if ((offset = strstr(data, "<ABContactDeleteResponse"))) {
		msn_buddy_rml(ma, bud);
		ext_buddy_removed(ma, bud->passport);

		ma->buddies = l_list_remove(ma->buddies, bud);
		msn_buddy_free(bud);
	}
}

void msn_buddy_remove_response(MsnAccount *ma, int error, void *cbdata)
{
	/* TODO validate response */
	MsnBuddy *bud = cbdata;
	char *url = strdup("https://contacts.msn.com/abservice/abservice.asmx");
	char *soap_action =
		"http://www.msn.com/webservices/AddressBook/ABContactDelete";

	char *ab_request = msn_soap_build_request(MSN_CONTACT_DELETE_REQUEST,
		ma->contact_ticket,
		bud->contact_id);

	msn_http_request(ma, MSN_HTTP_POST, soap_action, url, ab_request,
		msn_contact_remove_response, NULL, bud);

	free(url);
	free(ab_request);
}

void msn_buddy_remove(MsnAccount *ma, MsnBuddy *bud)
{
	MsnMembershipData *mmbr = m_new0(MsnMembershipData, 1);

	mmbr->add = 0;
	mmbr->scenario = "ContactMsgrAPI";
	mmbr->list = "Allow";
	mmbr->callback = msn_buddy_remove_response;
	mmbr->data = bud;

	msn_membership_list_update(ma, bud, mmbr);
}

void msn_buddy_invite(MsnConnection *mc, const char *passport)
{
	msn_message_send(mc, NULL, MSN_COMMAND_CAL, passport);
}
