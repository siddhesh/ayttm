#ifndef _MSN_ERRORS_H_
#define _MSN_ERRORS_H_

#include <stdio.h>
#include <string.h>

enum {
	MSN_ERROR_NS_CONNECTION_RESET=0x01,

	MSN_LOGIN_OK=0x100,
	MSN_LOGIN_FAIL_PASSPORT,
	MSN_LOGIN_FAIL_SSO,
	MSN_LOGIN_FAIL_VER,
	MSN_LOGIN_FAIL_OTHER,
	MSN_MESSAGE_DELIVERY_FAILED=0x200
};

char *msn_strerror(int error);

#endif

