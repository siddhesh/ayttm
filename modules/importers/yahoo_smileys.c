/*
 * Alternate Smileys Plugin for Everybuddy 
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
#define plugin_info yahoo_smileys_LTX_plugin_info
#define plugin_init yahoo_smileys_LTX_plugin_init
#define plugin_finish yahoo_smileys_LTX_plugin_finish
#define module_version yahoo_smileys_LTX_module_version


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
	"Yahoo Smileys",
	"Load Yahoo smiley theme",
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
	printf("yahoo smileys init\n");

	if(!my_smileys)
		init_alt_smileys();
  
	smiley_tag=eb_add_menu_item("Yahoo Smileys", EB_IMPORT_MENU, enable_smileys, ebmIMPORTDATA, NULL);
	if(!smiley_tag) {
		fprintf(stderr, "Error!  Unable to add Yahoo Smiley menu to import menu\n");
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
		g_warning("Unable to remove Yahoo Smiley menu item from import menu!");
		return -1;
	}

	smiley_tag=NULL;

	return 0;
}

/*************************************************************************************
 *                             End Module Code
 ************************************************************************************/

#include "pixmaps/yahoo_rip/yahoo_alien.xpm"
#include "pixmaps/yahoo_rip/yahoo_angel.xpm"
#include "pixmaps/yahoo_rip/yahoo_angry.xpm"
#include "pixmaps/yahoo_rip/yahoo_bigcry.xpm"
#include "pixmaps/yahoo_rip/yahoo_biglaugh.xpm"
#include "pixmaps/yahoo_rip/yahoo_blush.xpm"
#include "pixmaps/yahoo_rip/yahoo_clown.xpm"
#include "pixmaps/yahoo_rip/yahoo_confused.xpm"
#include "pixmaps/yahoo_rip/yahoo_cooldude.xpm"
#include "pixmaps/yahoo_rip/yahoo_cow.xpm"
#include "pixmaps/yahoo_rip/yahoo_devil.xpm"
#include "pixmaps/yahoo_rip/yahoo_glasses.xpm"
#include "pixmaps/yahoo_rip/yahoo_grin.xpm"
#include "pixmaps/yahoo_rip/yahoo_heart.xpm"
#include "pixmaps/yahoo_rip/yahoo_oh.xpm"
#include "pixmaps/yahoo_rip/yahoo_rose.xpm"
#include "pixmaps/yahoo_rip/yahoo_sad.xpm"
#include "pixmaps/yahoo_rip/yahoo_skull.xpm"
#include "pixmaps/yahoo_rip/yahoo_smile.xpm"
#include "pixmaps/yahoo_rip/yahoo_tongue.xpm"
#include "pixmaps/yahoo_rip/yahoo_undecided.xpm"
#include "pixmaps/yahoo_rip/yahoo_vicious.xpm"
#include "pixmaps/yahoo_rip/yahoo_stop.xpm"
#include "pixmaps/yahoo_rip/yahoo_wink.xpm"
#include "pixmaps/yahoo_rip/yahoo_zzz.xpm"
#include "pixmaps/yahoo_rip/yahoo_cowboy.xpm"
#include "pixmaps/yahoo_rip/yahoo_flying.xpm"
#include "pixmaps/yahoo_rip/yahoo_lightbulb.xpm"
#include "pixmaps/yahoo_rip/yahoo_monkey.xpm"
#include "pixmaps/yahoo_rip/yahoo_pig.xpm"
#include "pixmaps/yahoo_rip/yahoo_pumkin.xpm"
#include "pixmaps/yahoo_rip/yahoo_shhh.xpm"
#include "pixmaps/yahoo_rip/yahoo_sick.xpm"
#include "pixmaps/yahoo_rip/yahoo_us_flag.xpm"
#include "pixmaps/yahoo_rip/yahoo_eyelash.xpm"
#include "pixmaps/yahoo_rip/yahoo_applause.xpm"
#include "pixmaps/yahoo_rip/yahoo_doh.xpm"
#include "pixmaps/yahoo_rip/yahoo_drool.xpm"
#include "pixmaps/yahoo_rip/yahoo_eyebrow.xpm"
#include "pixmaps/yahoo_rip/yahoo_kiss.xpm"
#include "pixmaps/yahoo_rip/yahoo_rolling_eyes.xpm"
#include "pixmaps/yahoo_rip/yahoo_silly.xpm"
#include "pixmaps/yahoo_rip/yahoo_thinking.xpm"
#include "pixmaps/yahoo_rip/yahoo_tired.xpm"
#include "pixmaps/yahoo_rip/yahoo_worried.xpm"

