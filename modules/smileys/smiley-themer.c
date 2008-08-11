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
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#ifndef __MINGW32__
#include <X11/xpm.h>
#else
#define XPM_NO_X
#include <noX/xpm.h>
#endif


/*******************************************************************************
 *                             Begin Module Code
 ******************************************************************************/
/*  Module defines */
#define plugin_info smiley_themer_LTX_plugin_info
#define module_version smiley_themer_LTX_module_version


static int plugin_init();
static int plugin_finish();
static int reload_prefs();

static int is_setting_state=1;
static int ref_count=0;
static char smiley_directory[MAX_PREF_LEN]=AYTTM_SMILEY_DIR;
static char last_selected[MAX_PREF_LEN]="";
static int do_smiley_debug = 0;

static char rcfilename[]="aysmile.rc";

#ifndef FREE
# define FREE(x) if(x){free(x); x=NULL;}
#endif


/*  Module Exports */
PLUGIN_INFO plugin_info = {
	PLUGIN_SMILEY,
	"Smiley Themes",
	"Loads smiley themes from disk at run time",
	"$Revision: 1.16 $",
	"$Date: 2008/08/11 04:50:46 $",
	&ref_count,
	plugin_init,
	plugin_finish,
	reload_prefs,
	NULL
};
/* End Module Exports */

unsigned int module_version() {return CORE_VERSION;}

static void load_themes();
static void unload_themes();
static void enable_smileys(ebmCallbackData *data);
static void activate_theme_by_name(const char *name);

static int plugin_init()
{
	input_list *il;

	if(!smiley_directory[0])
		return -1;

	il = g_new0(input_list, 1);
	ref_count = 0;
	plugin_info.prefs = il;
	il->widget.entry.value = smiley_directory;
	il->name = "smiley_directory";
	il->label= _("Smiley Directory:");
	il->type = EB_INPUT_ENTRY;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.entry.value = last_selected;
	il->name = "last_selected";
	il->label= _("Last Selected:");
	il->type = EB_INPUT_ENTRY;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.checkbox.value = &do_smiley_debug;
	il->name = "do_smiley_debug";
	il->label= _("Enable debugging");
	il->type = EB_INPUT_CHECKBOX;

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

static int reload_prefs()
{
	is_setting_state=0;
	unload_themes();
	load_themes();
	activate_theme_by_name(last_selected);
	return 0;
}

/*******************************************************************************
 *                             End Module Code
 ******************************************************************************/

static int smiley_log(char *fmt,...)
{
	if(do_smiley_debug) {
		va_list ap;

		va_start(ap, fmt);

		vfprintf(stderr, fmt, ap);
		fflush(stderr);
		va_end(ap);
	}
	return 0;
}

#define LOG(x) if(do_smiley_debug) { smiley_log("%s:%d: ", __FILE__, __LINE__); \
	smiley_log x; \
	smiley_log("\n"); }

struct smiley_theme {
	char *name;
	char *description;
	char *author;
	char *copyright;
	char *date;
	char *revision;

	LList *smileys;
	void *menu_tag;

	int core;
};

static LList *themes=NULL;

static void unload_theme(struct smiley_theme * theme)
{
	if(theme->core) {
		ay_remove_smiley_set(theme->name);
		if(theme->menu_tag)
			eb_remove_menu_item(EB_SMILEY_MENU, theme->menu_tag);
		return;
	}

	if(smileys == theme->smileys)
		smileys = eb_smileys();

	if(theme->name)
		ay_remove_smiley_set(theme->name);
	if(theme->menu_tag)
		eb_remove_menu_item(EB_SMILEY_MENU, theme->menu_tag);

	while(theme->smileys) {
		struct smiley_struct *smiley = theme->smileys->data;
		XpmFree(smiley->pixmap);
		FREE(smiley->service);
		FREE(smiley);
		theme->smileys = l_list_remove_link(theme->smileys, theme->smileys);
	}

	FREE(theme->name);
	FREE(theme->description);
	FREE(theme->author);
	FREE(theme->copyright);
	FREE(theme->date);
	FREE(theme->revision);
	FREE(theme);
}

