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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#ifdef __MINGW32__
#define __IN_PLUGIN__ 1
#endif
#include "plugin_api.h"
#include "smileys.h"
#include "prefs.h"

/*******************************************************************************
 *                             Begin Module Code
 ******************************************************************************/
/*  Module defines */
#define plugin_info smileysc_LTX_plugin_info
#define module_version smileysc_LTX_module_version


/* Function Prototypes */
static void init_alt_smileys(void);
static void enable_smileys(ebmCallbackData *data);
static int plugin_init();
static int plugin_finish();

static int ref_count=0;
static LList *my_smileys=NULL;


/*  Module Exports */
PLUGIN_INFO plugin_info = {
	PLUGIN_SMILEY,
	"Console Smilies",
	"Provides the console smiley theme",
	"$Revision: 1.7 $",
	"$Date: 2003/05/06 17:04:49 $",
	&ref_count,
	plugin_init,
	plugin_finish,
	NULL
};
/* End Module Exports */

static void *smiley_tag = NULL;

unsigned int module_version() {return CORE_VERSION;}

static int plugin_init()
{
	eb_debug(DBG_MOD,"console smileys init\n");

	if(!my_smileys)
		init_alt_smileys();
  
	smiley_tag=eb_add_menu_item("Console Smilies", EB_IMPORT_MENU, enable_smileys, ebmIMPORTDATA, NULL);
	if(!smiley_tag) {
		eb_debug(DBG_MOD,"Error!  Unable to add Console Smiley menu to import menu\n");
		return -1;
	}

	ay_add_smiley_set( plugin_info.module_name, my_smileys );
	
	return 0;
}

static int plugin_finish()
{
	int result = 0;
	
	ay_remove_smiley_set( plugin_info.module_name );

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
		g_warning("Unable to remove Console Smiley menu item from import menu!");
		return -1;
	}

	smiley_tag=NULL;

	return 0;
}

/*******************************************************************************
 *                             End Module Code
 ******************************************************************************/

#include "pixmaps/console_smileys/blank.xpm"
#include "pixmaps/console_smileys/cooldude.xpm"
#include "pixmaps/console_smileys/grin.xpm"
#include "pixmaps/console_smileys/laugh.xpm"
#include "pixmaps/console_smileys/oh.xpm"
#include "pixmaps/console_smileys/sad.xpm"
#include "pixmaps/console_smileys/smile.xpm"
#include "pixmaps/console_smileys/tongue.xpm"
#include "pixmaps/console_smileys/wink.xpm"
#include "pixmaps/console_smileys/sob.xpm"
#include "pixmaps/console_smileys/confused.xpm"
#include "pixmaps/console_smileys/kiss.xpm"
#include "pixmaps/console_smileys/heyy.xpm"
#include "pixmaps/console_smileys/blush.xpm"
#include "pixmaps/console_smileys/worried.xpm"

static void init_alt_smileys()
{
	my_smileys=add_smiley(my_smileys, "smile", smile_xpm);
	my_smileys=add_smiley(my_smileys, "sad", sad_xpm);
	my_smileys=add_smiley(my_smileys, "wink", wink_xpm);
	my_smileys=add_smiley(my_smileys, "laugh", laugh_xpm);
	my_smileys=add_smiley(my_smileys, "cry", sob_xpm);
	my_smileys=add_smiley(my_smileys, "biglaugh", laugh_xpm);
	my_smileys=add_smiley(my_smileys, "tongue", tongue_xpm);
	my_smileys=add_smiley(my_smileys, "cooldude", cooldude_xpm);
	my_smileys=add_smiley(my_smileys, "worried", worried_xpm);
	my_smileys=add_smiley(my_smileys, "grin", grin_xpm);
	my_smileys=add_smiley(my_smileys, "blank", blank_xpm);
	my_smileys=add_smiley(my_smileys, "oh", oh_xpm);
	my_smileys=add_smiley(my_smileys, "confused", confused_xpm);
	my_smileys=add_smiley(my_smileys, "lovey", kiss_xpm);
	my_smileys=add_smiley(my_smileys, "kiss", kiss_xpm);
	my_smileys=add_smiley(my_smileys, "heyy", heyy_xpm);
	my_smileys=add_smiley(my_smileys, "blush", blush_xpm);
}

static void reset_smileys(ebmCallbackData *data)
{
	/* Set smileys back to default regardless of what they are now */
	smileys = eb_smileys();

	eb_remove_menu_item(EB_IMPORT_MENU, smiley_tag);
	smiley_tag=eb_add_menu_item("Console Smilies", EB_IMPORT_MENU, enable_smileys, ebmIMPORTDATA, NULL);
}

static void enable_smileys(ebmCallbackData *data)
{
	if(!my_smileys)
		return;

	smileys = my_smileys;

	eb_remove_menu_item(EB_IMPORT_MENU, smiley_tag);
	smiley_tag=eb_add_menu_item("Default Smilies", EB_IMPORT_MENU, reset_smileys, ebmIMPORTDATA, NULL);
}

