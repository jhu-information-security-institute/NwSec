/*
  Copyright (C) 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002,
  2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013,
  2014, 2015 Free Software Foundation, Inc.

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
 * Copyright (c) 1988, 1993, 1994
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

/* This code is derived from software written by Ken Arnold and
   published in UNIX Review, Vol. 6, No. 8.  */

#include <config.h>

#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
/* Include glob.h last, because it may define "const" which breaks
   system headers on some platforms. */
#include <glob.h>

#include "extern.h"

/*
 * Special version of popen which avoids call to shell.  This ensures noone
 * may create a pipe to a hidden program as a side effect of a list or dir
 * command.
 */

#define MAX_ARGC 100
#define MAX_GARGC 1000

struct file_pid
{
  FILE *file;
  pid_t pid;
  struct file_pid *next;
};

/* A linked list associating ftpd_popen'd FILEs with pids.  */
struct file_pid *file_pids = 0;

#ifdef WITH_LIBLS
extern int ls_main (int argc, char *argv[]);
#endif

FILE *
ftpd_popen (char *program, const char *type)
{
  char *cp;
  FILE *iop;
  struct file_pid *fpid;
  int argc, gargc, pdes[2], pid;
  char **pop, *argv[MAX_ARGC], *gargv[MAX_GARGC];

  if (((*type != 'r') && (*type != 'w')) || type[1])
    return (NULL);

  if (pipe (pdes) < 0)
    return (NULL);

  /* break up string into pieces */
  for (argc = 0, cp = program; argc < MAX_ARGC - 1; cp = NULL, argc++)
    if (!(argv[argc] = strtok (cp, " \t\n")))
      break;
  argv[MAX_ARGC - 1] = NULL;

  /* glob each piece */
  gargv[0] = argv[0];
  for (gargc = argc = 1; argv[argc]; argc++)
    {
      glob_t gl;
      int flags = GLOB_NOCHECK;

#ifdef GLOB_BRACE
      flags |= GLOB_BRACE;
#endif
#ifdef GLOB_QUOTE
      flags |= GLOB_QUOTE;
#endif
#ifdef GLOB_TILDE
      flags |= GLOB_TILDE;
#endif

      memset (&gl, 0, sizeof (gl));
      if (glob (argv[argc], flags, NULL, &gl))
	gargv[gargc++] = strdup (argv[argc]);
      else if (gl.gl_pathc > 0)
	for (pop = gl.gl_pathv; *pop && (gargc < MAX_GARGC - 1); pop++)
	  gargv[gargc++] = strdup (*pop);
      globfree (&gl);
    }
  gargv[gargc] = NULL;

  iop = NULL;

#ifdef WITH_LIBLS
  /* Do not use vfork() for internal ls.  */
  pid = (strcmp (gargv[0], "/bin/ls") == 0) ? fork () : vfork ();
  switch (pid)
#else
  switch (pid = vfork ())
#endif
    {
    case -1:			/* error */
      close (pdes[0]);
      close (pdes[1]);
      goto pfree;
    case 0:			/* child */
      if (*type == 'r')
	{
	  if (pdes[1] != STDOUT_FILENO)
	    {
	      dup2 (pdes[1], STDOUT_FILENO);
	      close (pdes[1]);
	    }
	  dup2 (STDOUT_FILENO, STDERR_FILENO);	/* stderr too! */
	  close (pdes[0]);
	}
      else
	{
	  if (pdes[0] != STDIN_FILENO)
	    {
	      dup2 (pdes[0], STDIN_FILENO);
	      close (pdes[0]);
	    }
	  close (pdes[1]);
	}

#ifdef WITH_LIBLS
      /* mvo: should this be a config-option? */
      if (strcmp (gargv[0], "/bin/ls") == 0)
	{
	  optind = 0;
	  exit (ls_main (gargc, gargv));
	}
#endif

      execv (gargv[0], gargv);
      _exit (EXIT_FAILURE);
    }
  /* parent; assume fdopen can't fail...  */
  if (*type == 'r')
    {
      iop = fdopen (pdes[0], type);
      close (pdes[1]);
    }
  else
    {
      iop = fdopen (pdes[1], type);
      close (pdes[0]);
    }

  fpid = (struct file_pid *) malloc (sizeof (struct file_pid));
  if (fpid)
    {
      fpid->file = iop;
      fpid->pid = pid;
      fpid->next = file_pids;
      file_pids = fpid;
    }

pfree:for (argc = 1; gargv[argc] != NULL; argc++)
    free (gargv[argc]);

  return (iop);
}

int
ftpd_pclose (FILE * iop)
{
  struct file_pid *fpid = file_pids, *prev_fpid = 0;
  int status;
#ifdef HAVE_SIGACTION
  sigset_t sigs, osigs;
#else
  int omask;
#endif
  pid_t pid;

  /*
   * pclose returns -1 if stream is not associated with a
   * `popened' command, or, if already `pclosed'.
   */
  while (fpid && fpid->file != iop)
    {
      prev_fpid = fpid;
      fpid = fpid->next;
    }
  if (!fpid)
    return -1;

  if (prev_fpid)
    prev_fpid->next = fpid->next;
  else
    file_pids = fpid->next;

  fclose (iop);
#ifdef HAVE_SIGACTION
  sigemptyset (&sigs);
  sigaddset (&sigs, SIGINT);
  sigaddset (&sigs, SIGQUIT);
  sigaddset (&sigs, SIGHUP);
  sigprocmask (SIG_BLOCK, &sigs, &osigs);
#else
  omask = sigblock (sigmask (SIGINT) | sigmask (SIGQUIT) | sigmask (SIGHUP));
#endif
  while ((pid = waitpid (fpid->pid, &status, 0)) < 0 && errno == EINTR)
    continue;

  free (fpid);

#ifdef HAVE_SIGACTION
  sigprocmask (SIG_SETMASK, &osigs, 0);
#else
  sigsetmask (omask);
#endif
  if (pid < 0)
    return (pid);
  if (WIFEXITED (status))
    return (WEXITSTATUS (status));
  return (1);
}
