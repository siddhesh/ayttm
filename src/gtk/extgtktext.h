/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

/* 
 * Modified by the AOL Instant Messenger (SM) Development Team in August 2001. 
 * Besides adding support for images and action-requiring 
 * (eg. hyperlinked) text, the following was also applied: 
 * Underline Patch (#2) for GtkText by Patrick Earl 
 * (located at http://www.informatik.fh-hamburg.de/pub/linux/gtk/patches/). 
 * In accordance with LGPL, the files that were modified 
 * for use in the Linux AIM software are provided free of charge 
 * with this distribution and on the AOL Linux AIM website. 
 * The modifications were localized to gtktext widget. 
 */


#ifndef __EXT_GTK_TEXT_H__
#define __EXT_GTK_TEXT_H__

#include <gdk/gdk.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtkeditable.h>
#ifdef HAVE_LIBXFT
#include <X11/Xft/Xft.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef HAVE_LIBXFT
#define VFont GdkFont
#else
#define VFont XftFont
#endif

#define EXT_GTK_TYPE_TEXT                  (ext_gtk_text_get_type ())
#define GTK_SCTEXT(obj)                  (GTK_CHECK_CAST ((obj), EXT_GTK_TYPE_TEXT, ExtGtkText))
#define EXT_GTK_TEXT(obj)                  (GTK_CHECK_CAST ((obj), EXT_GTK_TYPE_TEXT, ExtGtkText))
#define EXT_GTK_TEXT_CLASS(klass)          (GTK_CHECK_CLASS_CAST ((klass), EXT_GTK_TYPE_TEXT, ExtGtkTextClass))
#define EXT_GTK_IS_TEXT(obj)               (GTK_CHECK_TYPE ((obj), EXT_GTK_TYPE_TEXT))
#define EXT_GTK_IS_TEXT_CLASS(klass)       (GTK_CHECK_CLASS_TYPE ((klass), EXT_GTK_TYPE_TEXT))

typedef struct _ExtGtkTextFont       ExtGtkTextFont;
typedef struct _ExtGtkPropertyMark   ExtGtkPropertyMark; 
typedef struct _ExtGtkText           ExtGtkText;   
typedef struct _ExtGtkTextClass      ExtGtkTextClass; 

typedef void   DataFunc           (GdkWindow * window, gpointer info);


struct _ExtGtkPropertyMark
{
  /* Position in list. */
  GList* property;

  /* Offset into that property. */
  guint offset;

  /* Current index. */
  guint index;
};

struct _ExtGtkText
{
  GtkEditable editable;

  GdkWindow *text_area;

  GtkAdjustment *hadj;
  GtkAdjustment *vadj;

  GdkGC *gc;


		      /* GAPPED TEXT SEGMENT */

  /* The text, a single segment of text a'la emacs, with a gap
   * where insertion occurs. */
  union { GdkWChar *wc; guchar  *ch; } text;
  /* The allocated length of the text segment. */
  guint text_len;
  /* The gap position, index into address where a char
   * should be inserted. */
  guint gap_position;
  /* The gap size, s.t. *(text + gap_position + gap_size) is
   * the first valid character following the gap. */
  guint gap_size;
  /* The last character position, index into address where a
   * character should be appeneded.  Thus, text_end - gap_size
   * is the length of the actual data. */
  guint text_end;
			/* LINE START CACHE */

  /* A cache of line-start information.  Data is a LineParam*. */
  GList *line_start_cache;
  /* Index to the start of the first visible line. */
  guint first_line_start_index;
  /* The number of pixels cut off of the top line. */
  guint first_cut_pixels;
  /* First visible horizontal pixel. */
  guint first_onscreen_hor_pixel;
  /* First visible vertical pixel. */
  guint first_onscreen_ver_pixel;

			     /* FLAGS */

  /* True iff this buffer is wrapping lines, otherwise it is using a
   * horizontal scrollbar. */
  guint line_wrap : 1;
  guint word_wrap : 1;
 /* If a fontset is supplied for the widget, use_wchar become true,
   * and we use GdkWchar as the encoding of text. */
  guint use_wchar : 1;

  /* Frozen, don't do updates. @@@ fixme */
  guint freeze_count;
			/* TEXT PROPERTIES */

  /* A doubly-linked-list containing TextProperty objects. */
  GList *text_properties;
  /* The end of this list. */
  GList *text_properties_end;
  /* The first node before or on the point along with its offset to
   * the point and the buffer's current point.  This is the only
   * PropertyMark whose index is guaranteed to remain correct
   * following a buffer insertion or deletion. */
  ExtGtkPropertyMark point;

