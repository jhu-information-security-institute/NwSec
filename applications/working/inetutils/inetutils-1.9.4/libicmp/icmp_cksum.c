/*
  Copyright (C) 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009,
  2010, 2011, 2012, 2013, 2014, 2015 Free Software Foundation, Inc.

  This file is part of GNU Inetutils.

  GNU Inetutils is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or (at
  your option) any later version.

  GNU Inetutils is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see `http://www.gnu.org/licenses/'. */

#include <config.h>

#include <sys/types.h>
#include <unistd.h>

unsigned short
icmp_cksum (unsigned char * addr, int len)
{
  register int sum = 0;
  unsigned short answer = 0;
  unsigned short *wp;

  for (wp = (unsigned short *) addr; len > 1; wp++, len -= 2)
    sum += *wp;

  /* Take in an odd byte if present */
  if (len == 1)
    {
      *(unsigned char *) & answer = *(unsigned char *) wp;
      sum += answer;
    }

  sum = (sum >> 16) + (sum & 0xffff);	/* add high 16 to low 16 */
  sum += (sum >> 16);		/* add carry */
  answer = ~sum;		/* truncate to 16 bits */
  return answer;
}
