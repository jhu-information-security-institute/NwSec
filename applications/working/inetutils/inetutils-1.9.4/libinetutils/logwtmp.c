/* logwtmp.c - A version of BSDs `logwtmp' that should be widely portable
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

/* Written by Miles Bader.  */

/* If `KEEP_OPEN' is defined, then a special version of logwtmp is compiled,
   called logwtmp_keep_open, which keeps the wtmp file descriptor open
   between calls, and doesn't attempt to open the file after the first call. */

#include <config.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <errno.h>
#ifdef HAVE_UTMPX_H
# ifndef __USE_GNU
#  define __USE_GNU 1
# endif
# include <utmpx.h>
# define OUR_WTMP PATH_WTMPX
#elif defined HAVE_UTMP_H
# include <utmp.h>
# define OUR_WTMP PATH_WTMP
#endif
#include <string.h>

#if !HAVE_DECL_ERRNO
extern int errno;
#endif

#ifdef HAVE_UTMPX_H
static void
_logwtmp (struct utmpx *ut)
#else /* !HAVE_UTMPX_H */
static void
_logwtmp (struct utmp *ut)
#endif /* !HAVE_UTMP_H */
{
#ifdef KEEP_OPEN
  static int fd = -1;

  if (fd < 0)
    fd = open (OUR_WTMP, O_WRONLY | O_APPEND, 0);
#else
  int fd = open (OUR_WTMP, O_WRONLY | O_APPEND, 0);
#endif

  if (fd >= 0)
    {
      struct stat st;

#ifdef HAVE_FLOCK
      if (flock (fd, LOCK_EX | LOCK_NB) < 0 && errno != ENOSYS)
	{
	  sleep (1);
	  flock (fd, LOCK_EX | LOCK_NB);	/* ignore error */
	}
#endif

#ifdef HAVE_FTRUNCATE
      if (fstat (fd, &st) == 0
	  && write (fd, (char *) ut, sizeof *ut) != sizeof *ut)
	ftruncate (fd, st.st_size);
#else
      write (fd, (char *) ut, sizeof *ut);
#endif

#ifdef HAVE_FLOCK
      flock (fd, LOCK_UN);
#endif

#ifndef KEEP_OPEN
      close (fd);
#endif
    }
}

void
#ifdef KEEP_OPEN
logwtmp_keep_open (char *line, char *name, char *host)
#else
logwtmp (char *line, char *name, char *host)
#endif
{
#ifdef HAVE_UTMPX_H
  struct utmpx ut;
  struct timeval tv;
#else /* !HAVE_UTMPX_H */
  struct utmp ut;
# ifdef HAVE_STRUCT_UTMP_UT_TV
  struct timeval tv;
# endif
#endif

  /* Set information in new entry.  */
  memset (&ut, 0, sizeof (ut));
#if defined HAVE_STRUCT_UTMP_UT_TYPE || defined HAVE_STRUCT_UTMPX_UT_TYPE
  if (name && *name)
    ut.ut_type = USER_PROCESS;
  else
    ut.ut_type = DEAD_PROCESS;
#endif /* UT_TYPE */
#if defined HAVE_STRUCT_UTMP_UT_PID || defined HAVE_STRUCT_UTMPX_UT_PID
  ut.ut_pid = getpid ();
#endif /* UT_PID */

  strncpy (ut.ut_line, line, sizeof ut.ut_line);
#if defined HAVE_STRUCT_UTMP_UT_USER || defined HAVE_STRUCT_UTMPX_UT_USER
  strncpy (ut.ut_user, name, sizeof ut.ut_user);
#elif defined HAVE_STRUCT_UTMP_UT_NAME || defined HAVE_STRUCT_UTMPX_UT_NAME
  strncpy (ut.ut_name, name, sizeof ut.ut_name);
#endif

#if defined HAVE_STRUCT_UTMP_UT_HOST || defined HAVE_STRUCT_UTMPX_UT_HOST
  strncpy (ut.ut_host, host, sizeof ut.ut_host);
# ifdef HAVE_STRUCT_UTMPX_UT_SYSLEN	/* Only utmpx.  */
  if (strlen (host) < sizeof (ut.ut_host))
    ut.ut_syslen = strlen (host) + 1;	/* Including NUL.  */
  else
    {
      ut.ut_host[sizeof (ut.ut_host) - 1] = '\0';
      ut.ut_syslen = sizeof (ut.ut_host);
    }
# endif /* UT_SYSLEN */
#endif /* UT_HOST */

#if defined HAVE_STRUCT_UTMP_UT_TV || defined HAVE_STRUCT_UTMPX_UT_TV
  gettimeofday (&tv, NULL);
  ut.ut_tv.tv_sec = tv.tv_sec;
  ut.ut_tv.tv_usec = tv.tv_usec;
#else
  time (&ut.ut_time);
#endif

  _logwtmp (&ut);
}
