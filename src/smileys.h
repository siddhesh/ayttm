/*
 * Ayttm 
 *
 * Copyright (C) 2003, the Ayttm team
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

#ifndef __SMILEYS_H__
#define __SMILEYS_H__

#include "chat_window.h"

struct protocol_smiley_struct {
	char text[16];		// :-), :), ;-), etc
	char name[64];		// this goes into the <smiley> tag for the gtkhtml stuff
};

typedef struct protocol_smiley_struct protocol_smiley;

struct smiley_struct {
	char *service;
	char name[64];
	const gchar **pixmap;	// from an xpm file, you know the drill...
};

typedef struct smiley_struct smiley;

#if defined(__MINGW32__) && defined(__IN_PLUGIN__)
__declspec(dllimport)
LList *smileys;
#else
extern LList *smileys;
#endif

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct {
		const char *set_name;	///< name of the set [key]
		LList *set_smiley_list;	///< the list of (struct smiley_struct) which make up the set
	} t_smiley_set;

	typedef LList t_smiley_set_list;	///< a list of t_smiley_set

	void ay_add_smiley_set(const char *inName, LList *inSmileyList);
	t_smiley_set_list *ay_get_smiley_sets(void);
	t_smiley_set *ay_lookup_smiley_set(const char *inName);
	void ay_remove_smiley_set(const char *inName);

	void init_smileys(void);

	gchar *eb_smilify(const char *text, LList *protocol_smileys,
		const char *service);

	LList *eb_default_smileys(void);

#if defined(__MINGW32__) && defined(__IN_PLUGIN__)
	 __declspec(dllimport)
#endif
	LList *add_smiley(LList *list, const char *name, const char **data,
		const char *service);

	LList *add_protocol_smiley(LList *list, const char *text,
		const char *name);

/* someone figure out how to do this with LList * const */
	LList *eb_smileys(void);

	smiley *get_smiley_by_name(const char *name);
	smiley *get_smiley_by_name_and_service(const char *name,
		const char *service);

	typedef struct _smiley_callback_data smiley_callback_data;

	struct _smiley_callback_data {
		chat_window *c_window;
	};

	void show_smileys_cb(smiley_callback_data *data);

#ifdef __cplusplus
}				/* extern "C" */
#endif
#endif
