#ifndef __SPELLCHECK_H__
#define __SPELLCHECK_H__

#include "llist.h"

#ifdef __cplusplus
extern "C" {
#endif
	
int ay_spell_check(const char * word);
LList * ay_spell_check_suggest(const char * word);
void ay_spell_check_reload();

#ifdef __cplusplus
}
#endif

#endif
