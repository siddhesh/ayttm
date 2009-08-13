#ifndef _MSN_SB_H_
#define _MSN_SB_H_

#include "msn.h"

typedef void (*SBCallback) (MsnConnection *mc, int error, void *data);

struct _SBPayload {
	char *challenge;
	char *room;

	char *session_id;
	void *data;
	SBCallback callback;

	int num_members;
	int incoming;
} ;

void msn_connect_sb(MsnAccount *ma, const char *host, int port);

void msn_connect_sb_with_info(MsnConnection *mc, char *room_name, void *data);
void msn_get_sb(MsnAccount *ma, char *room_name, void *data, SBCallback callback);

void msn_got_sb_join(MsnConnection *mc);

void msn_sb_disconnect(MsnConnection *sb);
void msn_sb_disconnected(MsnConnection *sb);

#endif

