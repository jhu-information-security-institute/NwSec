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
 * The window 'manager', initializes curses and handles the actual
 * displaying of text
 */

#include <config.h>

#include <ctype.h>
#include "talk.h"

xwin_t my_win;
xwin_t his_win;
WINDOW *line_win;

int curses_initialized = 0;

#undef max
/*
 * max HAS to be a function, it is called with
 * a argument of the form --foo at least once.
 */
int
max (int a, int b)
{
  return a > b ? a : b;
}

/*
 * Read the character at the indicated position in win
 */
static int
readwin (WINDOW * win, int line, int col)
{
  int oldline, oldcol;
  register int c;

  getyx (win, oldline, oldcol);
  wmove (win, line, col);
  c = winch (win);
  wmove (win, oldline, oldcol);
  return (c);
}

/*
 * Scroll a window, blanking out the line following the current line
 * so that the current position is obvious
 */
static void
xscroll (register xwin_t * win, int flag)
{
  if (flag == -1)
    {
      wmove (win->x_win, 0, 0);
      win->x_line = 0;
      win->x_col = 0;
      return;
    }
  win->x_line = (win->x_line + 1) % win->x_nlines;
  win->x_col = 0;
  wmove (win->x_win, win->x_line, win->x_col);
  wclrtoeol (win->x_win);
  wmove (win->x_win, (win->x_line + 1) % win->x_nlines, win->x_col);
  wclrtoeol (win->x_win);
  wmove (win->x_win, win->x_line, win->x_col);
}

/*
 * Display some text on somebody's window, processing some control
 * characters while we are at it.
 */
int
display (register xwin_t * win, register char *text, int size)
{
  register int i;
  unsigned char cch;

  for (i = 0; i < size; i++)
    {
      if (*text == '\n')
	{
	  xscroll (win, 0);
	  text++;
	  continue;
	}
      if (*text == '\a')
	{
	  beep ();
	  wrefresh (curscr);
	  text++;
	  continue;
	}
      /* erase character */
      if (*text == win->cerase)
	{
	  wmove (win->x_win, win->x_line, max (--win->x_col, 0));
	  getyx (win->x_win, win->x_line, win->x_col);
	  waddch (win->x_win, ' ');
	  wmove (win->x_win, win->x_line, win->x_col);
	  getyx (win->x_win, win->x_line, win->x_col);
	  text++;
	  continue;
	}
      /*
       * On word erase search backwards until we find
       * the beginning of a word or the beginning of
       * the line.
       */
      if (*text == win->werase)
	{
	  int endcol, xcol, i, c;

	  endcol = win->x_col;
	  xcol = endcol - 1;
	  while (xcol >= 0)
	    {
	      c = readwin (win->x_win, win->x_line, xcol);
	      if (c != ' ')
		break;
	      xcol--;
	    }
	  while (xcol >= 0)
	    {
	      c = readwin (win->x_win, win->x_line, xcol);
	      if (c == ' ')
		break;
	      xcol--;
	    }
	  wmove (win->x_win, win->x_line, xcol + 1);
	  for (i = xcol + 1; i < endcol; i++)
	    waddch (win->x_win, ' ');
	  wmove (win->x_win, win->x_line, xcol + 1);
	  getyx (win->x_win, win->x_line, win->x_col);
	  text++;
	  continue;
	}
      /* line kill */
      if (*text == win->kill)
	{
	  wmove (win->x_win, win->x_line, 0);
	  wclrtoeol (win->x_win);
	  getyx (win->x_win, win->x_line, win->x_col);
	  text++;
	  continue;
	}
      /* Refresh screen with input ^L, Ctrl-L.
       * Local trigger only.
       */
      if (*text == '\f')
	{
	  if (win == &my_win)
	    wrefresh (curscr);
	  text++;
	  continue;
	}
      /* Clear both windows with input ^D, Ctrl-D.
       * Local trigger only.
       */
      if (*text == '\04' && win == &my_win)
	{
	  wclear (my_win.x_win);
	  wclear (his_win.x_win);
	  wrefresh (my_win.x_win);
	  wrefresh (his_win.x_win);
	  text++;
	  continue;
	}
      if (win->x_col == COLS - 1)
	{
	  /* check for wraparound */
	  xscroll (win, 0);
	}

      /*
       * Printable characters, SP, and TAB are printed
       * verbatim.  Characters beyond the ASCII table
       * must be handled.  Beware of sign extension!
       *
       * The locale setting is in effect when testing
       * printability of any input character.
       */
      if (isprint (*text & 0xff) || *text == '\t')
	waddch (win->x_win, *text & 0xff);
      else
	{
	  waddch (win->x_win, '^');
	  getyx (win->x_win, win->x_line, win->x_col);
	  if (win->x_col == COLS - 1)	/* check for wraparound */
	    xscroll (win, 0);
	  cch = (*text & 63) + 64;
	  waddch (win->x_win, cch);
	}

      getyx (win->x_win, win->x_line, win->x_col);
      text++;
    }
  wrefresh (win->x_win);

  return 0;
}
