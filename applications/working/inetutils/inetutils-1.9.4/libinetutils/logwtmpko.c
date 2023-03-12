/*
  Copyright (C) 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
  2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015 Free Software
  Foundation, Inc.

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

#if defined HAVE_UPDWTMPX && !defined HAVE_LOGWTMP
# include <utmpx.h>	/* updwtmp() */
#endif

#define KEEP_OPEN
#include "logwtmp.c"

/* Solaris does not provide logwtmp(3c).
 * It is needed in addition to logwtmp_keep_open().
 */
#if defined HAVE_UPDWTMPX && !defined HAVE_LOGWTMP
void
logwtmp (char *line, char *name, char *host)
{
  struct utmpx ut;
  struct timeval tv;

  memset (&ut, 0, sizeof (ut));
#ifdef HAVE_STRUCT_UTMPX_UT_TYPE
  if (name && *name)
    ut.ut_type = USER_PROCESS;
  else
    ut.ut_type = DEAD_PROCESS;
#endif /* UT_TYPE */

  strncpy (ut.ut_line, line, sizeof ut.ut_line);
#ifdef HAVE_STRUCT_UTMPX_UT_USER
  strncpy (ut.ut_user, name, sizeof ut.ut_user);
#elif defined HAVE_STRUCT_UTMPX_UT_NAME
  strncpy (ut.ut_name, name, sizeof ut.ut_name);
#endif
#ifdef HAVE_STRUCT_UTMPX_UT_HOST
  strncpy (ut.ut_host, host, sizeof ut.ut_host);
# ifdef HAVE_STRUCT_UTMPX_UT_SYSLEN
  if (strlen (host) < sizeof (ut.ut_host))
    ut.ut_syslen = strlen (host) + 1;
  else
    {
      ut.ut_host[sizeof (ut.ut_host) - 1] = '\0';
      ut.ut_syslen = sizeof (ut.ut_host);
    }
# endif /* UT_SYSLEN */
#endif
#ifdef HAVE_STRUCT_UTMPX_UT_PID
  ut.ut_pid = getpid ();
#endif

  gettimeofday (&tv, NULL);
  ut.ut_tv.tv_sec = tv.tv_sec;
  ut.ut_tv.tv_usec = tv.tv_usec;

  updwtmpx (PATH_WTMPX, &ut);
}
#endif /* HAVE_UPDWTMPX && !HAVE_LOGWTMP */
