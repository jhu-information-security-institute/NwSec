/* identify -- Probe system and report characteristica.
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

/* Collect information about this system and report it.
 * Some identified characteristica of relevance to GNU Inetutils
 * are displayed for use in bug reporting and resolution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/param.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#define tell_macro(a,b)	\
  printf ("%s: %s\n", (b) ? "Available macro" : "Not defined", (a));

int
main (void)
{
  int a, ux;
  struct utsname uts;
  struct sockaddr_un su;

  if (uname (&uts) < 0)
    {
      fprintf (stderr, "Not able to identify running system.\n");
      exit (EXIT_FAILURE);
    }

  /* Identify the hardware.  */
  printf ("Running system: %s, %s\n",
	  uts.sysname, uts.machine);
  printf (" Variant: %s\n", uts.release);
  printf (" Variant: %s\n", uts.version);
  puts ("");

  /*
   * Report on macros that determine alternate code.
   * These depend on toolchains and hardware.
   */
  ux = 0;
#ifdef unix
  ux = 1;
#endif
  tell_macro ("unix", ux);

  ux = 0;
#ifdef __unix
  ux = 1;
#endif
  tell_macro ("__unix", ux);

  ux = 0;
#ifdef __unix__
  ux = 1;
#endif
  tell_macro ("__unix__", ux);

  ux = 0;
#ifdef __sun
  ux = 1;
#endif
  tell_macro ("__sun", ux);

  ux = 0;
#ifdef __sun__
  ux = 1;
#endif
  tell_macro ("__sun__", ux);

  a = 0;
#ifdef TN3270
  a = 1;
#endif
  tell_macro ("TN3270", a);

  /*
   * Implementation specific charateristica.
   */
  puts ("");

  a = 0;
#ifdef BSD
  a = 1;
#endif
  tell_macro ("BSD", a);

  printf ("Size of 'struct sockaddr_un.sun_path': %zu\n",
	  sizeof (su.sun_path));
  return 0;
}
