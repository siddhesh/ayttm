/*
 * Giles Smileys Plugin for Everybuddy 
 *
 * Copyright (C) 2002, Philip S Tellis <philip@gnu.org.in>
 * Pixmaps Copyright (C) 2002, Giles Hamlin <hamlin@clara.net>
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
#include <glib.h>
#ifdef __MINGW32__
#define __IN_PLUGIN__ 1
#endif
#include "plugin_api.h"
#include "smileys.h"
#include "prefs.h"

/*************************************************************************************
 *                             Begin Module Code
 ************************************************************************************/
/*  Module defines */
#define plugin_info giles_smiles_LTX_plugin_info
#define plugin_init giles_smiles_LTX_plugin_init
#define plugin_finish giles_smiles_LTX_plugin_finish
#define module_version giles_smiles_LTX_module_version


/* Function Prototypes */
static void init_alt_smileys(void);
static void enable_smileys(ebmCallbackData *data);
int plugin_init();
int plugin_finish();

static int ref_count=0;
static LList *my_smileys=NULL;


/*  Module Exports */
PLUGIN_INFO plugin_info = {
	PLUGIN_UTILITY,
	"Giles Smileys",
	"Load Giles smileys",
	"$Revision: 1.4 $",
	"$Date: 2003/04/29 08:32:00 $",
	&ref_count,
	plugin_init,
	plugin_finish,
	NULL
};
/* End Module Exports */

static void *smiley_tag = NULL;

int plugin_init()
{
	eb_debug(DBG_MOD,"giles smileys init\n");

	if(!my_smileys)
		init_alt_smileys();
  
	smiley_tag=eb_add_menu_item("Giles Smileys", EB_IMPORT_MENU, enable_smileys, ebmIMPORTDATA, NULL);
	if(!smiley_tag) {
		eb_debug(DBG_MOD,"Error!  Unable to add Giles Smiley menu to import menu\n");
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
		g_warning("Unable to remove Giles Smiley menu item from import menu!");
		return -1;
	}

	smiley_tag=NULL;

	return 0;
}

/*************************************************************************************
 *                             End Module Code
 ************************************************************************************/

#include "pixmaps/standard2/dead20.xpm"
#include "pixmaps/standard2/embarrased20.xpm"
#include "pixmaps/standard2/grin20.xpm"
#include "pixmaps/standard2/indifferent20.xpm"
#include "pixmaps/standard2/kiss20.xpm"
#include "pixmaps/standard2/quiet20.xpm"
#include "pixmaps/standard2/sad20.xpm"
#include "pixmaps/standard2/shades20.xpm"
#include "pixmaps/standard2/shocked20.xpm"
#include "pixmaps/standard2/smile20.xpm"
#include "pixmaps/standard2/tears20.xpm"
#include "pixmaps/standard2/tongue20.xpm"
#include "pixmaps/standard2/wink20.xpm"

static void init_alt_smileys()
{
	my_smileys=add_smiley(my_smileys, "dead", dead20_xpm);
	my_smileys=add_smiley(my_smileys, "blank", embarrased20_xpm);
	my_smileys=add_smiley(my_smileys, "blush", embarrased20_xpm);
	my_smileys=add_smiley(my_smileys, "grin", grin20_xpm);
	my_smileys=add_smiley(my_smileys, "worried", indifferent20_xpm);
	my_smileys=add_smiley(my_smileys, "lovey", kiss20_xpm);
	my_smileys=add_smiley(my_smileys, "quiet", quiet20_xpm);
	my_smileys=add_smiley(my_smileys, "sad", sad20_xpm);
	my_smileys=add_smiley(my_smileys, "cooldude", shades20_xpm);
	my_smileys=add_smiley(my_smileys, "oh", shocked20_xpm);
	my_smileys=add_smiley(my_smileys, "smile", smile20_xpm);
	my_smileys=add_smiley(my_smileys, "sob", tears20_xpm);
	my_smileys=add_smiley(my_smileys, "tongue", tongue20_xpm);
	my_smileys=add_smiley(my_smileys, "wink", wink20_xpm);
}

static void reset_smileys(ebmCallbackData *data)
{
	/* Set smileys back to default regardless of what they are now */
	smileys = eb_smileys();

	eb_remove_menu_item(EB_IMPORT_MENU, smiley_tag);
	smiley_tag=eb_add_menu_item("Giles Smileys", EB_IMPORT_MENU, enable_smileys, ebmIMPORTDATA, NULL);
}

static void enable_smileys(ebmCallbackData *data)
{
	if(!my_smileys)
		return;

	smileys = my_smileys;

	eb_remove_menu_item(EB_IMPORT_MENU, smiley_tag);
	smiley_tag=eb_add_menu_item("Default Smileys", EB_IMPORT_MENU, reset_smileys, ebmIMPORTDATA, NULL);
}

