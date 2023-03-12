/*
  Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
  2011, 2012, 2013, 2014, 2015 Free Software Foundation, Inc.

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

#include "telnetd.h"
#include <sys/wait.h>
#include <pty.h>

#ifdef AUTHENTICATION
# include <libtelnet/auth.h>
#endif
#include <libinetutils.h>

void
setup_utmp (char *line, char *host)
{
  char *ut_id = utmp_ptsid (line, "tn");
  utmp_init (line + sizeof (PATH_TTY_PFX) - 1, ".telnet", ut_id, host);
}


int
startslave (char *host, int autologin, char *autoname)
{
  pid_t pid;
  int master;

#ifdef AUTHENTICATION
  if (!autoname || !autoname[0])
    autologin = 0;

  if (autologin < auth_level)
    {
      fatal (net, "Authorization failed");
      exit (EXIT_FAILURE);
    }
#else /* !AUTHENTICATION */
  (void) autoname;	/* Silence warnings.  */
#endif

  pid = forkpty (&master, line, NULL, NULL);
  if (pid < 0)
    {
      if (errno == ENOENT)
	{
	  syslog (LOG_ERR, "Out of ptys");
	  fatal (net, "Out of ptys");
	}
      else
	{
	  syslog (LOG_ERR, "forkpty: %m");
	  fatal (net, "Forkpty");
	}
    }

  if (pid == 0)
    {
      /* Child */
      if (net > 2)
	close (net);

      setup_utmp (line, host);
      start_login (host, autologin, line);
    }

  /* Master */
  return master;
}

extern char **environ;
/*
 * scrub_env()
 *
 * Remove a few things from the environment that
 * don't need to be there.
 *
 * Security fix included in telnet-95.10.23.NE of David Borman <deb@cray.com>.
 */
static void
scrub_env (void)
{
  register char **cpp, **cpp2;

  for (cpp2 = cpp = environ; *cpp; cpp++)
    {
      if (strncmp (*cpp, "LD_", 3)
	  && strncmp (*cpp, "_RLD_", 5)
	  && strncmp (*cpp, "LIBPATH=", 8) && strncmp (*cpp, "IFS=", 4))
	*cpp2++ = *cpp;
    }
  *cpp2 = 0;
}

void
start_login (char *host, int autologin, char *name)
{
  char *cmd;
  int argc;
  char **argv;

  (void) host;		/* Silence warnings.  Diagnostic use?  */
  (void) autologin;
  (void) name;

  scrub_env ();

  /* Set the environment variable "LINEMODE" to indicate our linemode */
  if (lmodetype == REAL_LINEMODE)
    setenv ("LINEMODE", "real", 1);
  else if (lmodetype == KLUDGE_LINEMODE || lmodetype == KLUDGE_OK)
    setenv ("LINEMODE", "kludge", 1);

  cmd = expand_line (login_invocation);
  if (!cmd)
    fatal (net, "can't expand login command line");
  argcv_get (cmd, "", &argc, &argv);
  execv (argv[0], argv);
  syslog (LOG_ERR, "%s: %m\n", cmd);
  fatalperror (net, cmd);
}

/* SIG is generally naught every time the server itself
 * decides to close the connection out of an error condition.
 * In response to TELOPT_LOGOUT from the client, SIG is set
 * to SIGHUP, so we consider the exit as a success.  In other
 * cases, when the forked client process is caught exiting,
 * then SIG will be SIGCHLD.  Then we deliver the clients's
 * reported exit code.
 */
void
cleanup (int sig)
{
  int status = EXIT_FAILURE;
  char *p;

  if (sig == SIGCHLD)
    {
      pid_t pid = waitpid ((pid_t) - 1, &status, WNOHANG);
      syslog (LOG_INFO, "child process %ld exited: %d",
	      (long) pid, WEXITSTATUS (status));

      status = WEXITSTATUS (status);
    }
  else if (sig == SIGHUP)
    status = EXIT_SUCCESS;	/* Response to TELOPT_LOGOUT.  */

  p = line + sizeof (PATH_TTY_PFX) - 1;
  utmp_logout (p);
  chmod (line, 0644);
  chown (line, 0, 0);
  shutdown (net, 2);
  exit (status);
}
