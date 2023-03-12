/* tcpget - get single response from a TCP port.
  Copyright (C) 2013, 2014, 2015 Free Software Foundation, Inc.

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

/* Written by Mats Erik Andersson.  */

/* Tcpget receives and displays whatever text a server cares to send
 * without any interaction from a client and within a set time period.
 * An alarm timer is set to five seconds by default, but the limit
 * can be set to another value with a command line switch, starting
 * at one second, but limited upwards to one hour!
 *
 * Invocation:
 *
 *   tcpget [-t secs] host tcp-port
 */

#include <config.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <progname.h>
#ifdef HAVE_LOCALE_H
# include <locale.h>
#endif

int
main (int argc, char *argv[])
{
  int fd, opt, rc;
  int timeout = 5;	/* Defaulting to five seconds of waiting time.  */
  char buffer[256];
  struct addrinfo hints, *ai, *res;

  set_program_name (argv[0]);

#ifdef HAVE_SETLOCALE
  setlocale (LC_ALL, "");
#endif

  while ((opt = getopt (argc, argv, "t:")) != -1)
    {
      int t;

      switch (opt)
	{
	case 't':
	  t = atoi (optarg);
	  if (t > 0 && t <= 3600 /* on hour */)
	    timeout = t;
	  break;

	default:
	  fprintf (stderr, "Usage: %s [-t secs] host port\n", argv[0]);
	  exit (EXIT_FAILURE);
	}
    }

  if (argc < optind + 2)
    return (EXIT_FAILURE);

  memset (&hints, 0, sizeof (hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  rc = getaddrinfo (argv[optind], argv[optind + 1], &hints, &res);

  if (rc)
    {
      fprintf (stderr, "%s: %s\n", argv[0], gai_strerror (rc));
      return (EXIT_FAILURE);
    }

  for (ai = res; ai; ai = ai->ai_next)
    {
      fd = socket (ai->ai_family, ai->ai_socktype, ai->ai_protocol);
      if (fd < 0)
	continue;

      if (connect (fd, ai->ai_addr, ai->ai_addrlen) >= 0)
	break;

      close (fd);
    }

  freeaddrinfo (res);

  if (ai && (fd >= 0))
    {
      ssize_t n;

      alarm (timeout);

      while ((n = recv (fd, buffer, sizeof (buffer), 0)))
	write (STDOUT_FILENO, buffer, n);

      close (fd);
    }

  close (STDIN_FILENO);
  close (STDOUT_FILENO);
  close (STDERR_FILENO);

  return EXIT_SUCCESS;
}
