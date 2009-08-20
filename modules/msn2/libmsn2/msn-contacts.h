#ifndef _MSN_CONTACTS_H_
#define _MSN_CONTACTS_H_

#include "msn.h"

void msn_download_address_book(MsnAccount *ma);
void msn_sync_contacts(MsnAccount *ma);

void msn_buddy_add(MsnAccount *ma, const char *passport, const char *friendlyname, const char *grp);
void msn_buddy_allow(MsnAccount *ma, MsnBuddy *bud);
void msn_buddy_remove_pending(MsnAccount *ma, MsnBuddy *bud);

void msn_group_add(MsnAccount *ma, const char *groupname);

#endif
