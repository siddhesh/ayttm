#ifndef __IMAGE_WINDOW_H__
#define __IMAGE_WINDOW_H__

int ay_image_window_new(int width, int height, const char *title);
int ay_image_window_add_data(int tag, const unsigned char *buf, int count, int new);
void ay_image_window_close(int tag);

#endif
