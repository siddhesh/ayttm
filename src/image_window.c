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

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif
#include "image_window.h"
#include "mem_util.h"
#include <stdio.h>
#include <stdlib.h>
#include "debug.h"

#ifndef HAVE_GDK_PIXBUF
#include "globals.h"

int ay_image_window_new(int width, int height, const char *title, ay_image_window_cancel_callback callback, void *callback_data)
{ 
	eb_debug(DBG_CORE, "Image window support not included\n");
	return 0; 
}
int ay_image_window_add_data(int tag, const unsigned char *buf, long count, int new) { return 0; }
void ay_image_window_close(int tag) { }

#else

#include "intl.h"
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf-loader.h>
#include "llist.h"
#include "mem_util.h"

#include "globals.h"

struct ay_image_wnd {
	GtkWidget *window;
	GtkWidget *pixmap;
	GdkPixbufLoader *loader;
	int loader_open;

	ay_image_window_cancel_callback callback;
	void *callback_data;

	int tag;
};

static int last_tag = 0;
static LList *images = NULL;


unsigned char *(*image_2_jpg)(const unsigned char *, long *) = NULL;
unsigned char *(*image_2_jpc)(const unsigned char *, long *) = NULL;

static struct ay_image_wnd * get_image_wnd_by_tag(int tag)
{
	LList * l;
	for (l = images; l; l = l_list_next(l)) {
		struct ay_image_wnd * aiw = l->data;
		if(aiw->tag == tag)
			return aiw;
	}
	return NULL;
}


static void ay_image_window_destroy(GtkWidget *widget, gpointer data)
{
	struct ay_image_wnd *aiw = data;

	if(!aiw)
		return;

	images = l_list_remove(images, aiw);

	if(aiw->loader_open) {
		gdk_pixbuf_loader_close(aiw->loader);
		aiw->loader_open=0;
	}

	if(aiw->callback)
		aiw->callback(aiw->tag, aiw->callback_data);

	if(aiw->tag == last_tag)
		last_tag--;

	g_free(aiw);
}

static char *dummy_pixmap_xpm[] = {
	"1 1 1 1",
	"  c None",
	" "
};

static GtkWidget *create_dummy_pixmap(GtkWidget * widget)
{
	GdkColormap *colormap;
	GdkPixmap *gdkpixmap;
	GdkBitmap *mask;
	GtkWidget *pixmap;

	colormap = gtk_widget_get_colormap(widget);
	gdkpixmap = gdk_pixmap_colormap_create_from_xpm_d(NULL, colormap, &mask,
						  NULL, dummy_pixmap_xpm);
	pixmap = gtk_pixmap_new(gdkpixmap, mask);
	gdk_pixmap_unref(gdkpixmap);
	gdk_bitmap_unref(mask);
	return pixmap;
}

int ay_image_window_new(int width, int height, const char *title, ay_image_window_cancel_callback callback, void *callback_data)
{
	struct ay_image_wnd * aiw;
	GtkWidget *wndImage;
	GtkWidget *vbox1;
	GtkWidget *pixImage;
	GtkWidget *btnClose;

	if (!title)
		title = _("Viewing Image");

	wndImage = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(wndImage), title);

	vbox1 = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(vbox1);
	gtk_container_add(GTK_CONTAINER(wndImage), vbox1);

	pixImage = create_dummy_pixmap(wndImage);
	gtk_widget_show(pixImage);

	gtk_box_pack_start(GTK_BOX(vbox1), pixImage, TRUE, TRUE, 0);
	gtk_widget_set_usize(pixImage, width, height);

	btnClose = gtk_button_new_with_label(_("Close"));
	gtk_signal_connect_object(GTK_OBJECT(btnClose), "clicked",
				GTK_SIGNAL_FUNC(gtk_widget_destroy),
				GTK_OBJECT(wndImage));
	gtk_widget_show(btnClose);
	gtk_box_pack_start(GTK_BOX(vbox1), btnClose, FALSE, FALSE, 0);


	aiw = g_new0(struct ay_image_wnd, 1);
	aiw->window = wndImage;
	aiw->pixmap = pixImage;
	aiw->tag = ++last_tag;
	aiw->callback = callback;
	aiw->callback_data = callback_data;

	images = l_list_prepend(images, aiw);

	gtk_signal_connect(GTK_OBJECT(wndImage), "destroy",
				GTK_SIGNAL_FUNC(ay_image_window_destroy), aiw);

	gtk_widget_show(wndImage);

	return aiw->tag;
}


