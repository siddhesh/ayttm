/*
 * Ayttm
 *
 * Copyright (C) 2003, the Ayttm team
 * 
 * Ayttm is a derivative of Everybuddy
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

#ifndef __PREFS_H__
#define __PREFS_H__

#include "input_list.h"


#define MAX_PREF_NAME_LEN 255
#define MAX_PREF_LEN 1024

#ifdef __cplusplus
extern "C" {
#endif


typedef struct
{
	const char		*module_type;
	const char		*brief_desc;
	const char		*loaded_status;
	const char		*version;
	const char		*date;
	const char		*file_name;
	const char		*status_desc;
	const char		*service_name;
	
	input_list		*pref_list;
} t_module_pref;

struct prefs
{
	struct general
	{
		int		do_ayttm_debug;
		int		use_alternate_browser;
		char	alternate_browser[MAX_PREF_LEN];

		#ifdef HAVE_LIBPSPELL
			int		do_spell_checking;
			char	spell_dictionary[MAX_PREF_LEN];
		#endif
	} general;
	
	struct logging
	{
		int		do_logging;
		int		do_restore_last_conv;
	} logging;
	
	struct chat
	{
		int		do_ignore_unknown;
		int		font_size;
		int		do_raise_window;
		int		do_send_idle_time;
		int		do_ignore_fore;
		int		do_ignore_back;
		int		do_ignore_font;
	} chat;
	
	struct tabs
	{
		int		do_tabbed_chat;
		int		do_tabbed_chat_orient;	/* Tab Orientation:  0 => bottom, 1 => top, 2=> left, 3 => right */
		char	accel_prev_tab[MAX_PREF_LEN];
		char	accel_next_tab[MAX_PREF_LEN];
	} tabs;
	
	struct sound
	{
		int		do_no_sound_when_away;
		int		do_no_sound_for_ignore;
		int		do_online_sound;
		int		do_play_send;
		int		do_play_first;
		int		do_play_receive;
		
		char	BuddyArriveFilename[MAX_PREF_LEN];
		char	BuddyAwayFilename[MAX_PREF_LEN];
		char	BuddyLeaveFilename[MAX_PREF_LEN];
		char	SendFilename[MAX_PREF_LEN];
		char	ReceiveFilename[MAX_PREF_LEN];
		char	FirstMsgFilename[MAX_PREF_LEN];
		
		double	SoundVolume;
	} sound;
	
	struct advanced
	{
		int		proxy_type;
		char	proxy_host[MAX_PREF_LEN];
		int		proxy_port;
		
		int		do_proxy_auth;
		char	proxy_user[MAX_PREF_LEN];
		char	proxy_password[MAX_PREF_LEN];
		
		int		use_recoding;
		char	local_encoding[MAX_PREF_LEN];
		char	remote_encoding[MAX_PREF_LEN];
	} advanced;
	
	struct module
	{
		LList	*module_info;
	} module;
};


void	ayttm_prefs_init( void );
#if defined(__MINGW32__) && defined(__IN_PLUGIN__)
__declspec(dllimport)
#endif
void	ayttm_prefs_read( void );
#if defined(__MINGW32__) && defined(__IN_PLUGIN__)
__declspec(dllimport)
#endif
void	ayttm_prefs_read_file( char *file );
void	ayttm_prefs_write( void );

void			ayttm_prefs_show_window( void );
t_module_pref	*ayttm_prefs_find_module_by_name( const struct prefs *inPrefs, const char *inName );
void			ayttm_prefs_apply( struct prefs *inPrefs );
void			ayttm_prefs_cancel( struct prefs *inPrefs );

#if defined(__MINGW32__) && defined(__IN_PLUGIN__)
__declspec(dllimport) void *GetPref( const char *key );
#else
void	*GetPref( const char *key );
#endif

void	*SetPref( const char *key, void *data );

void	iSetLocalPref( const char *key, int data );
int		iGetLocalPref( const char *key );

void	fSetLocalPref( const char *key, float data );
float	fGetLocalPref( const char *key);

void	cSetLocalPref( const char *key, char *data );
char	*cGetLocalPref( const char *key );

void	save_account_info( const char *service, LList *pairs );

#ifdef __cplusplus
}
#endif

#endif
