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

#ifndef __SOUND_H__
#define __SOUND_H__

enum eSoundID
{
	SOUND_BUDDY_ARRIVE = 0,
	SOUND_BUDDY_LEAVE,
	SOUND_BUDDY_AWAY,
	SOUND_SEND,
	SOUND_RECEIVE,
	SOUND_FIRSTMSG,
	
	SOUND_MAX
};

#ifdef __cplusplus
extern "C" {
#endif

void play_sound(int sound);
void playsoundfile(char *soundfile);
void sound_init();
void sound_shutdown();

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __SOUND_H__ */
