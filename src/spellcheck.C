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

#ifdef HAVE_LIBENCHANT

#include <enchant++.h>
#include <stdlib.h>
#include <string.h>
#include "prefs.h"
#include "globals.h"	// for DBG_CORE
#include "debug.h"

class AySpellChecker {
	enchant::Dict *spell_checker;
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
	const char * lang = cGetLocalPref("spell_dictionary");
	const char * env_lang[] = {"LC_LANG", "LC_ALL", "LANG", NULL};
	int i=0;

	while(env_lang[i] && (!lang || !lang[0])) {
		lang = getenv(env_lang[i]); 
		i++;
	}

	if(!lang || !lang[0])
		lang = "en_GB";

	return lang;
}

AySpellChecker::AySpellChecker() : spell_checker(NULL)
{
	reload();
}

void AySpellChecker::reload()
{
	language = get_language();
	delete spell_checker;
	spell_checker = enchant::Broker::instance()->request_dict(language);
}

int AySpellChecker::check(const char * word)
{
	if(!word || !spell_checker)
		return 1;
	else
		return spell_checker->check(word);
}

LList * AySpellChecker::suggest(const char * word)
{
	if(!word || !spell_checker)
		return NULL;

	std::vector<std::string> suggestions;
	spell_checker->suggest(word, suggestions);

	LList * words = NULL;
	std::vector<std::string>::iterator aEnd = suggestions.end();
	for (std::vector<std::string>::iterator aI = suggestions.begin(); aI != aEnd; ++aI)
		words = l_list_append(words, strdup(aI->c_str()));

	return words;
}

AySpellChecker::~AySpellChecker()
{
	delete spell_checker;
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
