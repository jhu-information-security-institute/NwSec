/*
  Copyright (C) 2012, 2013, 2014, 2015 Free Software Foundation, Inc.

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

#include <argp.h>
#include <error.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <netdb.h>

#include "libinetutils.h"
#include "progname.h"
#include "xgethostname.h"

const char doc[] =
  "Show domain part of the system's fully qualified host name.\n\
\n\
The tool uses gethostname to get the host name of the system\n\
and getaddrinfo to resolve it into a canonical name.  The part\n\
after the first period ('.') of the canonical name is shown.";
const char *program_authors[] = {
  "Simon Josefsson",
  NULL
};

static struct argp argp = { NULL, NULL, NULL, doc, NULL, NULL, NULL };

void
dnsdomainname (void)
{
  char *host_name;
  struct addrinfo hints, *res;
  const char *dn;
  int rc;

  host_name = xgethostname ();
  if (!host_name)
    error (EXIT_FAILURE, errno, "cannot determine host name");

  memset (&hints, 0, sizeof (struct addrinfo));
  hints.ai_flags = AI_CANONNAME;

  rc = getaddrinfo (host_name, NULL, &hints, &res);
  if (rc != 0)
    error (EXIT_FAILURE, 0, "%s", gai_strerror (rc));

  dn = strchr (res->ai_canonname, '.');
  if (dn)
    puts (dn + 1);

  free (host_name);
  freeaddrinfo (res);
}

int
main (int argc, char *argv[])
{
  set_program_name (argv[0]);

  iu_argp_init ("dnsdomainname", program_authors);
  argp_parse (&argp, argc, argv, 0, NULL, NULL);

  dnsdomainname ();

  exit (EXIT_SUCCESS);
}
