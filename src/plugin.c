/*
 * Ayttm 
 *
 * Copyright (C) 2003, the Ayttm team
 * 
 * Ayttm is derivative of Everybuddy
 * Copyright (C) 1999-2002, Torrey Searle <tsearle@uci.edu>
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
typedef long __off32_t;
#define AYTTM_DELIM ";"
#else
#define AYTTM_DELIM ":"
#endif
#include <sys/types.h>
#include <dirent.h> /* Routines to read directories to find modules */
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "globals.h"
#include "defaults.h"
#include "service.h"
#include "plugin.h"
#include "nomodule.h"
#include "value_pair.h"
#include "status.h"
#include "messages.h"



const char *PLUGIN_TYPE_TXT[] =
{
	"SERVICE",
	"FILTER",
	"IMPORTER",
	"SMILEY",
	"UTILITY",
	"UNKNOWN"
};

const char *PLUGIN_STATUS_TXT[] =
{
	"Not Loaded", 
	"Loaded", 
	"Cannot Load"
};

static PLUGIN_INFO Plugin_Cannot_Load = {PLUGIN_UNKNOWN, 
				  "Unknown",
				  "Unknown", 
				  "Unknown", 
				  "Unknown", NULL, NULL, NULL, NULL};

static int	load_service_plugin( lt_dlhandle Module, PLUGIN_INFO *info, const char *name );
static int	load_plugin_default( lt_dlhandle Module, PLUGIN_INFO *info, const char *name );


static int compare_plugin_loaded_service(gconstpointer a, gconstpointer b)
{
	const eb_PLUGIN_INFO *epi=a;
	if(epi->service && epi->status==PLUGIN_LOADED && !strcmp( epi->service, (char *)b))
		return(0);
	return(1);
}

static int compare_plugin_name(gconstpointer a, gconstpointer b)
{
	const eb_PLUGIN_INFO *epi=a;
	if(epi->name && !strcmp( epi->name, (char *)b))
		return(0);
	return(1);
}


static eb_PLUGIN_INFO *FindLoadedPluginByService(const char *service)
{
	LList *plugins=GetPref(EB_PLUGIN_LIST);
	LList *PluginData = l_list_find_custom(plugins, service, compare_plugin_loaded_service);
	if(PluginData)
		return(PluginData->data);
	return(NULL);
}

eb_PLUGIN_INFO *FindPluginByName(const char *name)
{
	LList *plugins=GetPref(EB_PLUGIN_LIST);
	LList *PluginData = l_list_find_custom(plugins, name, compare_plugin_name);
	if(PluginData)
		return(PluginData->data);
	return(NULL);
}


/* Will add/update info about a plugin */
static void	SetPluginInfo( PLUGIN_INFO *pi, const char *name, lt_dlhandle Module,
	PLUGIN_STATUS status, const char *status_desc, const char *service, gboolean force )
{
	LList *plugins=NULL;
	eb_PLUGIN_INFO *epi=NULL;

	epi=FindPluginByName(name);
	if(!epi) {
		epi=g_new0(eb_PLUGIN_INFO, 1);
		plugins=GetPref(EB_PLUGIN_LIST);
		plugins=l_list_append(plugins, epi);
		SetPref(EB_PLUGIN_LIST, plugins);
	}
	else if(force==TRUE || epi->status!=PLUGIN_LOADED) {
		if(epi->service)
			free(epi->service);
		free(epi->name);
		free(epi->pi.module_name);
		free(epi->pi.description);
		free(epi->pi.version);
		free(epi->pi.date);
	}
	else 	/* A plugin is already succesfully load */
		return;
	epi->status=status;
	epi->status_desc=status_desc;
	if(!pi)
		pi=&Plugin_Cannot_Load;
	epi->pi.type=pi->type;
	epi->pi.module_name=strdup(pi->module_name);
	epi->pi.description=strdup(pi->description);
	epi->pi.version=strdup(pi->version);
	epi->pi.date=strdup(pi->date);
	epi->pi.init=pi->init;
	epi->pi.finish=pi->finish;
	epi->pi.prefs=pi->prefs;
	epi->name=strdup(name);
	if(service)
		epi->service=strdup(service);
	epi->Module=Module;
	
	if (status == PLUGIN_CANNOT_LOAD) {
		char buff[512];
		snprintf(buff, 512, _("Plugin %s has not been loaded because of an error:\n\n%s"),
				name, status_desc);
		ay_do_error( _("Plugin error"), buff );
	}
}

