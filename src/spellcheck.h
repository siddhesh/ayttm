/*
 * Ayttm 
 *
 * Copyright (C) 2003, the Ayttm team
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

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
