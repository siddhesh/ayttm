
/*
 * The general design is as follows:
 *
 * Since a single MSN account spawns multiple connections over
 * the lifecycle of a chat, the basic block is the MsnConnection.
 * Hence, the MsnConnection is what you will see being passed 
 * around most of the time. This is different from the other protocol
 * implementations.
 *
 * An MsnConnection belongs to a certain account and can be either a 
 * connection to the SwitchBoard, Name Service for an FTP transfer. 
 * Also, it can either be incoming or outgoing. since you can have an
 * incoming FTP connection as well.
 *
 * Each Connection also has an associated trid, which is incremented
 * each time a message is sent.
 */


#ifndef _MSN_ACCOUNT_H_
#define _MSN_ACCOUNT_H_

#include "msn.h"
#include "llist.h"

typedef enum {
	MSN_BUDDY_FORWARD	= 1,
	MSN_BUDDY_ALLOW		= 1 << 1,
	MSN_BUDDY_BLOCK		= 1 << 2,
	MSN_BUDDY_REVERSE	= 1 << 3,
	MSN_BUDDY_PENDING	= 1 << 4
} MsnBuddyList;

typedef enum {
	MSN_BUDDY_PASSPORT	= 1,
	MSN_BUDDY_EMAIL		= 1 << 1,
} MsnBuddyType;

struct _MsnBuddy {
	char *passport;
	char *friendlyname;
	int status;

	MsnGroup *group;

	MsnBuddyType type;
	MsnBuddyList list;

	MsnConnection *sb;
	int connecting;

	LList *mq;	/* Messages queued to send */

	void *ext_data;
};

struct _MsnGroup {
	char *guid;
	char *name;
};

struct _MsnIM {
	char *body;
	int bold;
	int italic;
	int underline;
	char *font;
	char *color;
	int typing;
} ;

struct _MsnAccount {
	char *passport;			/* The passport email address: foo@hotmail.com or bar@msn.com */
	char *friendlyname;		/* Friendly name: Foo Bar */
	char *password;			/* The password (duh) */

	void *ext_data;			/* Any external data you want to attach? */
	char *policy;			/* Cookie. Used for SSO */
	unsigned char *nonce;			/* Used for SSO */
	int nonce_len;			/* Length of the nonce */
	char *ticket;			/* Ticket for the messenger */
	unsigned char *secret;			/* Secret. Used for final USR */
	int secret_len;			/* Length of the secret */
	char *contact_ticket;		/* Used when you want to get and/or modify address book */

	MsnConnection *ns_connection;	/* The notification server connection */
	LList *connections;		/* All the connections this account is holding right now */

	LList *buddies;			/* List of buddies */
	LList *groups;			/* List of groups */

	char *cache_key;

	int blp;
	int status;			/* Messenger Status */
} ;

#define msn_account_new() (m_new0(MsnAccount, 1))

void msn_account_cancel_connect(MsnAccount *ma);
void msn_set_presence(MsnAccount *ma, int state);

#endif

