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

#ifndef __ACTIVITY_BAR_H__
#define __ACTIVITY_BAR_H__

typedef void (*activity_bar_cancel_callback) (void *userdata);

#ifdef __cplusplus
extern "C" {
#endif

	int ay_activity_bar_add(const char *label,
		activity_bar_cancel_callback cancel_callback, void *userdata);
	int ay_progress_bar_add(const char *label, unsigned long size,
		activity_bar_cancel_callback cancel_callback, void *userdata);
	void ay_activity_bar_remove(int tag);
	void ay_activity_bar_update_label(int tag, const char *label);
	void ay_progress_bar_update_progress(int tag, const unsigned long done);
#ifdef __cplusplus
}
#endif
#endif