static void unload_themes()
{
	while(themes) {
		struct smiley_theme *theme = themes->data;
		unload_theme(theme);
		themes = l_list_remove_link(themes, themes);
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

static int splitline(char *buff, char **key, char **value)
{
	char *tmp;

	if(!buff[0] || buff[0] == '#')
		return 0;
	if(strchr(buff, '=') <= buff)
		return 0;

	/* remove leading spaces */
	for(; *buff && isspace(*buff); buff++)
		;

	if(!*buff)		/* empty key */
		return 0;

	*key = buff;

	*value = strchr(buff, '=');
	**value='\0';

	/* remove trailing spaces */
	for(tmp=buff; *tmp && !isspace(*tmp); tmp++)
		;
	if(*tmp)
		*tmp='\0';

	buff = (*value)+1;
	/* remove leading spaces of value */
	for(; *buff && isspace(*buff); buff++)
		;

	if(!*buff)		/* empty value */
		return 0;

	*value = buff;

	/* remove trailing spaces */
	for(tmp=buff+strlen(buff)-1; tmp >= buff && isspace(*tmp); tmp--)
		*tmp='\0';

	return 1;
}

static struct smiley_theme * load_theme(const char *theme_name)
{
	FILE *themerc;
	char buff[1024];
	struct smiley_theme *theme;
	char *curr_protocol=NULL;

	snprintf(buff, sizeof(buff), "%s/%s/%s",
			smiley_directory, theme_name, rcfilename);

	if(!(themerc = fopen(buff, "rt"))) {
		LOG(("Could not find/open %s error %d: %s", rcfilename, errno, strerror(errno)));
		return NULL;
	}

	theme = calloc(1, sizeof(struct smiley_theme));

	while((smiley_readline(buff, sizeof(buff), themerc))>0) {
		const char **smiley_data;
		char filepath[1024];
		char *key, *value;

		if(!splitline(buff, &key, &value))
			continue;

		if(key[0] == '%') {
			key++;
			if(!strcmp(key, "name"))
				theme->name = strdup(value);
			else if(!strcmp(key, "desc"))
				theme->description = strdup(value);
			else if(!strcmp(key, "author"))
				theme->author = strdup(value);
			else if(!strcmp(key, "date"))
				theme->date = strdup(value);
			else if(!strcmp(key, "revision"))
				theme->revision = strdup(value);
			else if(!strcmp(key, "protocol")) {
				FREE(curr_protocol);
				curr_protocol = strdup(value);
			}
		} else {
			snprintf(filepath, sizeof(filepath), "%s/%s/%s",
					smiley_directory, theme_name, value);

			if(XpmReadFileToData(filepath, &smiley_data) != XpmSuccess) {
				LOG(("Could not read xpm file %s", filepath));
				continue;
			}

			theme->smileys = add_smiley(theme->smileys, key, smiley_data, curr_protocol);
		}
	}
	
	fclose(themerc);

	if(!theme->smileys) {
		unload_theme(theme);
		return NULL;
	}

	if(!theme->name)
		theme->name = strdup(theme_name);

	return theme;
}

static void enable_smileys(ebmCallbackData *data)
{
	struct smiley_theme *theme = (struct smiley_theme *)data;

	if(is_setting_state || !theme || !theme->smileys)
		return;

	is_setting_state = 1;
	smileys = theme->smileys;

	eb_activate_menu_item(EB_SMILEY_MENU, theme->menu_tag);

	strncpy(last_selected, theme->name, sizeof(last_selected)-1);
	is_setting_state = 0;
}

static void load_themes()
{
	struct smiley_theme *theme;
	struct dirent *entry;
	DIR *theme_dir = opendir(smiley_directory);
	if(!theme_dir) {
		LOG(("Unable to open smiley directory %s", smiley_directory));
		return;
	}

	LOG(("Opened smileydirectory %s\n",smiley_directory));
	
	theme = calloc(1, sizeof(struct smiley_theme));
	theme->name = _("Default");
	theme->smileys = eb_smileys();
	theme->core = 1;

	theme->menu_tag=eb_add_menu_item(theme->name, EB_SMILEY_MENU, enable_smileys, ebmSMILEYDATA, theme);
	if(!theme->menu_tag) {
		eb_debug(DBG_MOD,"Error!  Unable to add Smiley menu to smiley menu\n");
		free(theme);
	} else
		themes = l_list_prepend(themes, theme);

	while((entry=readdir(theme_dir))) {
		if(entry->d_name[0]=='.')
			continue;

		if(!(theme = load_theme(entry->d_name))) {
			LOG(("Could not load theme %s", entry->d_name));
			continue;
		}

		theme->menu_tag=eb_add_menu_item(theme->name, EB_SMILEY_MENU, enable_smileys, ebmSMILEYDATA, theme);
		if(!theme->menu_tag) {
			eb_debug(DBG_MOD,"Error!  Unable to add Smiley menu to smiley menu\n");
			unload_theme(theme);
			continue;
		}

		ay_add_smiley_set( theme->name, theme->smileys );

		themes = l_list_prepend(themes, theme);
	}

	closedir(theme_dir);
}

static void activate_theme_by_name(const char *name)
{
	LList * l;
	for(l=themes; l; l=l_list_next(l)) {
		struct smiley_theme *theme = l->data;
		if(!strcmp(theme->name, name)) {
			enable_smileys((ebmCallbackData *)theme);
			return;
		}
	}
}

