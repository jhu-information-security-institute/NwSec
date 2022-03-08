/*
  Copyright (C) 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003,
  2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014,
  2015 Free Software Foundation, Inc.

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
 * Copyright (c) 1983, 1993
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

/*
 * This file contains the I/O handling and the exchange of
 * edit characters. This connection itself is established in
 * ctl.c
 */

#include <config.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#ifdef HAVE_SYS_FILIO_H
# include <sys/filio.h>
#endif
#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/select.h>
#include "talk.h"

#define A_LONG_TIME 10000000

/*
 * The routine to do the actual talking
 */
int
talk (void)
{
  fd_set read_template, read_set;
  int stdin_fd = fileno (stdin);
  int i, nb, num_fds;
  char buf[BUFSIZ];
  struct timeval wait;

  message ("Connection established");
  beep ();
  current_line = 0;

  /*
   * Wait on both the other process (SOCKET) and stdin.
   */
  FD_ZERO (&read_template);
  FD_SET (sockt, &read_template);
  FD_SET (stdin_fd, &read_template);
  num_fds = (stdin_fd > sockt ? stdin_fd : sockt) + 1;

  for (;;)
    {
      read_set = read_template;
      wait.tv_sec = A_LONG_TIME;
      wait.tv_usec = 0;
      nb = select (num_fds, &read_set, 0, 0, &wait);
      if (nb <= 0)
	{
	  if (errno == EINTR)
	    {
	      read_set = read_template;
	      continue;
	    }
	  /* panic, we don't know what happened */
	  p_error ("Unexpected error from select");
	  quit ();
	}
      if (FD_ISSET (sockt, &read_set))
	{
	  /* There is data on sockt */
	  nb = read (sockt, buf, sizeof buf);
	  if (nb <= 0)
	    {
	      message ("Connection closed. Exiting");
	      quit ();
	    }
	  display (&his_win, buf, nb);
	}
      if (FD_ISSET (stdin_fd, &read_set))
	{
	  /*
	   * We can't make the tty non_blocking, because
	   * curses's output routines would screw up
	   */
	  ioctl (0, FIONREAD, (struct sgttyb *) &nb);
	  for (i = 0; i < nb; i++)
	    buf[i] = getch ();
	  display (&my_win, buf, nb);
	  /* might lose data here because sockt is non-blocking */
	  write (sockt, buf, nb);
	}
    }
}

/*
 * p_error prints the system error message on the standard location
 * on the screen and then exits, i.e., a curses version of perror.
 */
int
p_error (char *string)
{
  if (curses_initialized)
    {
      wmove (my_win.x_win, current_line % my_win.x_nlines, 0);
      wprintw (my_win.x_win, "[%s : %s (%d)]\n",
	       string, strerror (errno), errno);
      wrefresh (my_win.x_win);
      move (LINES - 1, 0);
      refresh ();
    }
  else
    perror (string);

  quit ();

  return 0;
}

/*
 * Display string in the standard location.
 */
int
message (char *string)
{
  if (curses_initialized)
    {
      wmove (my_win.x_win, current_line % my_win.x_nlines, 0);
      wprintw (my_win.x_win, "[%s]", string);
      wclrtoeol (my_win.x_win);
      current_line++;
      wmove (my_win.x_win, current_line % my_win.x_nlines, 0);
      wrefresh (my_win.x_win);
    }
  else
    if (string && string[0])
      printf ("[%s]\n", string);

  return 0;
}
