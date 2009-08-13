
#ifndef _MSN_HTTP_H_
#define _MSN_HTTP_H_

#include "msn.h"

typedef enum {
	MSN_HTTP_POST=1,
	MSN_HTTP_GET
} MsnRequestType;

typedef void (*MsnHttpCallback) (MsnAccount *ma, char *data, int len);

void msn_http_request(MsnAccount *ma, MsnRequestType type, char *soap_action, char *url, 
			char *request, MsnHttpCallback callback, char *params);

int msn_http_got_response(MsnConnection *mc, int len);


#endif