static void init_alt_smileys()
{
	my_smileys=add_smiley(my_smileys, "alien", yahoo_alien_xpm);
	my_smileys=add_smiley(my_smileys, "angel", yahoo_angel_xpm);
	my_smileys=add_smiley(my_smileys, "angry", yahoo_angry_xpm);
	my_smileys=add_smiley(my_smileys, "cry", yahoo_bigcry_xpm);
	my_smileys=add_smiley(my_smileys, "biglaugh", yahoo_biglaugh_xpm);
	my_smileys=add_smiley(my_smileys, "blush", yahoo_blush_xpm);
	my_smileys=add_smiley(my_smileys, "clown", yahoo_clown_xpm);
	my_smileys=add_smiley(my_smileys, "confused", yahoo_confused_xpm);
	my_smileys=add_smiley(my_smileys, "cooldude", yahoo_cooldude_xpm);
	my_smileys=add_smiley(my_smileys, "cow", yahoo_cow_xpm);
	my_smileys=add_smiley(my_smileys, "devil", yahoo_devil_xpm);
	my_smileys=add_smiley(my_smileys, "glasses", yahoo_glasses_xpm);
	my_smileys=add_smiley(my_smileys, "grin", yahoo_grin_xpm);
	my_smileys=add_smiley(my_smileys, "eyelash", yahoo_eyelash_xpm);
	my_smileys=add_smiley(my_smileys, "kiss", yahoo_kiss_xpm);
	my_smileys=add_smiley(my_smileys, "heart", yahoo_heart_xpm);
	my_smileys=add_smiley(my_smileys, "oh", yahoo_oh_xpm);
	my_smileys=add_smiley(my_smileys, "rose", yahoo_rose_xpm);
	my_smileys=add_smiley(my_smileys, "flower", yahoo_rose_xpm);
	my_smileys=add_smiley(my_smileys, "sad", yahoo_sad_xpm);
	my_smileys=add_smiley(my_smileys, "skull", yahoo_skull_xpm);
	my_smileys=add_smiley(my_smileys, "smile", yahoo_smile_xpm);
	my_smileys=add_smiley(my_smileys, "tongue", yahoo_tongue_xpm);
	my_smileys=add_smiley(my_smileys, "blank", yahoo_undecided_xpm);
	my_smileys=add_smiley(my_smileys, "neutral", yahoo_undecided_xpm);
	my_smileys=add_smiley(my_smileys, "heyyy", yahoo_vicious_xpm);
	my_smileys=add_smiley(my_smileys, "stop", yahoo_stop_xpm);
	my_smileys=add_smiley(my_smileys, "wink", yahoo_wink_xpm);
	my_smileys=add_smiley(my_smileys, "moon", yahoo_zzz_xpm);
	my_smileys=add_smiley(my_smileys, "zzz", yahoo_zzz_xpm);
	my_smileys=add_smiley(my_smileys, "thinking", yahoo_thinking_xpm);
	my_smileys=add_smiley(my_smileys, "tired", yahoo_tired_xpm);
	my_smileys=add_smiley(my_smileys, "worried", yahoo_worried_xpm);
	my_smileys=add_smiley(my_smileys, "silly", yahoo_silly_xpm);
	my_smileys=add_smiley(my_smileys, "rolling_eyes", yahoo_rolling_eyes_xpm);
	my_smileys=add_smiley(my_smileys, "eyebrow", yahoo_eyebrow_xpm);
	my_smileys=add_smiley(my_smileys, "drool", yahoo_drool_xpm);
	my_smileys=add_smiley(my_smileys, "doh", yahoo_doh_xpm);
	my_smileys=add_smiley(my_smileys, "applause", yahoo_applause_xpm);
	my_smileys=add_smiley(my_smileys, "cowboy", yahoo_cowboy_xpm);
	my_smileys=add_smiley(my_smileys, "flying", yahoo_flying_xpm);
	my_smileys=add_smiley(my_smileys, "lightbulb", yahoo_lightbulb_xpm);
	my_smileys=add_smiley(my_smileys, "monkey", yahoo_monkey_xpm);
	my_smileys=add_smiley(my_smileys, "pig", yahoo_pig_xpm);
	my_smileys=add_smiley(my_smileys, "shh", yahoo_shhh_xpm);
	my_smileys=add_smiley(my_smileys, "sick", yahoo_sick_xpm);
	my_smileys=add_smiley(my_smileys, "pumkin", yahoo_pumkin_xpm);
	my_smileys=add_smiley(my_smileys, "usflag", yahoo_us_flag_xpm);
}

static void reset_smileys(ebmCallbackData *data)
{
	/* Set smileys back to default regardless of what they are now */
	smileys = eb_smileys();

	eb_remove_menu_item(EB_IMPORT_MENU, smiley_tag);
	smiley_tag=eb_add_menu_item("Yahoo Smileys", EB_IMPORT_MENU, enable_smileys, ebmIMPORTDATA, NULL);
}

static void enable_smileys(ebmCallbackData *data)
{
	if(!my_smileys)
		return;

	smileys = my_smileys;

	eb_remove_menu_item(EB_IMPORT_MENU, smiley_tag);
	smiley_tag=eb_add_menu_item("Default Smileys", EB_IMPORT_MENU, reset_smileys, ebmIMPORTDATA, NULL);
}

