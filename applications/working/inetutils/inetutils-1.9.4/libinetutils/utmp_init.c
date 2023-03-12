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
#include <unistd.h>

/* utmp_init - update utmp and wtmp before login */

void
utmp_init (char *line, char *user, char *id, char *host)
{
#ifdef HAVE_UTMPX_H
  struct utmpx utx;
  struct timeval tv;
#else /* !HAVE_UTMPX_H */
  struct utmp utx;
# if defined HAVE_STRUCT_UTMP_UT_TV
  struct timeval tv;
# endif
#endif

  memset ((char *) &utx, 0, sizeof (utx));
#if defined HAVE_STRUCT_UTMP_UT_ID || defined HAVE_STRUCT_UTMPX_UT_ID
  strncpy (utx.ut_id, id, sizeof (utx.ut_id));
#endif
#if defined HAVE_STRUCT_UTMP_UT_USER || defined HAVE_STRUCT_UTMPX_UT_USER
  strncpy (utx.ut_user, user, sizeof (utx.ut_user));
#elif defined HAVE_STRUCT_UTMP_UT_NAME || defined HAVE_STRUCT_UTMPX_UT_NAME
  strncpy (utx.ut_name, user, sizeof (utx.ut_name));
#endif
#if defined HAVE_STRUCT_UTMP_UT_HOST || defined HAVE_STRUCT_UTMPX_UT_HOST
  strncpy (utx.ut_host, host, sizeof (utx.ut_host));
# ifdef HAVE_STRUCT_UTMPX_UT_SYSLEN	/* Only utmpx.  */
  if (strlen (host) < sizeof (utx.ut_host))
    utx.ut_syslen = strlen (host) + 1;
  else
    {
      utx.ut_host[sizeof (utx.ut_host) - 1] = '\0';
      utx.ut_syslen = sizeof (utx.ut_host);
    }
# endif
#endif /* UT_HOST */
#if defined HAVE_STRUCT_UTMP_UT_LINE || defined HAVE_STRUCT_UTMPX_UT_LINE
  strncpy (utx.ut_line, line, sizeof (utx.ut_line));
#endif

#if defined HAVE_STRUCT_UTMP_UT_PID || defined HAVE_STRUCT_UTMPX_UT_PID
  utx.ut_pid = getpid ();
#endif
#if defined HAVE_STRUCT_UTMP_UT_TYPE || defined HAVE_STRUCT_UTMPX_UT_TYPE
  utx.ut_type = LOGIN_PROCESS;
#endif
#if defined HAVE_STRUCT_UTMP_UT_TV || defined HAVE_STRUCT_UTMPX_UT_TV
  gettimeofday (&tv, 0);
  utx.ut_tv.tv_sec = tv.tv_sec;
  utx.ut_tv.tv_usec = tv.tv_usec;
#else
  time (&(utx.ut_time));
#endif

/* Prefer utmpx over utmp, and attempt to
 * use pututxline/pututline for writing the
 * initial entry.  Then apply whatever
 * wtmp updating that happens to be available.
 *
 * That failing, fall back to loginx/login.
 * This is made in order than we are granted
 * LOGIN_PROCESS type and stay unbound by
 * any tty sensing of stdin, stdout, or stderr,
 * like GNU libc would do in login().
 */
#ifdef HAVE_UTMPX_H
# ifdef HAVE_PUTUTXLINE
  setutxent ();
  pututxline (&utx);
  /* Some systems perform wtmp updating
   * already in calling pututxline().
   */
#  ifdef HAVE_UPDWTMPX
  updwtmpx (PATH_WTMPX, &utx);
#  elif defined HAVE_LOGWTMPX
  logwtmpx (line, user, id, 0, LOGIN_PROCESS);
#  endif /* wtmp updating */
  endutxent ();
# elif defined HAVE_LOGINX /* !HAVE_PUTUTXLINE */
  loginx (&utx, 0, LOGIN_PROCESS);
# endif /* HAVE_LOGINX && !HAVE_PUTUTXLINE */

#else /* !HAVE_UTMPX_H */
# ifdef HAVE_PUTUTLINE
  setutent ();
  pututline (&utx);
#  ifdef HAVE_UPDWTMP
  updwtmp (PATH_WTMP, &utx);
#  elif defined HAVE_LOGWTMP /* !HAVE_UPDWTMP */
  logwtmp (line, user, id);
#  endif /* wtmp updating */
  endutent ();
# elif defined HAVE_LOGIN /* !HAVE_PUTUTLINE */
  (void) id;		/* Silence warnings.  */
  login (&utx);
# endif /* HAVE_LOGIN && !HAVE_PUTUTLINE */
#endif /* !HAVE_UTMPX_H */
}

/* utmp_ptsid - generate utmp id for pseudo terminal */

char *
utmp_ptsid (char *line, char *tag)
{
  static char buf[5];

  strncpy (buf, tag, 2);
  strncpy (buf + 2, line + strlen (line) - 2, 2);
  return (buf);
}
