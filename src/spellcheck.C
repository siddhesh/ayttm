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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "spellcheck.h"
#include <stddef.h>

#ifdef HAVE_LIBASPELL

#define USE_ORIGINAL_MANAGER_FUNCS
#include <aspell.h>
#undef USE_ORIGINAL_MANAGER_FUNCS
#include <stdlib.h>
#include <string.h>
#include "prefs.h"
#include "globals.h"	// for DBG_CORE
#include "debug.h"

class AySpellChecker {
	AspellConfig *spell_config;
	AspellCanHaveError *possible_err;
	AspellSpeller *spell_checker;
	const char *language;

	public:

	AySpellChecker();
	void reload();
	int check(const char * word);
	LList * suggest(const char * word);
	~AySpellChecker();
};


/*
 * Returns the selected language from the first of the following that succeed:
 *  - prefs file
 *  - LC_LANG environment variable
 *  - LC_ALL environment variable
 *  - LANG environment variable
 *  - en_GB
 * Do not free this memory
 */
static const char * get_language()
{
	char * lang = cGetLocalPref("spell_dictionary");
	char * env_lang[] = {"LC_LANG", "LC_ALL", "LANG", NULL};
	int i=0;

	while(env_lang[i] && (!lang || !lang[0])) {
		lang = getenv(env_lang[i]); 
		i++;
	}

	if(!lang || !lang[0])
		lang = "en_GB";

	return lang;
}

AySpellChecker::AySpellChecker()
{
	spell_config = new_aspell_config();
	reload();
}

void AySpellChecker::reload()
{
	if(!spell_config)
		spell_config = new_aspell_config();

	language = get_language();
	aspell_config_replace(spell_config, "lang", language);
	possible_err = new_aspell_speller(spell_config);
	spell_checker = NULL;
	if (aspell_error_number(possible_err)) {
		delete_aspell_config(spell_config);
		eb_debug(DBG_CORE, "%s", aspell_error_message(possible_err));
		spell_config = NULL;
	}
	else
		spell_checker = to_aspell_speller(possible_err);
}

int AySpellChecker::check(const char * word)
{
	if(!word || !spell_checker)
		return 1;
	else
		return aspell_speller_check(spell_checker, word, -1);
}

LList * AySpellChecker::suggest(const char * word)
{
	if(!word || !spell_checker)
		return NULL;

	LList * words = NULL;
	const char *w;

	const AspellWordList *suggestions = aspell_speller_suggest(spell_checker, word, -1);
	AspellStringEnumeration *elements = aspell_word_list_elements(suggestions);
	while ((w = aspell_string_enumeration_next(elements)))
		words = l_list_append(words, strdup(w));
	delete_aspell_string_enumeration(elements);

	return words;
}

AySpellChecker::~AySpellChecker()
{
	if(spell_checker)
		delete_aspell_speller(spell_checker);
	if(spell_config)
		delete_aspell_config(spell_config);
}


static AySpellChecker speller;

int ay_spell_check(const char * word)
{
	return speller.check(word);
}

LList * ay_spell_check_suggest(const char * word)
{
	return speller.suggest(word);
}

void ay_spell_check_reload()
{
	speller.reload();
}

#else

int ay_spell_check(const char * word)
{
	return 1;
}

LList * ay_spell_check_suggest(const char * word)
{
	return NULL;
}

void ay_spell_check_reload()
{
}

#endif
