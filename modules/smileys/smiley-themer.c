/*
 * Smiley Themer Plugin for Everybuddy 
 *
 * Copyright (C) 2003, Philip S Tellis <bluesmoon @ users.sourceforge.net>
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

#include "intl.h"
#ifdef __MINGW32__
#define __IN_PLUGIN__ 1
#endif
#include "plugin_api.h"
#include "smileys.h"
#include "prefs.h"

#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>

#include <X11/xpm.h>


/*******************************************************************************
 *                             Begin Module Code
 ******************************************************************************/
/*  Module defines */
#define plugin_info smiley_themer_LTX_plugin_info
#define module_version smiley_themer_LTX_module_version


static int plugin_init();
static int plugin_finish();

static int ref_count=0;
static char smiley_directory[MAX_PREF_LEN]=AYTTM_SMILEY_DIR;
static char last_selected[MAX_PREF_LEN]="";

static char rcfilename[]="aysmile.rc";


/*  Module Exports */
PLUGIN_INFO plugin_info = {
	PLUGIN_SMILEY,
	"Smiley Themes",
	"Loads smiley themes from disk at run time",
	"$Revision: 1.1 $",
	"$Date: 2003/05/08 16:17:05 $",
	&ref_count,
	plugin_init,
	plugin_finish,
	NULL
};
/* End Module Exports */

unsigned int module_version() {return CORE_VERSION;}

static void load_themes();
static void unload_themes();
static void enable_smileys(ebmCallbackData *data);

static int plugin_init()
{
	input_list *il;

	if(!smiley_directory || !smiley_directory[0])
		return -1;

	il = g_new0(input_list, 1);
	ref_count = 0;
	plugin_info.prefs = il;
	il->widget.entry.value = smiley_directory;
	il->widget.entry.name = "smiley_directory";
	il->widget.entry.label= _("Smiley Directory:");
	il->type = EB_INPUT_ENTRY;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.entry.value = last_selected;
	il->widget.entry.name = "last_selected";
	il->widget.entry.label= _("Last Selected:");
	il->type = EB_INPUT_ENTRY;

	load_themes();

	return 0;
}

static int plugin_finish()
{
	while(plugin_info.prefs) {
		input_list *il = plugin_info.prefs->next;
		g_free(plugin_info.prefs);
		plugin_info.prefs = il;
	}

	unload_themes();

	return 0;
}

/*******************************************************************************
 *                             End Module Code
 ******************************************************************************/

struct smiley_theme {
	char *name;
	LList *smileys;
	void *menu_tag;
};

static LList *themes=NULL;

static void unload_themes()
{
	while(themes) {
		struct smiley_theme *theme = themes->data;
		ay_remove_smiley_set(theme->name);
		if(theme->menu_tag)
			eb_remove_menu_item(EB_IMPORT_MENU, theme->menu_tag);

		if(smileys == theme->smileys)
			smileys = eb_smileys();
		
		while(theme->smileys) {
			struct smiley_struct *smiley = theme->smileys->data;
			XpmFree(smiley->pixmap);
			theme->smileys = l_list_remove(theme->smileys, theme->smileys);
		}
		free(theme->name);
		themes = l_list_remove(themes, themes);
	}
}

static int smiley_readline(char *ptr, int maxlen, FILE * fp)
{
	int n;
	int c;

	for (n = 1; n < maxlen; n++) {

		c = fgetc(fp);

		if(c!=EOF) {
			if(c == '\r')			/* get rid of \r */
				continue;
			*ptr = (char)c;
			if (c == '\n')
				break;
			ptr++;
		} else {
			if (n == 1)
				return (0);		/* EOF, no data */
			else
				break;			/* EOF, w/ data */
		}
	}

	*ptr = 0;
	return (n);
}

