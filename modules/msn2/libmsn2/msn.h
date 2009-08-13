#ifndef _MSN_H_
#define _MSN_H_

typedef struct _MsnAccount	MsnAccount;
typedef struct _MsnMessage	MsnMessage;
typedef struct _MsnConnection	MsnConnection;
typedef struct _MsnIM		MsnIM;
typedef struct _MsnBuddy	MsnBuddy;
typedef struct _MsnGroup	MsnGroup;

typedef struct _SBPayload	SBPayload;

void msn_login(MsnAccount *ma) ;
void msn_logout(MsnAccount *ma);

void msn_invite_buddy(MsnAccount *ma, MsnBuddy *buddy, const char *message);

void msn_get_contacts(MsnAccount *ma);
int  msn_get_status_num(const char *state);
void msn_send_chl_response(MsnAccount *ma, const char *seed);
void msn_set_initial_presence(MsnAccount *ma, int state);

void msn_set_state(MsnAccount *ma, int state);

typedef enum {
	MSN_STATE_ONLINE,
	MSN_STATE_HIDDEN,
	MSN_STATE_BUSY,
	MSN_STATE_IDLE,
	MSN_STATE_BRB,
	MSN_STATE_AWAY,
	MSN_STATE_ON_PHONE,
	MSN_STATE_LUNCH,
	MSN_STATE_OFFLINE,
	MSN_STATES_COUNT
} MsnState;

extern const char *msn_state_strings[];

#include "msn-login.h"
#include "msn-contacts.h"
#include "msn-errors.h"
#include "msn-util.h"

#endif
