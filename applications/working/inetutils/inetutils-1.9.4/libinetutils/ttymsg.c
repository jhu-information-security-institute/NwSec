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

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <config.h>

#include <sys/types.h>
#include <sys/uio.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>

#define MAX_ERRBUF 1024

static int fork2 (void);
static char *normalize_path (char *path, const char *delim);

/*
 * Display the contents of a uio structure on a terminal.  Used by wall(1),
 * syslogd(8), and talkd(8).  Forks and finishes in child if write would block,
 * waiting up to tmout seconds.  Returns pointer to error string on unexpected
 * error; string is not newline-terminated.  Various "normal" errors are
 * ignored (exclusive-use, lack of permission, etc.).
 */
char *
ttymsg (struct iovec *iov, int iovcnt, char *line, int tmout)
{
  static char errbuf[MAX_ERRBUF];
  char *device;
  register int cnt, fd, left, wret;
  struct iovec localiov[6];
  int forked = 0;

  if (iovcnt > (int) (sizeof (localiov) / sizeof (localiov[0])))
    return (char *) ("too many iov's (change code in wall/ttymsg.c)");

  device = malloc (sizeof PATH_TTY_PFX - 1 + strlen (line) + 1);
  if (!device)
    {
      snprintf (errbuf, sizeof errbuf,
		"Not enough memory for tty device name");
      return errbuf;
    }

  strcpy (device, PATH_TTY_PFX);
  strcat (device, line);
  normalize_path (device, "/");
  if (strncmp (device, PATH_TTY_PFX, strlen (PATH_TTY_PFX)))
    {
      /* An attempt to break security... */
      snprintf (errbuf, sizeof (errbuf), "bad line name: %s", line);
      return (errbuf);
    }

  /*
   * open will fail on slip lines or exclusive-use lines
   * if not running as root; not an error.
   */
  fd = open (device, O_WRONLY | O_NONBLOCK, 0);
  if (fd < 0)
    {
      if (errno == EBUSY || errno == EACCES)
	return (NULL);
      snprintf (errbuf, sizeof (errbuf), "%s: %s", device, strerror (errno));
      free (device);
      return errbuf;
    }

  for (cnt = left = 0; cnt < iovcnt; ++cnt)
    left += iov[cnt].iov_len;

  for (;;)
    {
      wret = writev (fd, iov, iovcnt);
      if (wret >= left)
	break;
      if (wret >= 0)
	{
	  left -= wret;
	  if (iov != localiov)
	    {
	      memcpy (localiov, iov, iovcnt * sizeof (struct iovec));
	      iov = localiov;
	    }
	  for (cnt = 0; wret >= (int) iov->iov_len; ++cnt)
	    {
	      wret -= iov->iov_len;
	      ++iov;
	      --iovcnt;
	    }
	  if (wret)
	    {
	      iov->iov_base = (char *) iov->iov_base + wret;
	      iov->iov_len -= wret;
	    }
	  continue;
	}
      if (errno == EWOULDBLOCK)
	{
	  int cpid, off = 0;

	  if (forked)
	    {
	      close (fd);
	      _exit (EXIT_FAILURE);
	    }
	  cpid = fork2 ();
	  if (cpid < 0)
	    {
	      snprintf (errbuf, sizeof (errbuf),
			"fork: %s", strerror (errno));
	      close (fd);
	      free (device);
	      return (errbuf);
	    }
	  if (cpid)		/* Parent.  */
	    {
	      close (fd);
	      free (device);
	      return (NULL);
	    }
	  forked++;
	  /* wait at most tmout seconds */
	  signal (SIGALRM, SIG_DFL);
	  signal (SIGTERM, SIG_DFL);	/* XXX */
#ifdef HAVE_SIGACTION
	  {
	    sigset_t empty;
	    sigemptyset (&empty);
	    sigprocmask (SIG_SETMASK, &empty, 0);
	  }
#else
	  sigsetmask (0);
#endif
	  alarm ((u_int) tmout);
	  fcntl (fd, O_NONBLOCK, &off);
	  continue;
	}
      /*
       * We get ENODEV on a slip line if we're running as root,
       * and EIO if the line just went away.
       */
      if (errno == ENODEV || errno == EIO)
	break;
      close (fd);
      if (forked)
	_exit (EXIT_FAILURE);
      snprintf (errbuf, sizeof (errbuf), "%s: %s", device, strerror (errno));
      free (device);
      return (errbuf);
    }

  free (device);
  close (fd);
  if (forked)
    _exit (EXIT_SUCCESS);
  return (NULL);
}


/* This was part of the Unix-Faq, maintain by Andrew Gierth.
   fork2() -- like fork, but the new process is immediately orphaned
   (won't leave a zombie when it exits)
   Returns 1 to the parent, not any meaningful pid.
   The parent cannot wait() for the new process (it's unrelated).

   This version assumes that you *haven't* caught or ignored SIGCHLD.
   If you have, then you should just be using fork() instead anyway.  */

static int
fork2 (void)
{
  pid_t pid;
  int status;

  if (!(pid = fork ()))
    {
      switch (fork ())
	{
	case 0:		/* Child.  */
	  return 0;
	case -1:
	  _exit (errno);	/* Assumes all errnos are <256 */
	default:		/* Parent.  */
	  _exit (EXIT_SUCCESS);
	}
    }

  if (pid < 0 || waitpid (pid, &status, 0) < 0)
    return -1;

  if (WIFEXITED (status))
    if (WEXITSTATUS (status) == 0)
      return 1;
    else
      errno = WEXITSTATUS (status);
  else
    errno = EINTR;		/* well, sort of :-) */

  return -1;
}

char *
normalize_path (char *path, const char *delim)
{
  int len;
  char *p;

  if (!path)
    return path;

  len = strlen (path);

  /* Empty string is returned as is */
  if (len == 0)
    return path;

  /* delete trailing delimiter if any */
  if (len && path[len - 1] == delim[0])
    path[len - 1] = 0;

  /* Eliminate any /../ */
  for (p = strchr (path, '.'); p; p = strchr (p, '.'))
    {
      if (p > path && p[-1] == delim[0])
	{
	  if (p[1] == '.' && (p[2] == 0 || p[2] == delim[0]))
	    /* found */
	    {
	      char *q, *s;

	      /* Find previous delimiter */
	      for (q = p - 2; *q != delim[0] && q >= path; q--)
		;

	      if (q < path)
		break;
	      /* Copy stuff */
	      s = p + 2;
	      p = q;
	      while ((*q++ = *s++))
		;
	      continue;
	    }
	}

      p++;
    }

  if (path[0] == 0)
    {
      path[0] = delim[0];
      path[1] = 0;
    }

  return path;
}
