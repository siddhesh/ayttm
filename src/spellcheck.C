#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "spellcheck.h"
#include <stddef.h>

#ifdef HAVE_LIBPSPELL

#include <pspell/pspell.h>
#include <stdlib.h>
#include <string.h>
#include "prefs.h"
#include "globals.h"	// for DBG_CORE
#include "debug.h"

class AySpellChecker {
	PspellConfig *spell_config;
	PspellCanHaveError * possible_err;
	PspellManager * spell_checker;
	const char * language;

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

	while(env_lang[i] && !lang || !lang[0])
		lang = getenv(env_lang[i++]);

	if(!lang || !lang[0])
		lang = "en_GB";

	return lang;
}

AySpellChecker::AySpellChecker()
{
	spell_config = new_pspell_config();
	reload();
}

void AySpellChecker::reload()
{
	if(!spell_config)
		spell_config = new_pspell_config();

	language = get_language();
	pspell_config_replace(spell_config, "language-tag", language);
	possible_err = new_pspell_manager(spell_config);
	spell_checker=NULL;
	if(pspell_error_number(possible_err) != 0) {
		delete_pspell_config(spell_config);
		eb_debug(DBG_CORE, "%s", pspell_error_message(possible_err));
		spell_config=NULL;
	} else {
		spell_checker = to_pspell_manager(possible_err);
	}
}

int AySpellChecker::check(const char * word)
{
	if(!word || !spell_checker)
		return 1;
	else
		return pspell_manager_check(spell_checker, word, -1);
}

LList * AySpellChecker::suggest(const char * word)
{
	if(!word || !spell_checker)
		return NULL;

	LList * words = NULL;
	const char *w;

	const PspellWordList * suggestions = pspell_manager_suggest(spell_checker, word, -1);
	PspellStringEmulation * elements = pspell_word_list_elements(suggestions);
	while( (w = pspell_string_emulation_next(elements)) != NULL)
		words = l_list_append(words, strdup(w));
	delete_pspell_string_emulation(elements);

	return words;
}

AySpellChecker::~AySpellChecker()
{
	if(spell_checker)
		delete_pspell_manager(spell_checker);
	if(spell_config)
		delete_pspell_config(spell_config);
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