/* Find names which end in .la, the expected module extension */
static int select_module_entry(const struct dirent *dent)
{
	int len=0;
	char *ext;

	len=strlen(dent->d_name);
	if(len<4)
	   return(0);
	ext=(char *)dent->d_name;
	ext+=(len-3);
	eb_debug(DBG_CORE, "select_module_entry: %s[%s]\n", dent->d_name, ext);
	if(!strncmp(ext, ".la", 3))
	   return(1);
	return(0);
}

int load_module_full_path( const char *inFullPath )
{
	lt_dlhandle		Module;
	PLUGIN_INFO		*plugin_info = NULL;
	eb_PLUGIN_INFO	*epi = NULL;

	
	assert( inFullPath != NULL );
	
	eb_debug( DBG_CORE, "Opening module: %s\n", inFullPath );
	Module = lt_dlopen( inFullPath );
	eb_debug( DBG_CORE, "Module: %p\n", Module );

	/* Find out if this plugin is already loaded */
	if ( Module == NULL )
	{
		/* Only update status on a plugin that is not already loaded */
		SetPluginInfo( NULL, inFullPath, NULL, PLUGIN_CANNOT_LOAD, lt_dlerror(), NULL, FALSE );
		return( -1 );
	}
	
	plugin_info = (PLUGIN_INFO *)lt_dlsym( Module, "plugin_info" );
	if ( !plugin_info )
	{
		lt_dlclose( Module );
		/* Only update status on a plugin that is not already loaded */
		SetPluginInfo( NULL, inFullPath, NULL, PLUGIN_CANNOT_LOAD, _("Cannot resolve symbol \"plugin_info\"."), NULL, FALSE );
		return( -1 );
	}
	
	epi = FindPluginByName( inFullPath );
	if ( epi && epi->status == PLUGIN_LOADED )
	{
		lt_dlclose( Module );
		eb_debug( DBG_CORE, "Module already loaded: %s\n", inFullPath );
		return( -1 );
	}
	
	switch ( plugin_info->type )
	{
		case PLUGIN_SERVICE:
			load_service_plugin( Module, plugin_info, inFullPath );
			break;
			
		case PLUGIN_FILTER:
		case PLUGIN_IMPORTER:
		case PLUGIN_SMILEY:
		case PLUGIN_UTILITY:
			load_plugin_default( Module, plugin_info, inFullPath );
			break;
			
		default:
			assert( FALSE );
			break;
	}
	
	return( 0 );
}

int	load_module( const char *path, const char *name )
{
	char	full_path[PATH_MAX];

	assert( path != NULL );
	assert( name != NULL );
	
	snprintf( full_path, PATH_MAX, "%s/%s", path, name );
	
	return( load_module_full_path( full_path ) );
}