/*
 * tag   - identifies the window
 * buf   - image data to be shown.  NULL == no more data
 * count - nbytes in buf
 * new   - true if we need to create a new image
 *
 * returns: 1 on success, 0 on failure (window already closed?)
 */
int ay_image_window_add_data(int tag, const unsigned char *buf, long count, int new)
{
	struct ay_image_wnd * aiw = get_image_wnd_by_tag(tag);
	unsigned char *jpg_buf;
	GdkPixbuf *pixbuf;
	GdkPixmap *gdkpixmap;
	GdkBitmap *mask;

	if(!aiw || !buf) {
		return 0;
	}

	if(image_2_jpg)
		jpg_buf = image_2_jpg(buf, &count);
	else {
		eb_debug(DBG_CORE, "image decoder not loaded\n");
		jpg_buf = ay_memdup(buf, count);
	}

	eb_debug(DBG_CORE, "jpg_buf is %p\n", jpg_buf);
	aiw->loader = gdk_pixbuf_loader_new();
	gdk_pixbuf_loader_write(aiw->loader, jpg_buf, count);
	gdk_pixbuf_loader_close(aiw->loader);
	free(jpg_buf);

	pixbuf = gdk_pixbuf_loader_get_pixbuf(aiw->loader);

	if(!pixbuf) {
		return 0;
	}

	gdk_pixbuf_render_pixmap_and_mask(pixbuf, &gdkpixmap, &mask, 0);
	gdk_pixbuf_unref(pixbuf);

	gtk_pixmap_set(GTK_PIXMAP(aiw->pixmap), gdkpixmap, mask);

	gtk_widget_show(aiw->pixmap);

	gdk_pixmap_unref(gdkpixmap);
	return 1;
}

void ay_image_window_update_title(int tag, const char *title)
{
	struct ay_image_wnd * aiw = get_image_wnd_by_tag(tag);

	if(!aiw)
		return;

	gtk_window_set_title(GTK_WINDOW(aiw->window), title);
}

void ay_image_window_close(int tag)
{
	struct ay_image_wnd * aiw = get_image_wnd_by_tag(tag);

	if(!aiw)
		return;

	aiw->callback = aiw->callback_data = NULL;
	
	gtk_widget_destroy(aiw->window);
}

static int _cycle(void *data)
{
	int tag = (int)data;
	int size;
	FILE *img;
	char fn[100];
	static int i;
	unsigned char *in_img;
	snprintf(fn, sizeof(fn), "/home/philip/webcam-images/ayttm-img-%03d.jpc", i);
	img = fopen(fn, "rb");
	if(!img) {
		fprintf(stderr, "Could not open %s\n", fn);
		return 1;
	}
	fseek(img, 0, SEEK_END);
	size = ftell(img);
	fseek(img, 0, SEEK_SET);

	in_img = malloc(size);
	fprintf(stderr, "Wanted %d, got %d\n", size, fread(in_img, 1, size, img));
	fclose(img);

	ay_image_window_add_data(tag, in_img, size, 1);
	ay_image_window_add_data(tag, 0, 0, 0);

	free(in_img);

	i++;

	if(i==65)
		return 0;

	return 1;
}

int eb_timeout_add(int, int (*callback)(void *), void *);
void ay_image_window_test()
{
	int tag = ay_image_window_new(320, 240, 0,0,0);
	eb_timeout_add(500, _cycle, (void *)tag);
}
#endif	/* HAVE_GDK_PIXBUF */
