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

#ifdef __cplusplus
extern "C" {
#endif

int ay_image_window_new(int width, int height, const char *title);
int ay_image_window_add_data(int tag, const unsigned char *buf, int count, int new_image);
void ay_image_window_close(int tag);

#ifdef __cplusplus
}
#endif

#endif