/* This is really a modules loader now */
void load_modules( void )
{
	/* UNUSED struct dirent **namelist=NULL; */
	char buf[1024], *modules_path=NULL, *cur_path=NULL;
	char *tok_buf=NULL, *tok_buf_old=NULL;
	int n=0, success=0;
	struct dirent *dp;
	DIR *dirp;

	eb_debug(DBG_CORE, ">Entering\n");
	modules_path=g_strdup(cGetLocalPref("modules_path"));
	tok_buf = g_new0(char, strlen(modules_path)+1);
	/* Save the old pointer, because strtok_r will change it */
	tok_buf_old=tok_buf;
	lt_dlinit();
	lt_dlsetsearchpath(modules_path);

	/* Use a thread-safe strtok */
#ifdef HAVE_STRTOK_R
	cur_path=strtok_r(modules_path, AYTTM_DELIM, &tok_buf);
#else
	cur_path=strtok(modules_path, AYTTM_DELIM);
#endif
	if(!cur_path)
		cur_path=MODULE_DIR;
	do {
		if((dirp = opendir(cur_path)) == NULL)
		{
			snprintf(buf, sizeof(buf), _("Cannot open module directory \"%s\"."), cur_path);
			ay_do_warning( _("Plugin Warning"), buf );
			buf[0] = '\0';
			break;
		}
		n = 0;
		while((dp = readdir(dirp)) != NULL)
		{
			if( dp == NULL )
			{
				snprintf(buf, sizeof(buf), _("Looking for modules in %s"), cur_path);
				perror(buf);
				continue;
			}
			else if( select_module_entry( dp ) )
			{
				n++;
				success = load_module(cur_path, dp->d_name);
			}
		}
		if( n == 0 )
		{
			eb_debug(DBG_CORE, "<No modules found in %s, returning.\n", cur_path);
		}
		else
		{
			eb_debug(DBG_CORE, "Loaded %d modules from %s.\n", n, cur_path);
		}
		closedir(dirp);
#ifdef HAVE_STRTOK_R
	} while((cur_path=strtok_r(NULL, ":", &tok_buf)));
#else
	} while((cur_path=strtok(NULL, ":")));
#endif

	g_free(modules_path);
	g_free(tok_buf_old);
	eb_debug(DBG_CORE, "Adding idle_check\n");
	add_idle_check();
	eb_debug(DBG_CORE, "<End services_init\n");
}

static int	load_service_plugin( lt_dlhandle Module, PLUGIN_INFO *info, const char *name )
{
	struct service *Service_Info=NULL;
	struct service_callbacks *(*query_callbacks)();
	int (*module_version)();
	int service_id=-1;

	eb_PLUGIN_INFO *epi=NULL;
	LList *user_prefs=NULL;

	Service_Info = lt_dlsym(Module, "SERVICE_INFO");
	eb_debug(DBG_CORE, "SERVICE_INFO: %p\n", Service_Info);
	if(!Service_Info) {
		SetPluginInfo(info, name, NULL, PLUGIN_CANNOT_LOAD, _("Unable to resolve symbol SERVICE_INFO"), NULL, FALSE);
		lt_dlclose(Module);
		return(-1);
	}
	/* Don't load this module if there's a service of this type already loaded */
	epi=FindLoadedPluginByService(Service_Info->name);
	if(epi && epi->status==PLUGIN_LOADED) {
		fprintf(stderr, _("Not loading module %s, a module for that service is already loaded!\n"), name);
		SetPluginInfo(info, name, NULL, PLUGIN_CANNOT_LOAD, _("Same service is provided by an already loaded plugin."), Service_Info->name, FALSE);
		lt_dlclose(Module);
		return(-1);
	}
	
	module_version = lt_dlsym(Module, "module_version");
	if (!module_version || module_version() != CORE_VERSION)
	{
		const int	error_len = 1024;
		char 		error[error_len];
		
		if ( !module_version )
		{
			strncpy( error, _("This plugin is not binary compatible.\n"
					"Try a clean install (or read README)."), error_len );
		}
		else
		{
			snprintf( error, error_len, _("Plugin version (%d) differs from core version (%d), "
					"which means it is probably not binary compatible.\n"
					"Try a clean install (or read README)."), 
					module_version(),
					CORE_VERSION );
		}
					
		SetPluginInfo(info, name, NULL, PLUGIN_CANNOT_LOAD, error, Service_Info->name, FALSE);
		lt_dlclose(Module);
		return(-1);	
	}
	
	/* No more hard-coded service_id numbers */
	query_callbacks = lt_dlsym(Module, "query_callbacks");
	if(!query_callbacks) {
		SetPluginInfo(info, name, NULL, PLUGIN_CANNOT_LOAD, "Unable to resolve symbol \"query_callbacks\".", Service_Info->name, FALSE);
		lt_dlclose(Module);
		return(-1);
	}
	if(info->init) {
		eb_debug(DBG_CORE, "Executing init for %s\n", info->module_name);
		info->init();
	}
	if(info->prefs) {
		user_prefs=GetPref(name);
		if(user_prefs) {
			eb_update_from_value_pair(info->prefs, user_prefs);
		}
		eb_debug(DBG_MOD, "prefs name: %s\n", info->prefs->widget.entry.name);

		if(info->reload_prefs) {
			eb_debug(DBG_CORE, "Executing reload_prefs for %s\n", info->module_name);
			info->reload_prefs();
		}
	}
	Service_Info->sc=query_callbacks();
	/* The callbacks are defined by the SERVICE_INFO struct in each module */
	service_id = add_service(Service_Info);
	SetPluginInfo(info, name, Module, PLUGIN_LOADED, "", Service_Info->name, TRUE);
	eb_debug(DBG_CORE, "Added module:%s, service: %s\n", name, eb_services[service_id].name);
	eb_debug(DBG_CORE, "eb_services[%i].sc->read_local_account_config: %p\n", service_id, eb_services[service_id].sc->read_local_account_config);
	return(0);
}

