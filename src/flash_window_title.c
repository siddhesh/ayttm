#ifndef __MINGW32__
void flash_title(GdkWindow *window)
{
}
#else
#include <gdk/gdkwin32.h>

struct s_flashinfo {
	unsigned long cbsize;
	unsigned long hwnd;
	unsigned long dwflags;
	unsigned long ucount;
	unsigned long dwtimeout;
};

int FlashWindowEx(struct s_flashinfo * flashinfo);

const static int FLASHW_STOP = 0;	// Stop flashing
const static int FLASHW_CAPTION = 1;	// Flash the window caption
const static int FLASHW_TRAY = 2;	// Flash the taskbar button.
const static int FLASHW_ALL  = 3;	// Flash both the window caption and taskbar button.
const static int FLASHW_TIMER = 4;	// Flash continuously, until the FLASHW_STOP flag is set.
const static int FLASHW_TIMERNOFG = 12;	// Flash continuously until the window comes to the foreground

void flash_title(GdkWindow *window)
{
	unsigned long ll_win;
	struct s_flashinfo lstr_flashinfo;

	lstr_flashinfo.cbsize = 20;
	lstr_flashinfo.hwnd = GDK_DRAWABLE_XID(window); 
	lstr_flashinfo.dwflags = FLASHW_TIMERNOFG;
	lstr_flashinfo.ucount = 10;	// 10 times
	lstr_flashinfo.dwtimeout = 0;	// speed in ms, 0 default blink cursor rate

	FlashWindowEx(lstr_flashinfo)

}
#endif