			  /* SCRATCH AREA */

  union { GdkWChar *wc; guchar *ch; } scratch_buffer;
  guint   scratch_buffer_len;

			   /* SCROLLING */

  gint last_ver_value;

			     /* CURSOR */

  gint            cursor_pos_x;       /* Position of cursor. */
  gint            cursor_pos_y;       /* Baseline of line cursor is drawn on. */
  ExtGtkPropertyMark cursor_mark;        /* Where it is in the buffer. */
  GdkWChar        cursor_char;        /* Character to redraw. */
  gchar           cursor_char_offset; /* Distance from baseline of the font. */
  gint            cursor_virtual_x;   /* Where it would be if it could be. */
  gint            cursor_drawn_level; /* How many people have undrawn. */

			  /* Current Line */

  GList *current_line;

			   /* Tab Stops */

  GList *tab_stops;
  gint default_tab_width;

  ExtGtkTextFont *current_font;	/* Text font for current style */

  /* Timer used for auto-scrolling off ends */
  gint timer;
  
  guint button;			/* currently pressed mouse button */
  GdkGC *bg_gc;			/* gc for drawing background pixmap */
};

struct _ExtGtkTextClass
{
  GtkEditableClass parent_class;

  void  (*set_scroll_adjustments)   (ExtGtkText	    *text,
				     GtkAdjustment  *hadjustment,
				     GtkAdjustment  *vadjustment);
};

GtkType    ext_gtk_text_get_type        (void);
GtkWidget* ext_gtk_text_new             (GtkAdjustment *hadj,
				     GtkAdjustment *vadj);
void       ext_gtk_text_set_editable    (ExtGtkText       *text,
				     gboolean       editable);
void       ext_gtk_text_set_word_wrap   (ExtGtkText       *text,
				     gint           word_wrap);
void       ext_gtk_text_set_line_wrap   (ExtGtkText       *text,
				     gint           line_wrap);
void       ext_gtk_text_set_adjustments (ExtGtkText       *text,
				     GtkAdjustment *hadj,
				     GtkAdjustment *vadj);
void       ext_gtk_text_set_point       (ExtGtkText       *text,
				     guint          index);
guint      ext_gtk_text_get_point       (ExtGtkText       *text);
guint      ext_gtk_text_get_length      (ExtGtkText       *text);
void       ext_gtk_text_freeze          (ExtGtkText       *text);
void       ext_gtk_text_thaw            (ExtGtkText       *text);
void       ext_gtk_text_insert          (ExtGtkText       *text,
				     VFont	   *font,
				     GdkColor      *fore,
				     GdkColor      *back,
				     const char    *chars,
				     gint           length);
void       ext_gtk_text_insert_underlined(ExtGtkText       *text,
				     VFont *font,
				     GdkColor      *fore,
				     GdkColor      *back,
				     const char    *chars,
				     gint           length);
void
ext_gtk_text_insert_divider(ExtGtkText    *text,
		            VFont *font,
			    GdkColor   *fore,
			    GdkColor   *back,
			    const char *chars,
			    gint        nchars);

void       ext_gtk_text_insert_pixmap(ExtGtkText       *text,
				     VFont *font,
				     GdkColor      *fore,
				     GdkColor      *back,
                                     GdkDrawable   *image,
				     GdkBitmap	   *mask,
				     const char    *chars,
				     gint           length);
void       ext_gtk_text_insert_data_underlined (ExtGtkText       *text,
				     VFont *font,
				     GdkColor      *fore,
				     GdkColor      *back,
                                     gpointer      user_data,
                                     guint         user_data_length,
                                     DataFunc      *user_data_func, 
				     const char    *chars,
				     gint           length);
void       ext_gtk_text_insert_data (ExtGtkText       *text,
				     VFont *font,
				     GdkColor      *fore,
				     GdkColor      *back,
                                     gpointer      user_data,
                                     guint         user_data_length,
                                     DataFunc      *user_data_func, 
				     const char    *chars,
				     gint           length);
gint       ext_gtk_text_backward_delete (ExtGtkText       *text,
				     guint          nchars);
gint       ext_gtk_text_forward_delete  (ExtGtkText       *text,
				     guint          nchars);

#define EXT_GTK_TEXT_INDEX(t, index)	(((t)->use_wchar) \
	? ((index) < (t)->gap_position ? (t)->text.wc[index] : \
					(t)->text.wc[(index)+(t)->gap_size]) \
	: ((index) < (t)->gap_position ? (t)->text.ch[index] : \
					(t)->text.ch[(index)+(t)->gap_size]))

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __EXT_GTK_TEXT_H__ */
