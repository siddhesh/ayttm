/* This file is part of the Project Athena Zephyr Notification System.
 * It contains source for ZPeekNotice function.
 *
 *	Created by:	Robert French
 *
 *	$Id: ZPeekNot.c,v 1.1 2003/04/01 07:24:51 colinleroy Exp $
 *
 *	Copyright (c) 1987 by the Massachusetts Institute of Technology.
 *	For copying and distribution information, see the file
 *	"mit-copyright.h". 
 */

#ifndef lint
static char rcsid_ZPeekNotice_c[] = "$Id: ZPeekNot.c,v 1.1 2003/04/01 07:24:51 colinleroy Exp $";
#endif

#include <internal.h>

Code_t ZPeekNotice(notice, from)
    ZNotice_t *notice;
    struct sockaddr_in *from;
{
    char *buffer;
    int len;
    Code_t retval;
	
    if ((retval = ZPeekPacket(&buffer, &len, from)) != ZERR_NONE)
	return (retval);

    return (ZParseNotice(buffer, len, notice));
}
