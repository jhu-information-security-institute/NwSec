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
#include <sys/socket.h>
#include <netinet/in.h>
#include <protocols/talkd.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#ifdef HAVE_IDNA_H
# include <idna.h>
#endif

#include "talk_ctl.h"

int
get_addrs (char *my_machine_name, char *his_machine_name)
{
#if HAVE_DECL_GETADDRINFO || defined HAVE_IDN
  int err;
#endif
  char *lhost, *rhost;
#if HAVE_DECL_GETADDRINFO
  struct addrinfo hints, *res, *ai;
#else /* !HAVE_DECL_GETADDRINFO */
  struct hostent *hp;
#endif
  struct servent *sp;

#ifdef HAVE_IDN
  err = idna_to_ascii_lz (my_machine_name, &lhost, 0);
  if (err)
    {
      fprintf (stderr, "talk: %s: %s\n",
	       my_machine_name, idna_strerror (err));
      exit (-1);
    }

  err = idna_to_ascii_lz (his_machine_name, &rhost, 0);
  if (err)
    {
      fprintf (stderr, "talk: %s: %s\n",
	       his_machine_name, idna_strerror (err));
      exit (-1);
    }
#else /* !HAVE_IDN */
  lhost = my_machine_name;
  rhost = his_machine_name;
#endif

  msg.pid = htonl (getpid ());

  /* Look up the address of the local host.  */

#if HAVE_DECL_GETADDRINFO
  memset (&hints, 0, sizeof (hints));

  /* The talk-protocol is IPv4 only!  */
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
# ifdef AI_IDN
  hints.ai_flags |= AI_IDN;
# endif

  err = getaddrinfo (lhost, NULL, &hints, &res);
  if (err)
    {
      fprintf (stderr, "talk: %s: %s\n", lhost, gai_strerror (err));
      exit (-1);
    }

  /* Perform all sanity checks available.
   * Reduction of tests?
   */
  for (ai = res; ai; ai = ai->ai_next)
    {
      int f;

      if (ai->ai_family != AF_INET)
	continue;

      f = socket (ai->ai_family, ai->ai_socktype, ai->ai_protocol);
      if (f < 0)
	continue;

      /* Attempt binding to this local address.  */
      if (bind (f, ai->ai_addr, ai->ai_addrlen))
        {
	  close (f);
	  f = -1;
	  continue;
	}

      /* We have a usable address.  */
      close (f);
      break;
    }

  if (ai)
    memcpy (&my_machine_addr,
	    &((struct sockaddr_in *) ai->ai_addr)->sin_addr,
	    sizeof (my_machine_addr));

  freeaddrinfo (res);
  if (!ai)
    {
      fprintf (stderr, "talk: %s: %s\n", lhost, "address not found");
      exit (-1);
    }

#else /* !HAVE_DECL_GETADDRINFO */
  hp = gethostbyname (lhost);
  if (hp == NULL)
    {
      fprintf (stderr, "talk: %s(%s): ", lhost, my_machine_name);
      herror ((char *) NULL);
      exit (-1);
    }
  memmove (&my_machine_addr, hp->h_addr, hp->h_length);
#endif /* !HAVE_DECL_GETADDRINFO */

  /*
   * If the callee is on-machine, just copy the
   * network address, otherwise do a lookup...
   */
  if (strcmp (rhost, lhost))
    {
#if HAVE_DECL_GETADDRINFO
      err = getaddrinfo (rhost, NULL, &hints, &res);
      if (err)
	{
	  fprintf (stderr, "talk: %s: %s\n", rhost, gai_strerror (err));
	  exit (-1);
	}

      /* Perform all sanity checks available.  */
      for (ai = res; ai; ai = ai->ai_next)
	{
	  int f;

	  if (ai->ai_family != AF_INET)
	    continue;

	  f = socket (ai->ai_family, ai->ai_socktype, ai->ai_protocol);
	  if (f < 0)
	    continue;

	  /* We have a usable address family!  */
	  close (f);
	  break;
	}

      if (ai)
	memcpy (&his_machine_addr,
		&((struct sockaddr_in *) ai->ai_addr)->sin_addr,
		sizeof (his_machine_addr));

      freeaddrinfo (res);
      if (!ai)
	{
	  fprintf (stderr, "talk: %s: %s\n", rhost, "address not found");
	  exit (-1);
	}

#else /* !HAVE_DECL_GETADDRINFO */
      hp = gethostbyname (rhost);
      if (hp == NULL)
	{
	  fprintf (stderr, "talk: %s(%s): ", rhost, his_machine_name);
	  herror ((char *) NULL);
	  exit (-1);
	}
      memmove (&his_machine_addr, hp->h_addr, hp->h_length);
#endif /* !HAVE_DECL_GETADDRINFO */
    }
  else
    his_machine_addr = my_machine_addr;

  /* Find the server's port.  */
  sp = getservbyname ("ntalk", "udp");
  if (sp == 0)
    {
      fprintf (stderr, "talk: %s/%s: service is not registered.\n",
	       "ntalk", "udp");
      exit (-1);
    }
  daemon_port = ntohs (sp->s_port);

#ifdef HAVE_IDN
  free (lhost);
  free (rhost);
#endif

  return 0;
}