static int	load_plugin_default( lt_dlhandle Module, PLUGIN_INFO *info, const char *name )
{
	const int	buf_len = 1024;
	char		buf[buf_len];
	LList		*user_prefs = NULL;

	
	eb_debug( DBG_CORE, ">\n" );
	
	if ( !info->init )
	{
		SetPluginInfo( info, name, NULL, PLUGIN_CANNOT_LOAD, _("No init function defined"), NULL, FALSE );
		
		lt_dlclose( Module );
		
		snprintf( buf, buf_len, _("Utility module %s doesn't define an init function; skipping it."), name );
		ay_do_warning( _("Plugin Warning"), buf );
		
		return( -1 );
	}
	
	eb_debug( DBG_CORE, "Executing init for %s\n", info->module_name );
	info->init();
	
	if ( info->prefs )
	{
		user_prefs = GetPref( name );
		
		if ( user_prefs != NULL )
			eb_update_from_value_pair( info->prefs, user_prefs );

		eb_debug( DBG_MOD, "prefs name: %s\n", info->prefs->widget.entry.name );

		if(info->reload_prefs) {
			eb_debug(DBG_CORE, "Executing reload_prefs for %s\n", info->module_name);
			info->reload_prefs();
		}
	}
	
	SetPluginInfo( info, name, Module, PLUGIN_LOADED, "", NULL, TRUE );
	
	eb_debug( DBG_CORE, "<\n" );
	
	return( 0 );
}

int	unload_module_full_path( const char *inFullPath )
{
	eb_PLUGIN_INFO	*epi = NULL;

	
	assert( inFullPath != NULL );
	
	epi = FindPluginByName( inFullPath );
	
	if ( epi == NULL )
	{
		eb_debug( DBG_CORE, "<Plugin not found: %s [file: %s line: %d]\n", inFullPath, __FILE__, __LINE__ );
		return( 0 );
	}
	
	return( unload_module( epi ) );
}

