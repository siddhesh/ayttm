#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "spellcheck.h"
#include <stddef.h>

#ifdef HAVE_LIBPSPELL

#include <pspell/pspell.h>
#include "prefs.h"
#include "globals.h"	// for DBG_CORE
#include "debug.h"

class AySpellChecker {
	PspellConfig *spell_config;
	PspellCanHaveError * possible_err;
	PspellManager * spell_checker;
	char * language;

	public:

	AySpellChecker();
	void reload();
	int check(const char * word);
	~AySpellChecker();
};


AySpellChecker::AySpellChecker()
{
	spell_config = new_pspell_config();
	reload();
}

void AySpellChecker::reload()
{
	if(!spell_config)
		spell_config = new_pspell_config();

	language = cGetLocalPref("spell_dictionary");
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

AySpellChecker::~AySpellChecker()
{
	if(spell_config)
		delete_pspell_config(spell_config);
}


static AySpellChecker speller;

int ay_spell_check(const char * word)
{
	return speller.check(word);
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

void ay_spell_check_reload()
{
}

#endif
