/* ifconfig.c -- network interface configuration utility
  Copyright (C) 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009,
  2010, 2011, 2012, 2013, 2014, 2015 Free Software Foundation, Inc.

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

/* Written by Marcus Brinkmann.  */

#include <config.h>

#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>

#include <unistd.h>

#include <string.h>

#if STDC_HEADERS
# include <stdlib.h>
#else
# ifndef HAVE_STRCHR
#  define strchr index
#  define strrchr rindex
# endif
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <progname.h>
#include "ifconfig.h"

int
main (int argc, char *argv[])
{
  int err = 0;
  int sfd;
  struct ifconfig *ifp;
  set_program_name (argv[0]);
  parse_cmdline (argc, argv);

  sfd = socket (AF_INET, SOCK_STREAM, 0);
  if (sfd < 0)
    {
      error (0, errno, "socket error");
      exit (EXIT_FAILURE);
    }

  for (ifp = ifs; ifp < ifs + nifs; ifp++)
    {
      if (list_mode)
	{
	  /* Protect against mistakes in use of `-i'.  */
	  if (!if_nametoindex (ifp->name))
	    continue;

	  /* Use ERR as a marker of previous output.
	   * It will never interfere with a call to configure_if().  */
	  if (err++)
	    putchar (' ');
	  printf ("%s", ifp->name);
	  continue;
	}

      err = configure_if (sfd, ifp);
      if (err)
	break;
    }

  if (list_mode && err)
    putchar ('\n');

  close (sfd);
  return err;
}
