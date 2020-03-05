/* waitdaemon -- Verify that waitdaemon() is functional.
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

/* Verify that waitdaemon() in libinetutils.a is functional.
   This is needed by syslogd.
 */

#include <config.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#include <signal.h>
#include <errno.h>

#include <error.h>
#include <progname.h>
#include <libinetutils.h>
#include <unused-parameter.h>

extern int waitdaemon (int nochdir, int noclose, int maxwait);

void
doexit (int signo _GL_UNUSED_PARAMETER)
{
  _exit (EXIT_SUCCESS);
}

int
main (int argc _GL_UNUSED_PARAMETER, char *argv[])
{
  pid_t ppid;

  set_program_name (argv[0]);

  signal (SIGTERM, doexit);	/* Parent exits nicely at SIGTERM.  */

  ppid = waitdaemon (0, 0, 5);	/* Five seconds of delay.  */

  /* Child should return here before parent.  */
  if (ppid > 0)
    kill (ppid, SIGTERM);	/* Bless the parent process.  */

  /* Only grand child reaches this statement.  */
  return EXIT_FAILURE;
}
