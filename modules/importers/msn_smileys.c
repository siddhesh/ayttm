/*
 * MSN smileys plugin for Everybuddy
 * adapted from Alternate Smileys Plugin for Everybuddy
 *
 * Copyright (C) 2002, Philip S Tellis <philip@gnu.org.in>
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
 
unsigned int module_version() {return CORE_VERSION;}

#ifdef __MINGW32__
#define __IN_PLUGIN__ 1
#endif
#include "plugin_api.h"
#include "smileys.h"

/*************************************************************************************
 *                             Begin Module Code
 ************************************************************************************/
/*  Module defines */
#define plugin_info msn_smileys_LTX_plugin_info
#define plugin_init msn_smileys_LTX_plugin_init
#define plugin_finish msn_smileys_LTX_plugin_finish
#define module_version msn_smileys_LTX_module_version


/* Function Prototypes */
static void init_alt_smileys(void);
static void enable_smileys(ebmCallbackData *data);
int plugin_init();
int plugin_finish();

static int ref_count=0;
static LList *my_smileys=NULL;


/*  Module Exports */
PLUGIN_INFO plugin_info = {
	PLUGIN_SMILEY,
	"MSN Smileys",
	"Load MSN Messenger smiley theme",
	"$Revision: 1.4 $",
	"$Date: 2003/05/06 17:04:49 $",
	&ref_count,
	plugin_init,
	plugin_finish,
	NULL
};
/* End Module Exports */

static void *smiley_tag = NULL;

int plugin_init()
{
	printf("MSN smileys init\n");

	if(!my_smileys)
		init_alt_smileys();

	smiley_tag=eb_add_menu_item("MSN Smileys", EB_IMPORT_MENU, enable_smileys, ebmIMPORTDATA, NULL);
	if(!smiley_tag) {
		fprintf(stderr, "Error!  Unable to add MSN Smiley menu to import menu\n");
		return -1;
	}

	return 0;
}

int plugin_finish()
{
	int result = 0;

	if(smiley_tag)
		result = eb_remove_menu_item(EB_IMPORT_MENU, smiley_tag);
	/* 
	 * if we're using these smileys, then reset to 
	 * default to avoid references to freed memory 
	 * otherwise, leave it as it is
	 */
	if(smileys == my_smileys)
		smileys = eb_smileys();

	l_list_free(my_smileys);
	my_smileys=NULL;

	if(smiley_tag && result) {
		g_warning("Unable to remove MSN Smiley menu item from import menu!");
		return -1;
	}

	smiley_tag=NULL;

	return 0;
}

/*************************************************************************************
 *                             End Module Code
 ************************************************************************************/

#include "pixmaps/msn_rip/msn_angel.xpm"
#include "pixmaps/msn_rip/msn_angry.xpm"
#include "pixmaps/msn_rip/msn_bat.xpm"
#include "pixmaps/msn_rip/msn_beer.xpm"
#include "pixmaps/msn_rip/msn_boy.xpm"
#include "pixmaps/msn_rip/msn_brheart.xpm"
#include "pixmaps/msn_rip/msn_cake.xpm"
#include "pixmaps/msn_rip/msn_cat.xpm"
#include "pixmaps/msn_rip/msn_clock.xpm"
#include "pixmaps/msn_rip/msn_coffee.xpm"
#include "pixmaps/msn_rip/msn_cry.xpm"
#include "pixmaps/msn_rip/msn_deadflower.xpm"
#include "pixmaps/msn_rip/msn_devil.xpm"
#include "pixmaps/msn_rip/msn_dog.xpm"
#include "pixmaps/msn_rip/msn_drink.xpm"
#include "pixmaps/msn_rip/msn_email.xpm"
#include "pixmaps/msn_rip/msn_embarrassed.xpm"
#include "pixmaps/msn_rip/msn_film.xpm"
#include "pixmaps/msn_rip/msn_flower.xpm"
#include "pixmaps/msn_rip/msn_gift.xpm"
#include "pixmaps/msn_rip/msn_girl.xpm"
#include "pixmaps/msn_rip/msn_handcuffs.xpm"
#include "pixmaps/msn_rip/msn_heart.xpm"
#include "pixmaps/msn_rip/msn_hot.xpm"
#include "pixmaps/msn_rip/msn_icon.xpm"
#include "pixmaps/msn_rip/msn_idea.xpm"
#include "pixmaps/msn_rip/msn_kiss.xpm"
#include "pixmaps/msn_rip/msn_laugh.xpm"
#include "pixmaps/msn_rip/msn_neutral.xpm"
#include "pixmaps/msn_rip/msn_note.xpm"
#include "pixmaps/msn_rip/msn_ooooh.xpm"
#include "pixmaps/msn_rip/msn_phone.xpm"
#include "pixmaps/msn_rip/msn_photo.xpm"
#include "pixmaps/msn_rip/msn_question.xpm"
#include "pixmaps/msn_rip/msn_rainbow.xpm"
#include "pixmaps/msn_rip/msn_run.xpm"
#include "pixmaps/msn_rip/msn_runback.xpm"
#include "pixmaps/msn_rip/msn_sad.xpm"
#include "pixmaps/msn_rip/msn_sleep.xpm"
#include "pixmaps/msn_rip/msn_smiley.xpm"
#include "pixmaps/msn_rip/msn_star.xpm"
#include "pixmaps/msn_rip/msn_sun.xpm"
#include "pixmaps/msn_rip/msn_thumbdown.xpm"
#include "pixmaps/msn_rip/msn_thumbup.xpm"
#include "pixmaps/msn_rip/msn_tongue.xpm"
#include "pixmaps/msn_rip/msn_weird.xpm"
#include "pixmaps/msn_rip/msn_wink.xpm"

