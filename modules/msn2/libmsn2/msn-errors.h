#ifndef _MSN_ERRORS_H_
#define _MSN_ERRORS_H_


#include "msn.h"

struct _MsnError {
	int error_num;
	const char *message;
	int fatal;
	int nsfatal;
};


enum {
	MSN_ERROR_CONNECTION_RESET=0x01,

	MSN_LOGIN_OK=0x1000,
	MSN_LOGIN_FAIL_PASSPORT,
	MSN_LOGIN_FAIL_SSO,
	MSN_LOGIN_FAIL_VER,
	MSN_LOGIN_FAIL_OTHER,
	MSN_MESSAGE_DELIVERY_FAILED=0x2000
};

const MsnError *msn_strerror(int error);

#endif

