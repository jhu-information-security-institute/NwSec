/*
  Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
  2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015
  Free Software Foundation, Inc.

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

#include <intalkd.h>

#define D(c) #c
#define NITEMS(a) (sizeof (a) / sizeof ((a)[0]))

static const char *message_types[] = {
  D (LEAVE_INVITE),
  D (LOOK_UP),
  D (DELETE),
  D (ANNOUNCE)
};

static const char *answers[] = {
  D (SUCCESS),
  D (NOT_HERE),
  D (FAILED),
  D (MACHINE_UNKNOWN),
  D (PERMISSION_DENIED),
  D (UNKNOWN_REQUEST),
  D (BADVERSION),
  D (BADADDR),
  D (BADCTLADDR)
};


static const char *
_xlat_num (int num, const char *array[], int size)
{
  static char buf[64];

  if (num >= size)
    {
      snprintf (buf, sizeof buf, "%d", num);
      return buf;
    }
  else
    return array[num];
}

int
print_request (const char *cp, CTL_MSG * mp)
{
  syslog (LOG_DEBUG, "%s: %s: id %d, l_user %s, r_user %s, r_tty %s",
	  cp, _xlat_num (mp->type, message_types, NITEMS (message_types)),
	  mp->id_num, mp->l_name, mp->r_name, mp->r_tty);
  return 0;
}

int
print_response (const char *cp, CTL_RESPONSE * rp)
{
  syslog (LOG_DEBUG, "%s: %s: %s, id %d",
	  cp,
	  _xlat_num (rp->type, message_types, NITEMS (message_types)),
	  _xlat_num (rp->answer, answers, NITEMS (answers)),
	  ntohl (rp->id_num));
  return 0;
}
