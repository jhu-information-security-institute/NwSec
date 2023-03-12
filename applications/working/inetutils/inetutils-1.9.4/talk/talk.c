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

#include <config.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef HAVE_LOCALE_H
# include <locale.h>
#endif

#include "talk.h"
#include <argp.h>
#include <libinetutils.h>

void usage (void);

/*
 * talk:	A visual form of write. Using sockets, a two way
 *		connection is set up between the two people talking.
 *		With the aid of curses, the screen is split into two
 *		windows, and each users text is added to the window,
 *		one character at a time...
 *
 *		Written by Kipp Hickman
 *
 *		Modified to run under 4.1a by Clem Cole and Peter Moore
 *		Modified to run between hosts by Peter Moore, 8/19/82
 *		Modified to run under 4.1c by Peter Moore 3/17/83
 */

const char *program_authors[] =
  {
    "Kipp Hickman",
    "Clem Cole",
    "Peter Moore",
    NULL
  };

const char doc[] = "Talk to another user.";
const char args_doc[] = "person [ttyname]";
static struct argp argp = { NULL, NULL, args_doc, doc, NULL, NULL, NULL};

int
main (int argc, char *argv[])
{
  int index;

  set_program_name (argv[0]);
#ifdef HAVE_SETLOCALE
  setlocale (LC_ALL, "");
#endif
  iu_argp_init ("talk", program_authors);
  argp_parse (&argp, argc, argv, 0, &index, NULL);

  argc -= index;
  argv += index;

  if (argc == 0)
    {
      printf ("Usage: talk user [ttyname]\n");
      exit (-1);
    }
  if (!isatty (0))
    {
      printf ("Standard input must be a tty, not a pipe, nor a file.\n");
      exit (-1);
    }

  get_names (argc, argv);

  /* Let them know we are working on connections.  */
  current_state = "No connection yet.";

  open_ctl ();
  open_sockt ();
  current_state = "Service connection established.";

  start_msgs ();
  if (!check_local ())
    invite_remote ();
  end_msgs ();

  /* Our party is responding.  Upgrade user interface.  */
  init_display ();
  set_edit_chars ();

  talk ();
}


static const char usage_str[] =
  "Usage: talk [OPTIONS...] USER\n"
  "\n"
  "Options are:\n"
  "       --help              Display usage instructions\n"
  "       --version           Display program version\n";

void
usage (void)
{
  printf ("%s\n" "Send bug reports to <%s>.\n", usage_str, PACKAGE_BUGREPORT);
}
