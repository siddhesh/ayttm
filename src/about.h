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

#ifndef __ABOUT_H__
#define __ABOUT_H__

/* info about a developer for the about window */
typedef struct {
	const char *m_name;
	const char *m_email;
	const char *m_role;
} tDeveloperInfo;

typedef struct {
	const char *m_version;

	const tDeveloperInfo *m_ay_developers;
	int m_num_ay_developers;

	const tDeveloperInfo *m_ay_kudos;
	int m_num_ay_kudos;

	const tDeveloperInfo *m_eb_developers;
	int m_num_eb_developers;
} tAboutInfo;

#ifdef __cplusplus
extern "C" {
#endif

	void ay_show_about(void);

#ifdef __cplusplus
}
#endif
#endif
