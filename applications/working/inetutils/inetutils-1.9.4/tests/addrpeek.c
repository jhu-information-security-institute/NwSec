/* addrpeek - testing service for Inetd: remote address, environment vars
  Copyright (C) 2010, 2011, 2012, 2013, 2014, 2015 Free Software
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

/* Written by Mats Erik Andersson.  */

/* Addrpeek is a test client for examining Inetd, primarily TCP traffic.
 * The client executes all those tasks that were listed as server
 * arguments in the configuration file for Inetd:
 *
 *    addr : Reply with "Your address is $IP."
 *    env  : Reply with all known environment variables and their values.
 *
 * Reasonable entries in `inetf.conf' could be
 *
 *    # Return numerical address of the calling client.
 *    #
 *    7890 stream tcp nowait nobody /tmp/addrpeek addrpeek addr
 *    7890 stream tcp6 nowait nobody /tmp/addrpeek addrpeek addr
 *    #
 *    # Display all environment variables in use, and append
 *    # the client's address at the end.
 *    #
 *    tcpmux stream tcp nowait nobody internal
 *    tcpmux stream tcp6 nowait nobody internal
 *    tcpmux/env stream tcp nowait nobody /tmp/addrpeek addrkeep env addr
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

#ifndef SEPARATOR
# define SEPARATOR "\n"
#endif

/* TODO Develop some reliable test for the existence of ENVIRON.
 * It is detectable using HAVE_DECL_ENVIRON for GNU/Linux and
 * GNU/kFreeBSD. It is present, but not detectable for OpenBSD
 * and FreeBSD.
 */
extern char **environ;

static void
write_address (int fd)
{
  int type;
  size_t len;
  socklen_t sslen;
  char addr[INET6_ADDRSTRLEN], answer[128];
  struct sockaddr_storage ss;

  sslen = sizeof (type);
  getsockopt (fd, SOL_SOCKET, SO_TYPE, &type, &sslen);

  if (type == SOCK_STREAM)
    {
      sslen = sizeof (ss);
      getpeername (fd, (struct sockaddr *) &ss, &sslen);
    }
  else if (type == SOCK_DGRAM)
    {
      sslen = sizeof (ss);
      recvfrom (fd, answer, sizeof (answer), 0,
                  (struct sockaddr *) &ss, &sslen);
      shutdown (fd, SHUT_RD);
    }
  else
      return;

  getnameinfo ((struct sockaddr *) &ss, sslen, addr, sizeof (addr),
                NULL, 0, NI_NUMERICHOST);

  len = snprintf (answer, sizeof (answer),
                  "Your address is %s." SEPARATOR, addr);

  sendto (fd, answer, len, 0, (struct sockaddr *) &ss, sslen);
}

void
write_environment (int fd, char *envp[])
{
  for ( ; *envp; ++envp)
    {
      write (fd, *envp, strlen (*envp));
      write (fd, SEPARATOR, strlen (SEPARATOR));
    }
}

int
main (int argc, char *argv[])
{
  int j;
  set_program_name (argv[0]);
  for (j = 1; j < argc; ++j)
    {
      if (strncmp (argv[j], "addr", strlen ("addr")) == 0)
        {
          write_address (STDOUT_FILENO);
          continue;
        }

      if (strncmp (argv[j], "env", strlen ("env")) == 0)
        {
          write_environment (STDOUT_FILENO, environ);
          continue;
        }
    }

  close (STDIN_FILENO);
  close (STDOUT_FILENO);
  close (STDERR_FILENO);

  return EXIT_SUCCESS;
}
