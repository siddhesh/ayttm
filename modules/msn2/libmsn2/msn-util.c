/*
 * Ayttm
 *
 * Copyright (C) 2009, the Ayttm team
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

#include "msn-util.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>

char *msn_urlencode(const char *in)
{
	int ipos = 0, bpos = 0;
	char *str = NULL;
	int len = strlen(in);

	if (!(str = m_new0(char, 3 *len + 1)))
		 return "";

	while (in[ipos]) {
		while (isalnum(in[ipos]) || in[ipos] == '-' || in[ipos] == '_')
			str[bpos++] = in[ipos++];

		if (!in[ipos])
			break;

		snprintf(&str[bpos], 4, "%%%.2x", in[ipos]);
		bpos += 3;
		ipos++;
	}
	str[bpos] = '\0';

	/* free extra alloc'ed mem. */
	len = strlen(str);
	str = m_renew(char, str, len + 1);

	return (str);
}

char *msn_urldecode(const char *in)
{
	int ipos = 0, bpos = 0;
	char *str = NULL;
	int len = strlen(in);

	if (!(str = m_new0(char, len)))
		 return "";

	while (in[ipos]) {
		int num = 0;
		int i = 0;

		while (in[ipos] && in[ipos] != '%')
			str[bpos++] = in[ipos++];

		if (!in[ipos])
			break;

		for (i = 0; i < 2; i++) {
			int tmp = 0;
			ipos++;
			if (in[ipos] <= '9' && in[ipos] >= '0')
				tmp = in[ipos] - '0';
			else
				tmp = 10 + in[ipos] - 'a';

			if (!i)
				num = tmp * 16;
			else
				num += tmp;

		}

		str[bpos++] = num;
		ipos++;
	}
	str[bpos] = '\0';

	/* free extra alloc'ed mem. */
	len = strlen(str);
	str = m_renew(char, str, len + 1);

	return (str);
}
