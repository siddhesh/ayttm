/*
 * externs.h
 */

#ifndef __externs_h__
#define __externs_h__

#include "llist.h"

#if defined(__MINGW32__) && defined(__IN_PLUGIN__)
__declspec(dllimport) LList *outgoing_message_filters;
__declspec(dllimport) LList *incoming_message_filters;
__declspec(dllimport) LList *nick_modify_utility;
#else
extern LList *outgoing_message_filters;
extern LList *incoming_message_filters;
extern LList *nick_modify_utility;
#endif

#endif
