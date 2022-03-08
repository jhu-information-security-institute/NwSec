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
#include <string.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <protocols/talkd.h>
#include <pwd.h>
#include <libinetutils.h>
#include <unistd.h>
#include "talk.h"

char *getlogin (void);
char *ttyname (int);
extern CTL_MSG msg;

/*
 * Determine the local and remote user, tty, and machines
 */
int
get_names (int argc, char *argv[])
{
  char *his_name, *my_name;
  char *my_machine_name, *his_machine_name;
  char *his_tty;
  register char *cp;

  if ((my_name = getlogin ()) == NULL)
    {
      struct passwd *pw;

      if ((pw = getpwuid (getuid ())) == NULL)
	{
	  printf ("You don't exist. Go away.\n");
	  exit (-1);
	}
      my_name = pw->pw_name;
    }

  my_machine_name = localhost ();
  if (!my_machine_name)
    {
      perror ("Cannot get local hostname");
      exit (-1);
    }

  /* check for, and strip out, the machine name of the target */
  for (cp = argv[0]; *cp && !strchr ("@:!.", *cp); cp++)
    ;
  if (*cp == '\0')
    {
      /* this is a local to local talk */
      his_name = argv[0];
      his_machine_name = my_machine_name;
    }
  else
    {
      if (*cp++ == '@')
	{
	  /* user@host */
	  his_name = argv[0];
	  his_machine_name = cp;
	}
      else
	{
	  /* host.user or host!user or host:user */
	  his_name = cp;
	  his_machine_name = argv[0];
	}
      *--cp = '\0';
    }
  if (argc > 1)
    his_tty = argv[1];		/* tty name is arg 2 */
  else
    his_tty = "";
  get_addrs (my_machine_name, his_machine_name);
  /*
   * Initialize the message template.
   */
  msg.vers = TALK_VERSION;
  msg.addr.sa_family = htons (AF_INET);
  msg.ctl_addr.sa_family = htons (AF_INET);
  msg.id_num = htonl (0);
  strncpy (msg.l_name, my_name, NAME_SIZE);
  msg.l_name[NAME_SIZE - 1] = '\0';
  strncpy (msg.r_name, his_name, NAME_SIZE);
  msg.r_name[NAME_SIZE - 1] = '\0';
  strncpy (msg.r_tty, his_tty, TTY_SIZE);
  msg.r_tty[TTY_SIZE - 1] = '\0';

  free (my_machine_name);

  return 0;
}
