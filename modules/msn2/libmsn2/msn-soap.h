
#ifndef _MSN_SOAP_H_
#define _MSN_SOAP_H_

extern const char *MSN_SOAP_LOGIN_REQUEST;
extern const char *MSN_MEMBERSHIP_LIST_REQUEST;
extern const char *MSN_CONTACT_LIST_REQUEST;


/* 
 * Provide values for the format specifiers in the skeleton
 * Returns a newly allocated string. Free it.
 */
char *msn_soap_build_request(const char *skel, ...);

#endif

