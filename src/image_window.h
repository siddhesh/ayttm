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

#ifndef __IMAGE_WINDOW_H__
#define __IMAGE_WINDOW_H__

typedef void (*ay_image_window_cancel_callback) (int tag, void *data);

#ifdef __cplusplus
extern "C" {
#endif

	int ay_image_window_new(int width, int height, const char *title,
		ay_image_window_cancel_callback cancel_callback,
		void *callback_data);
	int ay_image_window_add_data(int tag, const unsigned char *buf,
		long count, int new_image);
	void ay_image_window_update_title(int tag, const char *title);
	void ay_image_window_close(int tag);

	extern unsigned char *(*image_2_jpg) (const unsigned char *, long *);
	extern unsigned char *(*image_2_jpc) (const unsigned char *, long *);

#ifdef __cplusplus
}
#endif
#endif