static void init_alt_smileys()
{
	my_smileys=add_smiley(my_smileys, "angel", msn_angel);
	my_smileys=add_smiley(my_smileys, "angry", msn_angry);
	my_smileys=add_smiley(my_smileys, "bat", msn_bat);
	my_smileys=add_smiley(my_smileys, "beer", msn_beer);
	my_smileys=add_smiley(my_smileys, "boy", msn_boy);
	my_smileys=add_smiley(my_smileys, "broken_heart", msn_brheart);
	my_smileys=add_smiley(my_smileys, "cake", msn_cake);
	my_smileys=add_smiley(my_smileys, "cat", msn_cat);
	my_smileys=add_smiley(my_smileys, "clock", msn_clock);
	my_smileys=add_smiley(my_smileys, "coffee", msn_coffee);
	my_smileys=add_smiley(my_smileys, "dead_flower", msn_deadflower);
	my_smileys=add_smiley(my_smileys, "devil", msn_devil);
	my_smileys=add_smiley(my_smileys, "dog", msn_dog);
	my_smileys=add_smiley(my_smileys, "wine", msn_drink);
	my_smileys=add_smiley(my_smileys, "letter", msn_email);
	my_smileys=add_smiley(my_smileys, "film", msn_film);
	my_smileys=add_smiley(my_smileys, "flower", msn_flower);
	my_smileys=add_smiley(my_smileys, "rose", msn_flower);
	my_smileys=add_smiley(my_smileys, "gift", msn_gift);
	my_smileys=add_smiley(my_smileys, "girl", msn_girl);
	my_smileys=add_smiley(my_smileys, "handcuffs", msn_handcuffs);
	my_smileys=add_smiley(my_smileys, "heart", msn_heart);
	my_smileys=add_smiley(my_smileys, "msn_icon", msn_icon);
	my_smileys=add_smiley(my_smileys, "lightbulb", msn_idea);
	my_smileys=add_smiley(my_smileys, "msn_icon", msn_icon);
	my_smileys=add_smiley(my_smileys, "kiss", msn_kiss);
	my_smileys=add_smiley(my_smileys, "phone", msn_phone);
	my_smileys=add_smiley(my_smileys, "camera", msn_photo);
	my_smileys=add_smiley(my_smileys, "a/s/l", msn_question);
	my_smileys=add_smiley(my_smileys, "rainbow", msn_rainbow);
	my_smileys=add_smiley(my_smileys, "boy_right", msn_run);
	my_smileys=add_smiley(my_smileys, "girl_left", msn_runback);
	my_smileys=add_smiley(my_smileys, "moon", msn_sleep);
	my_smileys=add_smiley(my_smileys, "zzz", msn_sleep);
	my_smileys=add_smiley(my_smileys, "star", msn_star);
	my_smileys=add_smiley(my_smileys, "sun", msn_sun);
	my_smileys=add_smiley(my_smileys, "no", msn_thumbdown);
	my_smileys=add_smiley(my_smileys, "yes", msn_thumbup);
	my_smileys=add_smiley(my_smileys, "confused", msn_weird);


	my_smileys=add_smiley(my_smileys, "wink", msn_wink);
	my_smileys=add_smiley(my_smileys, "tongue", msn_tongue);
	my_smileys=add_smiley(my_smileys, "smile", msn_smiley);
	my_smileys=add_smiley(my_smileys, "sad", msn_sad);
	my_smileys=add_smiley(my_smileys, "oh", msn_ooooh);
	my_smileys=add_smiley(my_smileys, "neutral", msn_neutral);
	my_smileys=add_smiley(my_smileys, "blank", msn_neutral);
	my_smileys=add_smiley(my_smileys, "note", msn_note);
        my_smileys=add_smiley(my_smileys, "laugh", msn_laugh);
        my_smileys=add_smiley(my_smileys, "biglaugh", msn_laugh);
	my_smileys=add_smiley(my_smileys, "cooldude", msn_hot);
	my_smileys=add_smiley(my_smileys, "blush", msn_embarrassed);
	my_smileys=add_smiley(my_smileys, "cry", msn_cry);
}

static void reset_smileys(ebmCallbackData *data)
{
	/* Set smileys back to default regardless of what they are now */
	smileys = eb_smileys();

	eb_remove_menu_item(EB_IMPORT_MENU, smiley_tag);
	smiley_tag=eb_add_menu_item("MSN Smileys", EB_IMPORT_MENU, enable_smileys, ebmIMPORTDATA, NULL);
}

static void enable_smileys(ebmCallbackData *data)
{
	if(!my_smileys)
		return;

	smileys = my_smileys;

	eb_remove_menu_item(EB_IMPORT_MENU, smiley_tag);
	smiley_tag=eb_add_menu_item("Default Smileys", EB_IMPORT_MENU, reset_smileys, ebmIMPORTDATA, NULL);
}

