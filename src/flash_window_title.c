#ifndef __MINGW32__
void flash_title(GdkWindow *window)
{
}
#else
#include "plugin_api.h"
#include "gtk/gtkutils.h"
#include <gdk/gdkwin32.h>

void flash_title_stop(GdkWindow *window)
{
	FlashWindow(GDK_DRAWABLE_XID(window),FALSE);
}

void flash_title(GdkWindow *window)
{
	FlashWindow(GDK_DRAWABLE_XID(window),TRUE);
	eb_timeout_add(2000,(GtkFunction)flash_title_stop, (gpointer) window);
}
#endif
