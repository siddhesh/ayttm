#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "msn-soap.h"
#include "msn-http.h"
#include "msn-contacts.h"
#include "msn-account.h"
#include "msn-util.h"
#include "msn-ext.h"

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

	snprintf(start_tag, sizeof(start_tag), "<%s>",tag);
	snprintf(end_tag, sizeof(end_tag), "</%s>",tag);

	*start = strstr(*start, start_tag);
	if (!*start) 
		return;

	*end = strstr(*start,end_tag);
	if (!*end) 
		return;

	*start += strlen(tag)+2;
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
	return (strcmp(((adl *)d1)->domain, ((adl *)d2)->domain) && ((adl *)d1)->type > ((adl *)d2)->type);
}


void msn_got_initial_adl_response(MsnConnection *mc)
{
	ext_msn_contacts_synced(mc->account);
}


void msn_got_initial_fqy_response(MsnConnection *mc)
{
	MsnMessage *msg = mc->current_message;
	char out[MAX_ADL_SIZE], bufsize[5];
	int offset = 0;
	char *domain = msg->payload;

	sprintf(out, "<ml l=\"1\">");
	offset = strlen(out);

	while(domain) {
		char *contact, *contact_end, *end;

		domain = strstr(domain, "<d ");

		if(!domain)
			break;

		end = strchr(domain, '>');
		end++;

		strncat(out, domain, end-domain);
		offset += end-domain;

		contact = end;
		contact_end = strstr(contact, "</d>");
		*contact_end = '\0';
		domain = contact_end+1;

		while(contact) {
			char *t;
			contact = strstr(contact, "c ");

			if(!contact)
				break;

			contact +=5;
			end = strchr(contact, '\"');
			*end = '\0';

			t=strstr(end+1, "t=\"");
			t += 3;
			end = strchr(t, '\"');
			*end = '\0';

			sprintf(out+offset, "<c n=\"%s\" l=\"3\" t=\"%s\"/>", contact, t);
			offset += strlen(out+offset);
			contact = end + 1;
		}

		strcat(out, "</d>");
		offset += 4;
	}

	strcat(out, "</ml>");

	sprintf(bufsize, "%d", offset+5);
	msn_message_send(mc, out, MSN_COMMAND_ADL, bufsize);

	msn_connection_push_callback(mc, msn_got_initial_adl_response);
}


