/* readutmp - Basic test of existing utmp/utmpx access.
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

/* Readutmp reads the system's standard UTMP or UTMPX file
 * and verifies that a spcific user is logged in.
 *
 * Invocation:
 *
 *    readutmp
 *    readutmp name
 *
 * The first mode investigates the present user.
 */

#include <config.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#include <pwd.h>

#include <progname.h>
#include <readutmp.h>
#include <xalloc.h>

int
main (int argc, char *argv[])
{
#ifndef HAVE_GETUTXUSER
  STRUCT_UTMP *utmpp, *uptr;
  size_t count;
#endif
  struct passwd *pw;
  char *name;
  int found = 0;

  set_program_name (argv[0]);

  if (argc > 1)
    pw = getpwnam (argv[1]);
  else
    pw = getpwuid (getuid ());

  if (pw)
    name = xstrdup (pw->pw_name);
  else
    {
      fprintf (stderr, "Unknown user '%s'.\n",
	       (argc > 1) ? argv[1] : "my own UID");
      return EXIT_FAILURE;
    }

#ifdef HAVE_GETUTXUSER
  setutxent ();
  found = (getutxuser (name) != 0);
  endutxent ();
#else /* !HAVE_GETUTXUSER */
  if (read_utmp (UTMP_FILE, &count, &utmpp, READ_UTMP_USER_PROCESS))
    {
      perror ("read_utmp");
      return EXIT_FAILURE;
    }

  for (uptr = utmpp; uptr < utmpp + count; uptr++)
    if (!strncmp (name, UT_USER (uptr), sizeof (UT_USER (uptr))))
      {
	found = 1;
	break;
      }

  free (utmpp);
#endif /* HAVE_GETUTXUSER */

  if (found)
    return EXIT_SUCCESS;

  fprintf (stderr, "User '%s' is not logged in.\n", name);
  return EXIT_FAILURE;
}
