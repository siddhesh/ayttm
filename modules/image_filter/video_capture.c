/*
 * VideoCapture Plugin for Ayttm
 *
 * Copyright (C) 2003, Philip S Tellis <bluesmoon @ users.sourceforge.net>
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

#include "intl.h"
#ifdef __MINGW32__
#define __IN_PLUGIN__ 1
#endif
#include <stdio.h>
#include <string.h>
#include "plugin_api.h"
#include "video.h"
#include "mem_util.h"
#include "debug.h"

/*******************************************************************************
 *                             Begin Module Code
 ******************************************************************************/
/*  Module defines */
#define plugin_info video_capture_LTX_plugin_info
#define module_version video_capture_LTX_module_version


static int plugin_init();
static int plugin_finish();

static char frame_grabber[MAX_PREF_LEN]= AYTTM_BIN_DIR "/ayttm_streamer_wrapper.sh -d %d";

/*  Module Exports */
PLUGIN_INFO plugin_info = {
	PLUGIN_UTILITY,
	"Video Capture",
	"Capture video images from some source",
	"$Revision: 1.2 $",
	"$Date: 2003/12/21 07:37:25 $",
	NULL,
	plugin_init,
	plugin_finish,
	NULL,
	NULL
};
/* End Module Exports */

unsigned int module_version() {return CORE_VERSION;}

static long int grab_frame(unsigned char **image);

static long int (*old_grab_frame)(unsigned char **);

static int plugin_init()
{
	input_list *il = ay_new0(input_list, 1);

	plugin_info.prefs = il;
	il->widget.entry.value = frame_grabber;
	il->name = "frame_grabber";
	il->label= _("Frame grabber command:");
	il->type = EB_INPUT_ENTRY;

	old_grab_frame = video_grab_frame;
	video_grab_frame = grab_frame;

	return 0;
}

static int plugin_finish()
{
	video_grab_frame = old_grab_frame;

	return 0;
}

/*******************************************************************************
 *                             End Module Code
 ******************************************************************************/

static long int grab_frame(unsigned char **image)
{
	FILE *grabber_fp;
	unsigned char buff[1024];
	unsigned char *outbuf=NULL;
	long int nsize=0, n;

	char *cmd = strdup(frame_grabber);
	while(strstr(cmd, "%d")) {
		char *tmp = strstr(cmd, "%d");
		char *rest=NULL;
		if(*(tmp+2))
			rest = strdup(tmp+2);
		*tmp = NULL;
		cmd = ay_string_append(cmd, eb_config_dir());
		if(rest)
			cmd = ay_string_append(cmd, rest);
	}

	grabber_fp = popen(cmd, "r");
	ay_free(cmd);

	if(!grabber_fp)
		return -1;

	while(! (feof(grabber_fp) || ferror(grabber_fp)) ) {
		n = fread(buff, 1, sizeof(buff), grabber_fp);
		outbuf = ay_renew(unsigned char, outbuf, n+nsize);
		memcpy(outbuf+nsize, buff, n);
		nsize+=n;
	}

	pclose(grabber_fp);

	*image = outbuf;
	return nsize;
}

