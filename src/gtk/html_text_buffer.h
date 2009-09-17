#ifndef __HTML_TEXT_BUFFER_H__
#define __HTML_TEXT_BUFFER_H__

#include <gtk/gtk.h>
#include <string.h>

#define	HTML_IGNORE_NONE	0
#define	HTML_IGNORE_FOREGROUND	1
#define	HTML_IGNORE_BACKGROUND	2
#define	HTML_IGNORE_FONT	4

#ifdef __cplusplus
extern "C" {
#endif

	void html_text_buffer_append(GtkTextView *text_view, char *text,
		int ignore);

	void html_text_view_init(GtkTextView *text_view, int ignore_font);

#ifdef __cplusplus
}
#endif
#endif