int	unload_module( eb_PLUGIN_INFO *epi )
{
	int error=0;
	char buf[1024];

	eb_debug(DBG_CORE, ">Unloading plugin %s\n", epi->name);
	/* This is a service plugin, special handling required */
	if (epi->status!=PLUGIN_LOADED)
	{
		eb_debug(DBG_CORE, "<Plugin not loaded\n");
		return( 0 );
	}
	
	if(epi->pi.finish) {
		eb_debug(DBG_CORE, "Calling plugins finish function\n");
		error=epi->pi.finish();
		if(error > 0) {
			snprintf(buf, sizeof(buf), _("Unable to unload plugin %s; maybe it is still in use?"), epi->name);
			ay_do_error( _("Plugin error"), buf );
			eb_debug(DBG_CORE, "<Plugin failed to unload\n");
			return(-1);
		}
	}
	if(epi->service) {
		struct service SERVICE_INFO = { strdup(epi->service), -1, SERVICE_CAN_NOTHING, NULL };

		SERVICE_INFO.sc=eb_nomodule_query_callbacks();
		add_service(&SERVICE_INFO);
	}
	epi->status=PLUGIN_NOT_LOADED;
	epi->pi.prefs=NULL;
	eb_debug(DBG_CORE, "Closing plugin\n");
	if(lt_dlclose(epi->Module)) {
		fprintf(stderr, "Error closing plugin: %s\n", lt_dlerror());
	}
	eb_debug(DBG_CORE, "<Plugin unloaded\n");
	return(0);
}

void unload_modules( void )
{
	LList *plugins = GetPref(EB_PLUGIN_LIST);
	
	for ( ; plugins; plugins=plugins->next)
	{
		unload_module(plugins->data);
	}
}

/* Make sure that all the plugin_api accessible menus are initialized */
static int init_menu(char *menu_name, menu_func redraw_menu, ebmType type)
{
	menu_data *md=NULL;

	md=g_new0(menu_data, 1);
	md->menu_items=NULL;
	md->redraw_menu=redraw_menu;
	md->type=type;
	SetPref(menu_name, md);
	return(0);
}

static void rebuild_smiley_menu( void )
{
	GtkWidget *smiley_submenuitem;

	smiley_submenuitem = GetPref("widget::smiley_submenuitem");
	if(!smiley_submenuitem) {
		eb_debug(DBG_CORE, "Not rebuilding smiley menu, it's never been built.\n");
		return;
	}
	gtk_menu_item_remove_submenu(GTK_MENU_ITEM(smiley_submenuitem));
	eb_smiley_window(smiley_submenuitem);
	gtk_widget_draw(GTK_WIDGET(smiley_submenuitem), NULL);
}

static void rebuild_import_menu( void )
{
	GtkWidget *import_submenuitem;

	import_submenuitem = GetPref("widget::import_submenuitem");
	if(!import_submenuitem) {
		eb_debug(DBG_CORE, "Not rebuilding import menu, it's never been built.\n");
		return;
	}
	gtk_menu_item_remove_submenu(GTK_MENU_ITEM(import_submenuitem));
	eb_import_window(import_submenuitem);
	gtk_widget_draw(GTK_WIDGET(import_submenuitem), NULL);
}

static void rebuild_profile_menu( void )
{
	GtkWidget *profile_submenuitem;

	profile_submenuitem = GetPref("widget::profile_submenuitem");
	if(!profile_submenuitem) {
		eb_debug(DBG_CORE, "Not rebuilding profile menu, it's never been built.\n");
		return;
	}
	gtk_menu_item_remove_submenu(GTK_MENU_ITEM(profile_submenuitem));
	eb_profile_window(profile_submenuitem);
	gtk_widget_draw(GTK_WIDGET(profile_submenuitem), NULL);
}

/* Set up information about how menus are redrawn and what kind of data should be sent to callbacks */
int init_menus()
{
	init_menu(EB_PROFILE_MENU, rebuild_profile_menu, ebmPROFILEDATA);
	init_menu(EB_IMPORT_MENU, rebuild_import_menu, ebmIMPORTDATA);
	init_menu(EB_SMILEY_MENU, rebuild_smiley_menu, ebmSMILEYDATA);
	/* The chat window menu is dynamically redrawn */
	init_menu(EB_CHAT_WINDOW_MENU, NULL, ebmCONTACTDATA);
	init_menu(EB_CONTACT_MENU, NULL, ebmCONTACTDATA);
	return(0);
}
