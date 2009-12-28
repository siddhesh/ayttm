#include "video.h"

long int (*video_grab_frame) (unsigned char **) =
	(long int (*)(unsigned char **))0;
