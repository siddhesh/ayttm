AM_CFLAGS = $(GLIB_CFLAGS) $(JASPER_CFLAGS) -I$(top_srcdir)/src -DAYTTM_BIN_DIR=\"${bindir}\"

if MINGW32
WIN32_LDFLAGS = -no-undefined
WIN32_LIBADD = -L../../src -layttm -lintl -lglib-2.0
endif

pkglib_LTLIBRARIES = img2jpc.la video_capture.la
bin_SCRIPTS = ayttm_streamer_wrapper.sh

img2jpc_la_SOURCES = img2jpc.c 
img2jpc_la_LDFLAGS = -module -avoid-version $(WIN32_LDFLAGS)
img2jpc_la_LIBADD = $(JASPER_LIBS) $(WIN32_LIBADD)

video_capture_la_SOURCES = video_capture.c
video_capture_la_LDFLAGS = -module -avoid-version $(WIN32_LDFLAGS)
video_capture_la_LIBADD = $(WIN32_LIBADD)