static LList * load_theme(const char *theme_name)
{
	LList *_smileys=NULL;
	FILE *themerc;
	char buff[1024];
	snprintf(buff, sizeof(buff), "%s/%s/%s",
			smiley_directory, theme_name, rcfilename);

	if(!(themerc = fopen(buff, "rt")))
		return NULL;

	while((smiley_readline(buff, sizeof(buff), themerc))>0) {
		char *key, *filename, *tmp, filepath[1024];
		char **smiley_data;
		if(!buff[0] || buff[0] == '#')
			continue;
		if(strchr(buff, '=') <= buff)
			continue;

		for(key = buff; *key && *key == ' '; key++)
			;

		if(!*key)		/* empty key */
			continue;

		filename = strchr(key, '=');
		*filename='\0';
		tmp = strchr(key, ' ');
		if(tmp)
			*tmp='\0';
		for(++filename; *filename && *filename == ' '; filename++)
			;

		if(!*filename)		/* empty filename */
			continue;

		tmp = strchr(filename, ' ');
		if(tmp)
			*tmp='\0';

		
		snprintf(filepath, sizeof(filepath), "%s/%s/%s",
				smiley_directory, theme_name, filename);

		printf("loading %s...", filepath);
		if(XpmReadFileToData(filepath, &smiley_data) != XpmSuccess) {
			printf("Could not load %s\n", filepath);
			continue;
		}

		printf("%p\n", smiley_data);

		_smileys = add_smiley(_smileys, key, smiley_data);
	}
	
	fclose(themerc);

	return _smileys;
}

static void reset_smileys(ebmCallbackData *data)
{
	struct smiley_theme *theme = NULL;
	ebmImportData *eid;

	if(IS_ebmImportData(data))
		eid = (ebmImportData *)data;
	else {	/* This should never happen, unless something is horribly wrong */
		eb_debug(DBG_MOD, "*** Warning *** Unexpected ebmCallbackData type returned!\n");
		return;
	}

	theme = eid->cd.user_data;

	/* Set smileys back to default regardless of what they are now */
	smileys = eb_smileys();

	eb_remove_menu_item(EB_IMPORT_MENU, theme->menu_tag);
	theme->menu_tag=eb_add_menu_item(theme->name, EB_IMPORT_MENU, enable_smileys, ebmIMPORTDATA, theme);
}

static void enable_smileys(ebmCallbackData *data)
{
	struct smiley_theme *theme = NULL;
	ebmImportData *eid;

	if(IS_ebmImportData(data))
		eid = (ebmImportData *)data;
	else {	/* This should never happen, unless something is horribly wrong */
		eb_debug(DBG_MOD, "*** Warning *** Unexpected ebmCallbackData type returned!\n");
		return;
	}

	theme = eid->cd.user_data;

	if(!theme || !theme->smileys)
		return;

	smileys = theme->smileys;

	eb_remove_menu_item(EB_IMPORT_MENU, theme->menu_tag);
	theme->menu_tag=eb_add_menu_item("Default Smilies", EB_IMPORT_MENU, reset_smileys, ebmIMPORTDATA, theme);
}

static void load_themes()
{
	struct dirent *entry;
	DIR *theme_dir = opendir(smiley_directory);
	if(!theme_dir)
		return;

	while((entry=readdir(theme_dir))) {
		struct smiley_theme *theme;
		if(entry->d_name[0]=='.')
			continue;

		theme = calloc(1, sizeof(struct smiley_theme));
		theme->name = strdup(entry->d_name);
		if(!(theme->smileys = load_theme(theme->name)))
			continue;

		theme->menu_tag=eb_add_menu_item(theme->name, EB_IMPORT_MENU, enable_smileys, ebmIMPORTDATA, theme);
		if(!theme->menu_tag) {
			eb_debug(DBG_MOD,"Error!  Unable to add Smiley menu to import menu\n");
			l_list_free(theme->smileys);
			free(theme->name);
			continue;
		}

		ay_add_smiley_set( theme->name, theme->smileys );
	}

	closedir(theme_dir);
}