/* This has become a convoluted piece of crap. Need to rework */
static void _send_adl(MsnAccount *ma)
{
	char buf[MAX_ADL_SIZE];
	char bufsize[5];
	LList *in = ma->buddies;
	LList *l = NULL;
	int count = 0;
	int init = 0;
	int offset = 0;
	char *cur_domain = NULL;
	int cur_type = 0;
	int calledback = 0;

	/* Sort the list */
	while(in) {
		unsigned int mask = 0;
		adl *newadl;

		MsnBuddy *b = (MsnBuddy *)in->data;

		mask = b->list & ~(MSN_BUDDY_REVERSE | MSN_BUDDY_PENDING);

		if(mask) {
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

	while(l) {
		adl *a = (adl *)l->data;

		if(!init) {
			init = 1;
			sprintf(buf, "<ml l=\"1\">");
			offset = strlen(buf);
		}

		if(!cur_domain) {
			snprintf(buf + offset, sizeof(buf) - offset, "<d n=\"%s\">", a->domain);
			cur_domain = a->domain;
			cur_type = a->type;
			offset += strlen(buf+offset);
		}

		if( (count < MAX_ADL_CONTACTS - 1 && !strcmp(cur_domain, a->domain)) ) {
			if(cur_type == MSN_BUDDY_PASSPORT)
				snprintf(buf + offset, sizeof(buf) - offset, 
					"<c n=\"%s\" l=\"%u\" t=\"1\"/>", a->name, a->mask);
			else
				snprintf(buf + offset, sizeof(buf) - offset, 
					"<c n=\"%s\"/>", a->name);
		}
		else if ( cur_type != a->type ) {
			snprintf(buf+offset, sizeof(buf)-offset, "</d></ml>");

			snprintf(bufsize, sizeof(bufsize), "%d", strlen(buf));

			if(cur_type == MSN_BUDDY_PASSPORT)
				msn_message_send(ma->ns_connection, buf, MSN_COMMAND_ADL, bufsize);
			else
				msn_message_send(ma->ns_connection, buf, MSN_COMMAND_FQY, bufsize);

			buf[0] = '\0';
			offset = 0;
			count = 0;

			snprintf(buf, sizeof(buf), "<ml><d n=\"%s\"><c n=\"%s\"/>", a->domain, a->name);

			cur_domain = a->domain;
			cur_type = a->type;
		}
		else {
			snprintf(buf, sizeof(buf), "</d><d n=\"%s\"><c n=\"%s\" l=\"%u\" t=\"1\"/>", a->domain, a->name, a->mask);

			cur_domain = a->domain;
			cur_type = a->type;
		}

		offset += strlen(buf+offset);


		count++;

		l = l_list_next(l);
	}

	if(count) {
		snprintf(buf+offset, sizeof(buf)-offset, "</d></ml>");
		snprintf(bufsize, sizeof(bufsize), "%d", strlen(buf));
		if(cur_type == MSN_BUDDY_PASSPORT)
			msn_message_send(ma->ns_connection, buf, MSN_COMMAND_ADL, bufsize);
		else
			msn_message_send(ma->ns_connection, buf, MSN_COMMAND_FQY, bufsize);
	}

	/* We want to know the response to the last ADL/FQY */
	if(cur_type == MSN_BUDDY_PASSPORT)
		msn_connection_push_callback(ma->ns_connection, msn_got_initial_adl_response);
	else
		msn_connection_push_callback(ma->ns_connection, msn_got_initial_fqy_response);
}


static void msn_ab_response(MsnAccount *ma, char *data, int len, void *cbdata)
{
	char *offset = data;

	char *chunk;
	char *blp_offset;

	/* Pick groups */
	_get_next_tag_chunk(&chunk, &offset, "groups");

	if(!chunk)
		return;

	while(chunk) {
		char *in_offset = chunk;

		/* The guid */
		_get_next_tag_chunk(&chunk, &in_offset, "groupId");
		if(!chunk)
			break;

		MsnGroup *newgrp = m_new0(MsnGroup, 1);

		newgrp->guid = strdup(chunk);

		/* Group name */
		_get_next_tag_chunk(&chunk, &in_offset, "name");
		if(!chunk)
			break;

		newgrp->name = strdup(chunk);

		ma->groups = l_list_append(ma->groups, newgrp);

		chunk = in_offset;
	}

	_get_next_tag_chunk(&chunk, &offset, "contacts");
	if(!chunk)
		return;

	/* HACK! Get BLP. We don't seem to need GTC anymore */
	blp_offset = strstr(chunk, "MSN.IM.BLP");
	if(!blp_offset) {
		ext_show_error(ma->ns_connection, "Invalid Address book data from server");
	}

	blp_offset = strstr(blp_offset, "<Value>");
	blp_offset += 7;
	ma->blp = blp_offset[0] - '0';

	msn_message_send(ma->ns_connection, NULL, MSN_COMMAND_BLP, ma->blp?MSN_PRIVACY_ALLOW:MSN_PRIVACY_BLOCK);

	while(chunk) {
		LList *l = NULL;
		MsnBuddy *cur_buddy = NULL;
		MsnGroup *cur_group = NULL;
		char *in_offset = chunk, *in_chunk = chunk;

		_get_next_tag_chunk(&in_chunk, &in_offset, "Contact");
		if(!in_chunk)
			break;

		/* So that we can get to the next tag easily */
		chunk = in_offset;

		in_offset = in_chunk;
		/* Check if this is an email messenger member */
		_get_next_tag_chunk(&in_chunk, &in_offset, "contactEmailType");

		if(in_chunk && !strcmp(in_chunk, "Messenger2")) {
			_get_next_tag_chunk(&in_chunk, &in_offset, "email");
			l = ma->buddies;

			while(l) {
				MsnBuddy *b = (MsnBuddy *)l->data;

				if(!strcmp(b->passport, in_chunk)) {
					cur_buddy = b;
					break;
				}

				l = l_list_next(l);
			}
		}

		/* Check if there is a group entry for this */
		_get_next_tag_chunk(&in_chunk, &in_offset, "guid");

		if(in_chunk) {
			l = ma->groups;

			while(l) {
				MsnGroup *g = (MsnGroup *)l->data;

				if(!strcmp(g->guid, in_chunk)) {
					cur_group = g;
					break;
				}

				l = l_list_next(l);
			}
		}

		/* Get the Passport Name */
		_get_next_tag_chunk(&in_chunk, &in_offset, "passportName");

		if(in_chunk) {
			l = ma->buddies;

			while(l) {
				MsnBuddy *b = (MsnBuddy *)l->data;

				if(!strcmp(b->passport, in_chunk)) {
					cur_buddy = b;
					break;
				}

				l = l_list_next(l);
			}
		}

		/* Get display name only if the buddy exists in our membership list */

		if(cur_buddy) {
			cur_buddy->group = cur_group;

			_get_next_tag_chunk(&in_chunk, &in_offset, "displayName");

			if(in_chunk)
				cur_buddy->friendlyname = strdup(in_chunk);
			else
				cur_buddy->friendlyname = strdup(cur_buddy->passport);

#if __DEBUG__
			printf("Got info for %s (Name :: %s, Group :: %s)\n", cur_buddy->passport, 
				cur_buddy->friendlyname, cur_buddy->group?cur_buddy->group->name:"(*None*)");
#endif
		}
	}

	_send_adl(ma);

	/* Send your friendly name */
	msn_message_send(ma->ns_connection, NULL, MSN_COMMAND_PRP, "MFN", ma->friendlyname);
}


void msn_download_address_book(MsnAccount *ma)
{
	char *url = strdup("https://contacts.msn.com/abservice/abservice.asmx");
	char *soap_action = "http://www.msn.com/webservices/AddressBook/ABFindAll";

	char *ab_request = msn_soap_build_request(MSN_CONTACT_LIST_REQUEST, ma->contact_ticket);	

	msn_http_request(ma, MSN_HTTP_POST, soap_action, url, ab_request, msn_ab_response, NULL, NULL);

	free(url);
	free(ab_request);
}


static void msn_membership_response(MsnAccount *ma, char *data, int len, void *cbdata)
{
	char *offset = data;
	MsnBuddyType cur_type = -1;

	while (offset) {
		char *chunk;

		/* Pick the list */
		_get_next_tag_chunk(&chunk, &offset, "MemberRole");
		if(!chunk)
			break;

		if(!strcmp(chunk, "Forward"))
			cur_type = MSN_BUDDY_FORWARD;
		else if(!strcmp(chunk, "Allow"))
			cur_type = MSN_BUDDY_ALLOW|MSN_BUDDY_FORWARD;
		else if(!strcmp(chunk, "Reverse"))
			cur_type = MSN_BUDDY_REVERSE;
		else if(!strcmp(chunk, "Block"))
			cur_type = MSN_BUDDY_BLOCK;
		else if(!strcmp(chunk, "Pending"))
			cur_type = MSN_BUDDY_PENDING;
		else
			break;	/* We don't really care about anything else */

		/* Now Pick members in the list */
		_get_next_tag_chunk(&chunk, &offset, "Members");
		if(!chunk)
			break;

		while(chunk) {
			int type = 0;
			char *in_offset = chunk;
			const char *nick_tag = NULL;
			MsnBuddy *bud = NULL;
			LList *l = ma->buddies;

			_get_next_tag_chunk(&chunk, &in_offset, "Type");
			if(!chunk)
				break;

			if( !strcmp(chunk, "Passport") ) {
				nick_tag = "PassportName";
				type = MSN_BUDDY_PASSPORT;
			}
			else if( !strcmp(chunk, "Email") ) {
				nick_tag = "Email";
				type = MSN_BUDDY_EMAIL;
			}

			_get_next_tag_chunk(&chunk, &in_offset, nick_tag);
			if(!chunk)
				break;

			/* If buddy already exists, update the type */
			while(l) {
				MsnBuddy *b = (MsnBuddy *)l->data;

				if(!strcmp(b->passport, chunk)) {
					bud = b;
					bud->list |= cur_type;
					break;
				}

				l = l_list_next(l);
			}

			if(!bud) {
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

		while(l) {
			printf("%s\t\t%d\n", ((MsnBuddy *)l->data)->passport, ((MsnBuddy *)l->data)->list);
			l = l_list_next(l);
		}
	}
#endif

	/* Download contact list. This will give us group info, friendlyname and nick */
	msn_download_address_book(ma);
}


void msn_sync_contacts(MsnAccount *ma)
{
	char *url = strdup("https://contacts.msn.com/abservice/SharingService.asmx");
	char *soap_action = "http://www.msn.com/webservices/AddressBook/FindMembership";

	char *ab_request = msn_soap_build_request(MSN_MEMBERSHIP_LIST_REQUEST, ma->contact_ticket);	

	msn_http_request(ma, MSN_HTTP_POST, soap_action, url, ab_request, msn_membership_response, NULL, NULL);

	free(url);
	free(ab_request);
}


/* Reject a Pending member */
static void msn_remove_pending_response(MsnAccount *ma, char *data, int len, void *cbdata)
{
	MsnBuddy *bud = cbdata;

	if(!strstr(data, "<DeleteMemberResponse")) {
		fprintf(stderr, "Removal failed\n");
		return;
	}

	ma->buddies = l_list_remove(ma->buddies, bud);
	free(bud);
}


static const char *PASSPORT_MEMBER_INFO =
	"<Member xsi:type=\"PassportMember\" "
			"xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">"
		"<Type>Passport</Type>"
		"<State>Accepted</State>"
		"<PassportName>%s</PassportName>"
	"</Member>";

static const char *EMAIL_MEMBER_INFO =
	"<Member xsi:type=\"EmailMember\">"
		"<Type>Email</Type>"
		"<State>Accepted</State>"
		"<Email>%s</Email>"
	"</Member>";


void msn_buddy_remove_pending(MsnAccount *ma, MsnBuddy *bud)
{
	char *url = strdup("https://contacts.msn.com/abservice/SharingService.asmx");
	char *soap_action = "http://www.msn.com/webservices/AddressBook/DeleteMember";

	char passport_tag[512];

	if(bud->type == MSN_BUDDY_PASSPORT)
		snprintf(passport_tag, sizeof(passport_tag), PASSPORT_MEMBER_INFO, bud->passport);
	else
		snprintf(passport_tag, sizeof(passport_tag), EMAIL_MEMBER_INFO, bud->passport);

	char *ab_request = msn_soap_build_request(MSN_MEMBERSHIP_LIST_MODIFY,
						"ContactMsgrAPI",
						ma->contact_ticket,
						"DeleteMember",
						"Pending",
						passport_tag,
						"DeleteMember"
						);

	msn_http_request(ma, MSN_HTTP_POST, soap_action, url, ab_request, msn_remove_pending_response, 
			"Connection: Keep-Alive\r\n"
			"Cache-Control: no-cache\r\n", bud);

	free(url);
	free(ab_request);
}


/* Add a contact */
void msn_buddy_allow_response(MsnAccount *ma, char *data, int len, void *cbdata)
{
	char *start = NULL, *offset = NULL;
	MsnBuddy *bud = cbdata;

	if((start = strstr(data, "<guid>"))) {
		start += 6;
		offset = strstr(start, "</guid>");
		if(offset) {
			LList *groups = ma->groups;
			*offset = '\0';
			while(groups) {
				MsnGroup *group = groups->data;

				if(!strcmp(group->guid, start)) {
					bud->group = group;
					break;
				}
				groups = l_list_next(groups);
			}
		}
	}

	if(offset) {
		bud->list &= ~MSN_BUDDY_PENDING;
		bud->list |= MSN_BUDDY_ALLOW;
		ext_buddy_added(ma, bud);
	}
}


static const char *PASSPORT_CONTACT_INFO =
	"<contactType>LivePending</contactType>"
	"<passportName>%s</passportName>"
	"<isMessengerUser>true</isMessengerUser>"
	"<MessengerMemberInfo>"
	        "<DisplayName>%s</DisplayName>"
	"</MessengerMemberInfo>";

static const char *EMAIL_CONTACT_INFO =
	"<emails>"
		"<ContactEmail>"
			"<contactEmailType>Messenger2</contactEmailType>"
			"<email>%s</email>"
			"<isMessengerEnabled>true</isMessengerEnabled>"
			"<Capability>0</Capability>"
			"<MessengerEnabledExternally>false</MessengerEnabledExternally>"
			"<propertiesChanged/>"
		"</ContactEmail>"
	"</emails>";


void msn_buddy_allow(MsnAccount *ma, MsnBuddy *bud)
{
	char *url = strdup("https://contacts.msn.com/abservice/abservice.asmx");
	char *soap_action = "http://www.msn.com/webservices/AddressBook/ABContactAdd";

	char passport_tag[512];

	if(bud->type == MSN_BUDDY_PASSPORT)
		snprintf(passport_tag, sizeof(passport_tag), PASSPORT_CONTACT_INFO, bud->passport, bud->friendlyname);
	else
		snprintf(passport_tag, sizeof(passport_tag), EMAIL_CONTACT_INFO, bud->passport);

	char *ab_request = msn_soap_build_request(MSN_CONTACT_ADD_REQUEST, 
						"ContactSave", 
						ma->contact_ticket,
						passport_tag,
						bud->friendlyname
						);

	msn_http_request(ma, MSN_HTTP_POST, soap_action, url, ab_request, msn_buddy_allow_response, NULL, bud);

	free(url);
	free(ab_request);
}


