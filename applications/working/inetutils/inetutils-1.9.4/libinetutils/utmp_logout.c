/*
  Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007,
  2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015 Free Software
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

/*
 * Copyright (c) 1995 Wietse Venema.  All rights reserved.
 *
 * Individual files may be covered by other copyrights (as noted in
 * the file itself.)
 *
 * This material was originally written and compiled by Wietse Venema
 * at Eindhoven University of Technology, The Netherlands, in 1990,
 * 1991, 1992, 1993, 1994 and 1995.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this entire copyright notice is duplicated in all
 * such copies.
 *
 * This software is provided "as is" and without any expressed or
 * implied warranties, including, without limitation, the implied
 * warranties of merchantibility and fitness for any particular
 * purpose.
 */

/* Written by Wietse Venema.  With port to GNU Inetutils done by Alain
   Magloire.  Reorganized to cope with full variation between utmpx
   and utmp, with different API sets, by Mats Erik Andersson.  */

#include <config.h>

#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#ifdef HAVE_UTMPX_H
# ifndef __USE_GNU
#  define __USE_GNU 1
# endif
# include <utmpx.h>
#else /* !HAVE_UTMPX_H */
# ifdef HAVE_UTIL_H
#  include <util.h>
# endif
# ifdef HAVE_LIBUTIL_H
#  include <libutil.h>
# endif
# include <utmp.h>
#endif
#include <string.h>

/* utmp_logout - update utmp and wtmp after logout */

void
utmp_logout (char *line)
{
#ifdef HAVE_UTMPX_H
  struct utmpx utx;
  struct utmpx *ut;

  strncpy (utx.ut_line, line, sizeof (utx.ut_line));

# ifdef HAVE_PUTUTXLINE
  setutxent();
  ut = getutxline (&utx);
  if (ut)
    {
      struct timeval tv;

      ut->ut_type = DEAD_PROCESS;
#  ifdef HAVE_STRUCT_UTMPX_UT_EXIT
      memset (&ut->ut_exit, 0, sizeof (ut->ut_exit));
#  endif
      gettimeofday (&tv, 0);
      ut->ut_tv.tv_sec = tv.tv_sec;
      ut->ut_tv.tv_usec = tv.tv_usec;
#  ifdef HAVE_STRUCT_UTMPX_UT_USER
      memset (&ut->ut_user, 0, sizeof (ut->ut_user));
#  elif defined HAVE_STRUCT_UTMPX_UT_NAME
      memset (&ut->ut_name, 0, sizeof (ut->ut_name));
#  endif
#  ifdef HAVE_STRUCT_UTMPX_UT_HOST
      memset (ut->ut_host, 0, sizeof (ut->ut_host));
#   ifdef HAVE_STRUCT_UTMPX_UT_SYSLEN
      ut->ut_syslen = 1;	/* Counting NUL.  */
#   endif
#  endif /* UT_HOST */
      pututxline (ut);
      /* Some systems perform wtmp updating
       * already in calling pututxline().
       */
#  ifdef HAVE_UPDWTMPX
      updwtmpx (PATH_WTMPX, ut);
#  elif defined HAVE_LOGWTMPX
      logwtmpx (ut->ut_line, "", "", 0, DEAD_PROCESS);
#  endif
    }
  endutxent ();
# elif defined HAVE_LOGOUTX /* !HAVE_PUTUTXLINE */
  if (logoutx (line, 0, DEAD_PROCESS))
    logwtmpx (line, "", "", 0, DEAD_PROCESS);
# endif /* HAVE_LOGOUTX */

#else /* !HAVE_UTMPX_H */
  struct utmp utx;
# ifdef HAVE_PUTUTLINE
  struct utmp *ut;
# endif

  strncpy (utx.ut_line, line, sizeof (utx.ut_line));

# ifdef HAVE_PUTUTLINE
  setutent();
  ut = getutline (&utx);
  if (ut)
    {
#  ifdef HAVE_STRUCT_UTMP_UT_TV
      struct timeval tv;
#  endif

#  ifdef HAVE_STRUCT_UTMP_UT_TYPE
      ut->ut_type = DEAD_PROCESS;
#  endif
#  ifdef HAVE_STRUCT_UTMP_UT_EXIT
      memset (&ut->ut_exit, 0, sizeof (ut->ut_exit));
#  endif
#  ifdef HAVE_STRUCT_UTMP_UT_TV
      gettimeofday (&tv, 0);
      ut->ut_tv.tv_sec = tv.tv_sec;
      ut->ut_tv.tv_usec = tv.tv_usec;
#  else /* !HAVE_STRUCT_UTMP_UT_TV */
      time (&(ut->ut_time));
#  endif
#  ifdef HAVE_STRUCT_UTMP_UT_USER
      memset (&ut->ut_user, 0, sizeof (ut->ut_user));
#  elif defined HAVE_STRUCT_UTMP_UT_NAME
      memset (&ut->ut_name, 0, sizeof (ut->ut_name));
#  endif
#  ifdef HAVE_STRUCT_UTMP_UT_HOST
      memset (ut->ut_host, 0, sizeof (ut->ut_host));
#  endif
      pututline (ut);
#  ifdef HAVE_UPDWTMP
      updwtmp (WTMP_FILE, ut);
#  elif defined HAVE_LOGWTMP /* !HAVE_UPDWTMP */
      logwtmp (ut->ut_line, "", "");
#  endif
    }
  endutent ();
# elif defined HAVE_LOGOUT /* !HAVE_PUTUTLINE */
  if (logout (line))
    logwtmp (line, "", "");
# endif /* HAVE_LOGOUT */
#endif
}
