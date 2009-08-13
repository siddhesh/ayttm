
#ifndef _MSN_UTIL_H_
#define _MSN_UTIL_H_

#include <stdlib.h>
#include "llist.h"

#define m_new0(type,num) ((type *)calloc(num,sizeof(type)))
#define m_renew(type,ptr,newsize) ((type *)realloc(ptr,sizeof(type)*newsize))

extern char *ext_base64_encode(unsigned char *data, int len);
extern unsigned char *ext_base64_decode(char *data, int *len);

char *msn_urlencode(const char *in);
char *msn_urldecode(const char *in);

#endif

