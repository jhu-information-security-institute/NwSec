/*
  Copyright (C) 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002,
  2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013,
  2014, 2015 Free Software Foundation, Inc.

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
#include <stdarg.h>
#include <sys/uio.h>

#undef MAX
#define MAX(a,b) ( (a) > (b) ? (a) : (b) )
#define N_LINES 5
#define N_CHARS 256

extern char *ttymsg (struct iovec *iov, int iovcnt, char *line, int tmout);

typedef struct
{
  int ind;
  int max_size;
  char line[N_LINES][N_CHARS];
  int size[N_LINES];
  char buf[N_LINES * N_CHARS + 3];
} LINE;

static void
init_line (LINE * lp)
{
  memset (lp, 0, sizeof *lp);
}

static void
format_line (LINE * lp, const char *fmt, ...)
{
  va_list ap;
  int i = lp->ind;

  if (lp->ind >= N_LINES)
    return;
  lp->ind++;
  va_start (ap, fmt);
  lp->size[i] = vsnprintf (lp->line[i], sizeof lp->line[i], fmt, ap);
  lp->max_size = MAX (lp->max_size, lp->size[i]);
  va_end (ap);
}

static char *
finish_line (LINE * lp)
{
  int i;
  char *p;

  p = lp->buf;
  *p++ = '\a';
  *p++ = '\r';
  *p++ = '\n';
  for (i = 0; i < lp->ind; i++)
    {
      char *q;
      int j;

      for (q = lp->line[i]; *q; q++)
	*p++ = *q;
      for (j = lp->size[i]; j < lp->max_size + 2; j++)
	*p++ = ' ';
      *p++ = '\r';
      *p++ = '\n';
    }
  *p = 0;
  return lp->buf;
}

static int
print_mesg (char *tty, CTL_MSG * request, char *remote_machine)
{
  time_t t;
  LINE ln;
  char *buf;
  struct tm *tm;
  struct iovec iovec;
  char *cp;

  time (&t);
  tm = localtime (&t);
  init_line (&ln);
  format_line (&ln, "");
  format_line (&ln, "Message from Talk_Daemon@%s at %d:%02d ...",
	       hostname, tm->tm_hour, tm->tm_min);
  format_line (&ln, "talk: connection requested by %s@%s",
	       request->l_name, remote_machine);
  format_line (&ln, "talk: respond with:  talk %s@%s",
	       request->l_name, remote_machine);
  format_line (&ln, "");
  format_line (&ln, "");
  buf = finish_line (&ln);

  iovec.iov_base = buf;
  iovec.iov_len = strlen (buf);

  if ((cp = ttymsg (&iovec, 1, tty, RING_WAIT - 5)) != NULL)
    {
      syslog (LOG_ERR, "%s", cp);
      return FAILED;
    }
  return SUCCESS;
}

/* See if the user is accepting messages. If so, announce that
   a talk is requested. */
int
announce (CTL_MSG * request, char *remote_machine)
{
  char *ttypath;
  int len;
  struct stat st;
  int rc;

  len = sizeof (PATH_TTY_PFX) + strlen (request->r_tty) + 2;
  ttypath = malloc (len);
  if (!ttypath)
    {
      syslog (LOG_ERR, "Out of memory");
      exit (EXIT_FAILURE);
    }
  sprintf (ttypath, "%s/%s", PATH_TTY_PFX, request->r_tty);
  rc = stat (ttypath, &st);
  free (ttypath);
  if (rc < 0 || (st.st_mode & S_IWGRP) == 0)
    return PERMISSION_DENIED;
  return print_mesg (request->r_tty, request, remote_machine);
}
