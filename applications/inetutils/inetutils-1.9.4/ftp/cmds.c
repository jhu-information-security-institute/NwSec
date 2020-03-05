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
 * Copyright (c) 1985, 1989, 1993, 1994
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

/*
 * FTP User Program -- Command Routines.
 */
#include <config.h>

#include <sys/param.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/ftp.h>

#include <ctype.h>
#include <error.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>	/* intmax_t */
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
/* Include glob.h last, because it may define "const" which breaks
   system headers on some platforms. */
#include <glob.h>

#ifdef HAVE_READLINE_READLINE_H
# include <readline/readline.h>
#elif defined HAVE_EDITLINE_READLINE_H
# include <editline/readline.h>
#endif
#ifdef HAVE_READLINE_HISTORY_H
# include <readline/history.h>
#elif defined HAVE_EDITLINE_HISTORY_H
# include <editline/history.h>
#endif

#include "ftp_var.h"
#include "unused-parameter.h"
#include "xalloc.h"
#include "xgetcwd.h"

#ifndef DEFPORT
# ifdef IPPORT_FTP
#  define DEFPORT IPPORT_FTP
# else /* !IPPORT_FTP */
#  define DEFPORT 21
# endif
#endif /* !DEFPORT */

/* Returns true if STR is entirely lower case.  */
static int
all_lower (char *str)
{
  while (*str)
    if (isupper (*str++))
      return 0;
  return 1;
}

/* Returns true if STR is entirely upper case.  */
static int
all_upper (char *str)
{
  while (*str)
    if (islower (*str++))
      return 0;
  return 1;
}

/* Destructively converts STR to lower case.  */
static char *
strdown (char *str)
{
  char *p;
  for (p = str; *p; p++)
    if (isupper (*p))
      *p = tolower (*p);
  return str;
}

jmp_buf jabort;
char *mname;
char *home = "/";

char *mapin = 0;
char *mapout = 0;

/*
 * `Another' gets another argument, and stores the new argc and argv.
 * It reverts to the top level (via main.c's intr()) on EOF/error.
 *
 * Returns false if no new arguments have been added.
 */
int
another (int *pargc, char ***pargv, const char *prompt)
{
  char *arg = NULL;
  char *buffer, *new;
  size_t size = 0, len = strlen (line);
  int ret;

  buffer = (char *) malloc (sizeof (char) * (strlen (prompt) + 4));
  if (!buffer)
    intr (0);

  sprintf (buffer, "(%s) ", prompt);

#if HAVE_READLINE
  if (usereadline)
    arg = readline (buffer);
  else
#endif /* HAVE_READLINE */
    {
      char *nl;

      fprintf (stdout, "%s", buffer);
      fflush (stdout);

      if (getline (&arg, &size, stdin) <= 0)
	{
	  free (buffer);
	  free (arg);
	  intr (0);
	}

      nl = strchr (arg, '\n');
      if (nl)
	*nl = '\0';
    }

  free (buffer);

#if HAVE_READLINE
  if (usereadline && arg && *arg)
    add_history (arg);
#endif /* HAVE_READLINE */

  if (!arg)
    intr (0);
  else if (!*arg)
    {
      free (arg);
      return 0;
    }

  new = realloc (line, sizeof (char) *
		       ((linelen ? linelen : len) + strlen (arg) + 2));
  if (!new)
    {
      free (arg);
      intr (0);
    }

  line = new;
  linelen = sizeof (char) *
	    ((linelen ? linelen : len) + strlen (arg) + 2);
  line[len++] = ' ';
  strcpy (&line[len], arg);
  free (arg);

  makeargv ();
  ret = margc > *pargc;
  *pargc = margc;
  *pargv = margv;
  return (ret);
}

/*
 * Connect to peer server and
 * auto-login, if possible.
 */
void
setpeer (int argc, char **argv)
{
  char *host = NULL;
  int port;

  if (connected && command ("NOOP") != COMPLETE)
    disconnect (0, 0);
  else if (connected)
    {
      printf ("Already connected to %s, use close first.\n", hostname);
      code = -1;
      return;
    }

  if (argc < 2)
    {
      if (hostname)
	{
          host = hostname;
          argc = 2;
        }
      else
        another (&argc, &argv, "to");
    }

  if (argc < 2 || argc > 3)
    {
      printf ("usage: %s host-name [port]\n", argv[0]);
      code = -1;
      return;
    }

  if (!host)
    host = argv[1];

  if (argc == 3)
    {
      if (isdigit(argv[2][0]) || argv[2][0] == '-')
	port = atoi (argv[2]);
      else
	{
	  struct servent *sp;

	  sp = getservbyname (argv[2], "tcp");
	  port = (sp) ? ntohs (sp->s_port) : 0;
	}

      if (port <= 0 || port > 65535)
	{
	  printf ("%s: bad port -- %s\n", argv[1], argv[2]);
	  printf ("usage: %s host-name [port]\n", argv[0]);
	  code = -1;
	  return;
	}
    }
  else
    {
      struct servent *sp;

      sp = getservbyname ("ftp", "tcp");
      port = (sp) ? ntohs (sp->s_port) : DEFPORT;
    }

  /* After hookup(), the global variable `hostname' contains
   * the canonical host name corresponding to the alias name
   * contained in HOST.  The return value of hookup() is not
   * NULL only if the server has answered our call.  The value
   * of HOST should be preserved for reporting inside login(),
   * which also detects a correct stanza of the netrc file.
   */
  if (hookup (host, port))
    {
      int overbose;

      connected = 1;
      /*
       * Set up defaults for FTP.
       */
      strcpy (typename, "ascii"), type = TYPE_A;
      curtype = TYPE_A;
      strcpy (formname, "non-print"), form = FORM_N;
      strcpy (modename, "stream"), mode = MODE_S;
      strcpy (structname, "file"), stru = STRU_F;
      strcpy (bytename, "8"), bytesize = 8;
      if (autologin)
	login (host);

#if (defined unix || defined __unix || defined __unix__) && NBBY == 8
/*
 * this ifdef is to keep someone form "porting" this to an incompatible
 * system and not checking this out. This way they have to think about it.
 */
      overbose = verbose;
      if (debug == 0)
	verbose = -1;
      if (command ("SYST") == COMPLETE && overbose)
	{
	  char *cp, c;
	  cp = strchr (reply_string + 4, ' ');
	  if (cp == NULL)
	    cp = strchr (reply_string + 4, '\r');
	  if (cp)
	    {
	      if (cp[-1] == '.')
		cp--;
	      c = *cp;
	      *cp = '\0';
	    }

	  printf ("Remote system type is %s.\n", reply_string + 4);
	  if (cp)
	    *cp = c;
	}
      if (!strncmp (reply_string, "215 UNIX Type: L8", 17))
	{
	  if (proxy)
	    unix_proxy = 1;
	  else
	    unix_server = 1;
	  /*
	   * Set type to 0 (not specified by user),
	   * meaning binary by default, but don't bother
	   * telling server.  We can use binary
	   * for text files unless changed by the user.
	   */
	  type = 0;
	  strcpy (typename, "binary");
	  if (overbose)
	    printf ("Using %s mode to transfer files.\n", typename);
	}
      else
	{
	  if (proxy)
	    unix_proxy = 0;
	  else
	    unix_server = 0;
	  if (overbose && !strncmp (reply_string, "215 TOPS20", 10))
	    printf
	      ("Remember to set tenex mode when transferring binary files from this machine.\n");
	}
      verbose = overbose;
#endif /* (unix || __unix || __unix__) && (NBBY == 8) */
    }
}

struct types
{
  char *t_name;
  char *t_mode;
  int t_type;
  char *t_arg;
} types[] =
  {
    {"ascii", "A", TYPE_A, 0},
    {"binary", "I", TYPE_I, 0},
    {"image", "I", TYPE_I, 0},
    {"ebcdic", "E", TYPE_E, 0},
    {"tenex", "L", TYPE_L, bytename},
    {NULL, NULL, 0, NULL}
  };

/*
 * Set transfer type.
 */
void
settype (int argc, char **argv)
{
  struct types *p;
  int comret;

  if (argc > 2)
    {
      char *sep;

      printf ("usage: %s [", argv[0]);
      sep = " ";
      for (p = types; p->t_name; p++)
	{
	  printf ("%s%s", sep, p->t_name);
	  sep = " | ";
	}
      printf (" ]\n");
      code = -1;
      return;
    }
  if (argc < 2)
    {
      printf ("Using %s mode to transfer files.\n", typename);
      code = 0;
      return;
    }
  for (p = types; p->t_name; p++)
    if (strcmp (argv[1], p->t_name) == 0)
      break;
  if (p->t_name == 0)
    {
      printf ("%s: unknown mode\n", argv[1]);
      code = -1;
      return;
    }
  if ((p->t_arg != NULL) && (*(p->t_arg) != '\0'))
    comret = command ("TYPE %s %s", p->t_mode, p->t_arg);
  else
    comret = command ("TYPE %s", p->t_mode);
  if (comret == COMPLETE)
    {
      strcpy (typename, p->t_name);
      curtype = type = p->t_type;
    }
}

/*
 * Internal form of settype; changes current type in use with server
 * without changing our notion of the type for data transfers.
 * Used to change to and from ascii for listings.
 */
void
changetype (int newtype, int show)
{
  struct types *p;
  int comret, oldverbose = verbose;

  if (newtype == 0)
    newtype = TYPE_I;
  if (newtype == curtype)
    return;
  if (debug == 0 && show == 0)
    verbose = 0;
  for (p = types; p->t_name; p++)
    if (newtype == p->t_type)
      break;
  if (p->t_name == 0)
    {
      printf ("ftp: internal error: unknown type %d\n", newtype);
      return;
    }
  if (newtype == TYPE_L && bytename[0] != '\0')
    comret = command ("TYPE %s %s", p->t_mode, bytename);
  else
    comret = command ("TYPE %s", p->t_mode);
  if (comret == COMPLETE)
    curtype = newtype;
  verbose = oldverbose;
}

char *stype[] = {
  "type",
  "",
  0
};

/*
 * Set binary transfer type.
 */
void
setbinary (int argc _GL_UNUSED_PARAMETER, char **argv _GL_UNUSED_PARAMETER)
{

  stype[1] = "binary";
  settype (2, stype);
}

/*
 * Set ascii transfer type.
 */
void
setascii (int argc _GL_UNUSED_PARAMETER, char **argv _GL_UNUSED_PARAMETER)
{

  stype[1] = "ascii";
  settype (2, stype);
}

/*
 * Set tenex transfer type.
 */
void
settenex (int argc _GL_UNUSED_PARAMETER, char **argv _GL_UNUSED_PARAMETER)
{

  stype[1] = "tenex";
  settype (2, stype);
}

/*
 * Set file transfer mode.
 */
void
setftmode (int argc _GL_UNUSED_PARAMETER, char **argv _GL_UNUSED_PARAMETER)
{

  printf ("We only support %s mode, sorry.\n", modename);
  code = -1;
}

/*
 * Set file transfer format.
 */
void
setform (int argc _GL_UNUSED_PARAMETER, char **argv _GL_UNUSED_PARAMETER)
{

  printf ("We only support %s format, sorry.\n", formname);
  code = -1;
}

/*
 * Set file transfer structure.
 */
void
setstruct (int argc _GL_UNUSED_PARAMETER, char **argv _GL_UNUSED_PARAMETER)
{

  printf ("We only support %s structure, sorry.\n", structname);
  code = -1;
}

/*
 * Send a single file.
 */
void
put (int argc, char **argv)
{
  char *cmd, *local, *remote;
  int loc = 0;

  if (argc == 2)
    {
      argc++;
      argv[2] = argv[1];
      loc++;
    }
  if (argc < 2 && !another (&argc, &argv, "local-file"))
    goto usage;
  if (argc < 3 && !another (&argc, &argv, "remote-file"))
    {
    usage:
      printf ("usage: %s local-file remote-file\n", argv[0]);
      code = -1;
      return;
    }

  local = globulize (argv[1]);
  if (!local)
    {
      code = -1;
      return;
    }

  /*
   * If "globulize" modifies argv[1], and argv[2] is a copy of
   * the old argv[1], make it a copy of the new argv[1].
   */
  if (loc)
    remote = strdup (local);
  else
    remote = strdup (argv[2]);

  cmd = (argv[0][0] == 'a') ? "APPE" : ((sunique) ? "STOU" : "STOR");
  if (loc && ntflag)
    {
      char *new = dotrans (remote);
      free (remote);
      remote = new;
    }
  if (loc && mapflag)
    {
      char *new = domap (remote);
      if (new != remote)
	{
	  free (remote);
	  remote = new;
	}
    }
  sendrequest (cmd, local, remote,
	       strcmp (argv[1], local) != 0 || strcmp (argv[2], remote) != 0);
  free (local);
  free (remote);
}

/*
 * Send multiple files.
 */
void
mput (int argc, char **argv)
{
  int i;
  sighandler_t oldintr;
  int ointer;

  if (argc < 2 && !another (&argc, &argv, "local-files"))
    {
      printf ("usage: %s local-files\n", argv[0]);
      code = -1;
      return;
    }
  mname = argv[0];
  mflag = 1;
  oldintr = signal (SIGINT, mabort);
  setjmp (jabort);
  if (proxy)
    {
      char *cp;

      while ((cp = remglob (argv, 0)) != NULL)
	{
	  if (*cp == 0)
	    mflag = 0;
	  if (mflag && confirm (argv[0], cp))
	    {
	      char *tp = cp;

	      if (mcase)
		{
		  if (all_upper (tp))
		    tp = strdown (strdup (tp));
		}
	      if (ntflag)
		{
		  char *new = dotrans (tp);
		  if (tp != cp)
		    free (tp);
		  tp = new;
		}
	      if (mapflag)
		{
		  char *new = domap (tp);
		  if (new != tp)
		    {
		      if (tp != cp)
			free (tp);
		      tp = new;
		    }
		}
	      sendrequest ((sunique) ? "STOU" : "STOR",
			   cp, tp, cp != tp || !interactive);
	      if (!mflag && fromatty)
		{
		  ointer = interactive;
		  interactive = 1;
		  if (confirm ("Continue with", "mput"))
		    {
		      mflag++;
		    }
		  interactive = ointer;
		}

	      if (tp != cp)
		free (tp);
	    }

	  free (cp);
	}
      signal (SIGINT, oldintr);
      mflag = 0;
      return;
    }
  for (i = 1; i < argc; i++)
    {
      char **cpp;
      glob_t gl;
      int flags;

      if (!doglob)
	{
	  if (mflag && confirm (argv[0], argv[i]))
	    {
	      char *tp = argv[i];
	      if (ntflag)
		tp = dotrans (tp);
	      if (mapflag)
		{
		  char *new = domap (tp);
		  if (new != tp)
		    {
		      if (tp != argv[i])
			free (tp);
		      tp = new;
		    }
		}
	      sendrequest ((sunique) ? "STOU" : "STOR",
			   argv[i], tp, tp != argv[i] || !interactive);
	      if (!mflag && fromatty)
		{
		  ointer = interactive;
		  interactive = 1;
		  if (confirm ("Continue with", "mput"))
		    {
		      mflag++;
		    }
		  interactive = ointer;
		}
	      if (tp != argv[i])
		free (tp);
	    }
	  continue;
	}

      memset (&gl, 0, sizeof (gl));
      flags = GLOB_NOCHECK;
#ifdef GLOB_BRACE
      flags |= GLOB_BRACE;
#endif
#ifdef GLOB_TILDE
      flags |= GLOB_TILDE;
#endif
#ifdef GLOB_QUOTE
      flags |= GLOB_QUOTE;
#endif
      if (glob (argv[i], flags, NULL, &gl) || gl.gl_pathc == 0)
	{
	  error (0, 0, "%s: not found", argv[i]);
	  globfree (&gl);
	  continue;
	}
      for (cpp = gl.gl_pathv; cpp && *cpp != NULL; cpp++)
	{
	  if (mflag && confirm (argv[0], *cpp))
	    {
	      char *tp = *cpp;
	      if (ntflag)
		tp = dotrans (tp);
	      if (mapflag)
		{
		  char *new = domap (tp);
		  if (new != tp)
		    {
		      if (tp != *cpp)
			free (tp);
		      tp = new;
		    }
		}
	      sendrequest ((sunique) ? "STOU" : "STOR",
			   *cpp, tp, *cpp != tp || !interactive);
	      if (!mflag && fromatty)
		{
		  ointer = interactive;
		  interactive = 1;
		  if (confirm ("Continue with", "mput"))
		    {
		      mflag++;
		    }
		  interactive = ointer;
		}
	      if (tp != *cpp)
		free (tp);
	    }
	}
      globfree (&gl);
    }
  signal (SIGINT, oldintr);
  mflag = 0;
}

void
reget (int argc, char **argv)
{

  getit (argc, argv, 1, "r+w");
}

void
get (int argc, char **argv)
{

  getit (argc, argv, 0, restart_point ? "r+w" : "w");
}

/*
 * Receive one file.
 */
int
getit (int argc, char **argv, int restartit, char *mode)
{
  int loc = 0;
  char *local;

  if (argc == 2)
    {
      argc++;
      argv[2] = argv[1];
      loc++;
    }
  if (argc < 2 && !another (&argc, &argv, "remote-file"))
    goto usage;
  if (argc < 3 && !another (&argc, &argv, "local-file"))
    {
    usage:
      printf ("usage: %s remote-file [ local-file ]\n", argv[0]);
      code = -1;
      return (0);
    }

  local = globulize (argv[2]);
  if (!local)
    {
      code = -1;
      return (0);
    }
  if (loc && mcase && all_upper (local))
    strdown (local);
  if (loc && ntflag)
    {
      char *new = dotrans (local);
      free (local);
      local = new;
    }
  if (loc && mapflag)
    {
      char *new = domap (local);
      if (new != local)
	{
	  free (local);
	  local = new;
	}
    }
  if (restartit)
    {
      struct stat stbuf;
      int ret;

      ret = stat (local, &stbuf);
      if (restartit == 1)
	{
	  if (ret < 0)
	    {
	      error (0, errno, "local: %s", local);
	      free (local);
	      return (0);
	    }
	  restart_point = stbuf.st_size;
	}
      else
	{
	  if (ret == 0)
	    {
	      int overbose;

	      overbose = verbose;
	      if (debug == 0)
		verbose = -1;
	      if (command ("MDTM %s", argv[1]) == COMPLETE)
		{
		  int yy, mo, day, hour, min, sec;
		  struct tm *tm;
		  verbose = overbose;
		  sscanf (reply_string,
			  "%*s %04d%02d%02d%02d%02d%02d",
			  &yy, &mo, &day, &hour, &min, &sec);
		  tm = gmtime (&stbuf.st_mtime);
		  tm->tm_mon++;
		  if (tm->tm_year + 1900 > yy)
		    {
		      free (local);
		      return (1);
		    }
		  if ((tm->tm_year + 1900 == yy &&
		       tm->tm_mon > mo) ||
		      (tm->tm_mon == mo &&
		       tm->tm_mday > day) ||
		      (tm->tm_mday == day &&
		       tm->tm_hour > hour) ||
		      (tm->tm_hour == hour &&
		       tm->tm_min > min) ||
		      (tm->tm_min == min && tm->tm_sec > sec))
		    {
		      free (local);
		      return (1);
		    }
		}
	      else
		{
		  printf ("%s\n", reply_string);
		  verbose = overbose;
		  free (local);
		  return (0);
		}
	    }
	}
    }

  recvrequest ("RETR", local, argv[1], mode, strcmp (local, argv[2]) != 0);
  restart_point = 0;
  free (local);
  return (0);
}

void
mabort (int signo _GL_UNUSED_PARAMETER)
{
  int ointer;

  printf ("\n");
  fflush (stdout);
  if (mflag && fromatty)
    {
      ointer = interactive;
      interactive = 1;
      if (confirm ("Continue with", mname))
	{
	  interactive = ointer;
	  longjmp (jabort, 0);
	}
      interactive = ointer;
    }
  mflag = 0;
  longjmp (jabort, 0);
}

/*
 * Get multiple files.
 */
void
mget (int argc, char **argv)
{
  sighandler_t oldintr;
  int ointer;
  char *cp, *tp;

  if (argc < 2 && !another (&argc, &argv, "remote-files"))
    {
      printf ("usage: %s remote-files\n", argv[0]);
      code = -1;
      return;
    }
  mname = argv[0];
  mflag = 1;
  oldintr = signal (SIGINT, mabort);
  setjmp (jabort);
  while ((cp = remglob (argv, proxy)) != NULL)
    {
      if (*cp == '\0')
	{
	  mflag = 0;
	  continue;
	}
      if (mflag && confirm (argv[0], cp))
	{
	  tp = cp;
	  if (mcase && !all_lower (tp))
	    tp = strdown (strdup (tp));
	  if (ntflag)
	    {
	      char *new = dotrans (tp);
	      if (tp != cp)
		free (tp);
	      tp = new;
	    }
	  if (mapflag)
	    {
	      char *new = domap (tp);
	      if (new != tp)
		{
		  if (tp != cp)
		    free (tp);
		  tp = new;
		}
	    }
	  recvrequest ("RETR", tp, cp, "w", tp != cp || !interactive);
	  if (!mflag && fromatty)
	    {
	      ointer = interactive;
	      interactive = 1;
	      if (confirm ("Continue with", "mget"))
		{
		  mflag++;
		}
	      interactive = ointer;
	    }
	  if (tp != cp)
	    free (tp);
	}
      free (cp);
    }
  signal (SIGINT, oldintr);
  mflag = 0;
}

char *
remglob (char **argv, int doswitch)
{
  static FILE *ftemp = NULL;
  static char **args;
  int buf_len = 0;
  char *buf = 0;
  int sofar = 0;
  int oldverbose, oldhash;
  int fd;
  char *cp, *mode;

  if (!mflag)
    {
      if (!doglob)
	{
	  args = NULL;
	}
      else
	{
	  if (ftemp)
	    {
	      fclose (ftemp);
	      ftemp = NULL;
	    }
	}
      return (NULL);
    }
  if (!doglob)
    {
      if (args == NULL)
	args = argv;
      if ((cp = *++args) == NULL)
	args = NULL;
      return cp ? 0 : strdup (cp);
    }
  if (ftemp == NULL)
    {
      char temp[sizeof PATH_TMP + sizeof "XXXXXX"];

      strcpy (temp, PATH_TMP);
      strcat (temp, "XXXXXX");
#ifdef HAVE_MKSTEMP
      fd = mkstemp (temp);
#else
      if (mktemp (temp) != NULL)
	fd = open (temp, O_CREAT | O_EXCL | O_RDWR, 0600);
      else
	fd = -1;
#endif
      if (fd < 0)
	{
	  printf ("unable to create temporary file %s: %s\n", temp,
		  strerror (errno));
	  return (NULL);
	}
      close (fd);

      oldverbose = verbose, verbose = 0;
      oldhash = hash, hash = 0;
      if (doswitch)
	{
	  pswitch (!proxy);
	}
      for (mode = "w"; *++argv != NULL; mode = "a")
	recvrequest ("NLST", temp, *argv, mode, 0);
      if (doswitch)
	{
	  pswitch (!proxy);
	}
      verbose = oldverbose;
      hash = oldhash;
      ftemp = fopen (temp, "r");
      unlink (temp);
      if (ftemp == NULL)
	{
	  printf ("can't find list of remote files, oops\n");
	  return (NULL);
	}
    }

  buf_len = 100;		/* Any old size */
  buf = malloc (buf_len + 1);

  sofar = 0;
  for (;;)
    {
      if (!buf)
	{
	  printf ("malloc failure\n");
	  return 0;
	}
      if (!fgets (buf + sofar, buf_len - sofar, ftemp))
	{
	  fclose (ftemp);
	  ftemp = NULL;
	  free (buf);
	  return 0;
	}

      sofar = strlen (buf);
      if (buf[sofar - 1] == '\n')
	{
	  buf[sofar - 1] = '\0';
	  return buf;
	}

      /* Make more room and read some more... */
      buf_len += buf_len;
      buf = realloc (buf, buf_len);
    }
}

char *
onoff (int bool)
{
  return (bool ? "on" : "off");
}

/*
 * Show status.
 */
void
status (int argc _GL_UNUSED_PARAMETER, char **argv _GL_UNUSED_PARAMETER)
{
  int i;

  if (connected)
    printf ("Connected to %s.\n", hostname);
  else
    printf ("Not connected.\n");
  printf ("Connection addressing: %s\n",
	  (usefamily == AF_UNSPEC) ? "any"
	    : (usefamily == AF_INET6) ? "IPv6" : "IPv4");
  if (!proxy)
    {
      pswitch (1);
      if (connected)
	{
	  printf ("Connected for proxy commands to %s.\n", hostname);
	}
      else
	{
	  printf ("No proxy connection.\n");
	}
      pswitch (0);
    }
  printf ("Mode: %s; Type: %s; Form: %s; Structure: %s\n",
	  modename, typename, formname, structname);
  printf ("Verbose: %s; Bell: %s; Prompting: %s; Globbing: %s\n",
	  onoff (verbose), onoff (bell), onoff (interactive), onoff (doglob));
  printf ("Store unique: %s; Receive unique: %s\n", onoff (sunique),
	  onoff (runique));
  printf ("Case: %s; CR stripping: %s\n", onoff (mcase), onoff (crflag));
  if (ntflag)
    {
      printf ("Ntrans: (in) %s (out) %s\n", ntin, ntout);
    }
  else
    {
      printf ("Ntrans: off\n");
    }
  if (mapflag)
    {
      printf ("Nmap: (in) %s (out) %s\n", mapin, mapout);
    }
  else
    {
      printf ("Nmap: off\n");
    }
  printf ("Hash mark printing: %s; Use of PORT cmds: %s\n",
	  onoff (hash), onoff (sendport));
  printf ("Use of EPRT/EPSV for IPv4: %s\n", onoff (doepsv4));
  if (macnum > 0)
    {
      printf ("Macros:\n");
      for (i = 0; i < macnum; i++)
	{
	  printf ("\t%s\n", macros[i].mac_name);
	}
    }
  code = 0;
}

/*
 * Set beep on cmd completed mode.
 */
void
setbell (int argc _GL_UNUSED_PARAMETER, char **argv _GL_UNUSED_PARAMETER)
{

  bell = !bell;
  printf ("Bell mode %s.\n", onoff (bell));
  code = bell;
}

/*
 * Turn on packet tracing.
 */
void
settrace (int argc _GL_UNUSED_PARAMETER, char **argv _GL_UNUSED_PARAMETER)
{

  trace = !trace;
  printf ("Packet tracing %s.\n", onoff (trace));
  code = trace;
}

/*
 * Toggle hash mark printing during transfers.
 *
 * Parse multipliers 'k', 'K', 'm', 'M', and
 * 'g', 'G' to simplify the size step.
 *
 * With a numerical argument, hash marking is
 * made active, and the step size is updated.
 * Toggle state only in absence of an argument.
 */
void
sethash (int argc _GL_UNUSED_PARAMETER, char **argv)
{
  char *p = argv[1];

  /* P is NULL when no argument was passed with `hash'.  */
  while (p && isdigit (*p))
    p++;

  if (argv[1] != NULL)
    sscanf (argv[1], "%d", &hashbytes);

  /* Apply a multiplier only if a numerical part exists.  */
  if (argv[1] && isdigit (*argv[1]))
    {
      hash = 1;			/* Enforce markers.  */

      switch (*p)
	{
	case 'g':
	case 'G':
	  hashbytes *= 1024;	/* Cascaded multiplication!  */
	case 'm':
	case 'M':
	  hashbytes *= 1024;
	case 'k':
	case 'K':
	  hashbytes *= 1024;
	}
    }

  if (hashbytes <= 0)
    hashbytes = 1024;

  if (!argv[1])			/* Toggle when argument is absent.  */
    hash = !hash;

  printf ("Hash mark printing %s", onoff (hash));
  if (hash)
    printf (" (%d bytes/hash mark)", hashbytes);
  printf (".\n");

  code = hash;
}

/*
 * Turn on printing of server echo's.
 */
void
setverbose (int argc _GL_UNUSED_PARAMETER, char **argv _GL_UNUSED_PARAMETER)
{

  verbose = !verbose;
  printf ("Verbose mode %s.\n", onoff (verbose));
  code = verbose;
}

/*
 * Allow any address family.
 */
void
setipany (int argc _GL_UNUSED_PARAMETER, char **argv _GL_UNUSED_PARAMETER)
{
  usefamily = AF_UNSPEC;
  printf ("Selecting addresses: %s.\n", "any");
  code = usefamily;
}

/*
 * Restrict to IPv4 addresses.
 */
void
setipv4 (int argc _GL_UNUSED_PARAMETER, char **argv _GL_UNUSED_PARAMETER)
{
  usefamily = AF_INET;
  printf ("Selecting addresses: %s.\n", "IPv4");
  code = usefamily;
}

/*
 * Restrict to IPv6 addresses.
 */
void
setipv6 (int argc _GL_UNUSED_PARAMETER, char **argv _GL_UNUSED_PARAMETER)
{
  usefamily = AF_INET6;
  printf ("Selecting addresses: %s.\n", "IPv6");
  code = usefamily;
}

/*
 * Toggle use of EPRT/EPRT for IPv4.
 */
void
setepsv4 (int argc _GL_UNUSED_PARAMETER, char **argv _GL_UNUSED_PARAMETER)
{

  doepsv4 = !doepsv4;
  printf ("Use of EPRT/EPSV for IPv4: %s.\n", onoff (doepsv4));
  code = doepsv4;
}

/*
 * Toggle PORT cmd use before each data connection.
 */
void
setport (int argc _GL_UNUSED_PARAMETER, char **argv _GL_UNUSED_PARAMETER)
{

  sendport = !sendport;
  printf ("Use of PORT cmds %s.\n", onoff (sendport));
  code = sendport;
}

/*
 * Turn on interactive prompting
 * during mget, mput, and mdelete.
 */
void
setprompt (int argc _GL_UNUSED_PARAMETER, char **argv _GL_UNUSED_PARAMETER)
{

  interactive = !interactive;
  printf ("Interactive mode %s.\n", onoff (interactive));
  code = interactive;
}

/*
 * Toggle metacharacter interpretation
 * on local file names.
 */
void
setglob (int argc _GL_UNUSED_PARAMETER, char **argv _GL_UNUSED_PARAMETER)
{

  doglob = !doglob;
  printf ("Globbing %s.\n", onoff (doglob));
  code = doglob;
}

/*
 * Set debugging mode on/off and/or
 * set level of debugging.
 */
void
setdebug (int argc, char **argv)
{
  int val;

  if (argc > 1)
    {
      val = atoi (argv[1]);
      if (val < 0)
	{
	  printf ("%s: bad debugging value.\n", argv[1]);
	  code = -1;
	  return;
	}
    }
  else
    val = !debug;
  debug = val;
  if (debug)
    options |= SO_DEBUG;
  else
    options &= ~SO_DEBUG;
  printf ("Debugging %s (debug=%d).\n", onoff (debug), debug);
  code = debug > 0;
}

/*
 * Set current working directory
 * on remote machine.
 */
void
cd (int argc, char **argv)
{

  if (argc < 2 && !another (&argc, &argv, "remote-directory"))
    {
      printf ("usage: %s remote-directory\n", argv[0]);
      code = -1;
      return;
    }
  if (command ("CWD %s", argv[1]) == ERROR && code == 500)
    {
      if (verbose)
	printf ("CWD command not recognized, trying XCWD\n");
      command ("XCWD %s", argv[1]);
    }
}

/*
 * Set current working directory
 * on local machine.
 */
void
lcd (int argc, char **argv)
{
  char *dir;

  if (argc < 2)
    argc++, argv[1] = home;
  if (argc != 2)
    {
      printf ("usage: %s local-directory\n", argv[0]);
      code = -1;
      return;
    }

  dir = globulize (argv[1]);
  if (!dir)
    {
      code = -1;
      return;
    }

  if (chdir (dir) < 0)
    {
      error (0, errno, "dir: %s", dir);
      free (dir);
      code = -1;
      return;
    }

  free (dir);

  dir = xgetcwd ();
  if (dir)
    {
      printf ("Local directory now %s\n", dir);
      free (dir);
    }
  else
    error (0, errno, "getcwd");
  code = 0;
}

/*
 * Delete a single file.
 */
void
delete (int argc, char **argv)
{

  if (argc < 2 && !another (&argc, &argv, "remote-file"))
    {
      printf ("usage: %s remote-file\n", argv[0]);
      code = -1;
      return;
    }
  command ("DELE %s", argv[1]);
}

/*
 * Delete multiple files.
 */
void
mdelete (int argc, char **argv)
{
  sighandler_t oldintr;
  int ointer;
  char *cp;

  if (argc < 2 && !another (&argc, &argv, "remote-files"))
    {
      printf ("usage: %s remote-files\n", argv[0]);
      code = -1;
      return;
    }
  mname = argv[0];
  mflag = 1;
  oldintr = signal (SIGINT, mabort);
  setjmp (jabort);
  while ((cp = remglob (argv, 0)) != NULL)
    {
      if (*cp == '\0')
	{
	  mflag = 0;
	  continue;
	}
      if (mflag && confirm (argv[0], cp))
	{
	  command ("DELE %s", cp);
	  if (!mflag && fromatty)
	    {
	      ointer = interactive;
	      interactive = 1;
	      if (confirm ("Continue with", "mdelete"))
		{
		  mflag++;
		}
	      interactive = ointer;
	    }
	}
      free (cp);
    }
  signal (SIGINT, oldintr);
  mflag = 0;
}

/*
 * Rename a remote file.
 */
void
renamefile (int argc, char **argv)
{

  if (argc < 2 && !another (&argc, &argv, "from-name"))
    goto usage;
  if (argc < 3 && !another (&argc, &argv, "to-name"))
    {
    usage:
      printf ("%s from-name to-name\n", argv[0]);
      code = -1;
      return;
    }
  if (command ("RNFR %s", argv[1]) == CONTINUE)
    command ("RNTO %s", argv[2]);
}

/*
 * Get a directory listing
 * of remote files.
 */
void
ls (int argc, char **argv)
{
  char *cmd, *dest;

  if (argc < 2)
    argc++, argv[1] = NULL;
  if (argc < 3)
    argc++, argv[2] = "-";
  if (argc > 3)
    {
      printf ("usage: %s remote-directory local-file\n", argv[0]);
      code = -1;
      return;
    }
  cmd = argv[0][0] == 'n' ? "NLST" : "LIST";

  if (strcmp (argv[2], "-") != 0)
    {
      dest = globulize (argv[2]);
      if (!dest)
	{
	  code = -1;
	  return;
	}
      if (*dest != '|' && !confirm ("output to local-file:", dest))
	{
	  code = -1;
	  goto out;
	}
    }
  else
    dest = 0;

  recvrequest (cmd, dest ? dest : "-", argv[1], "w", 0);
out:
  free (dest);
}

/*
 * Get a directory listing
 * of multiple remote files.
 */
void
mls (int argc, char **argv)
{
  sighandler_t oldintr;
  int ointer, i;
  char *cmd, mode[1], *dest;

  if (argc < 2 && !another (&argc, &argv, "remote-files"))
    goto usage;
  if (argc < 3 && !another (&argc, &argv, "local-file"))
    {
    usage:
      printf ("usage: %s remote-files local-file\n", argv[0]);
      code = -1;
      return;
    }

  dest = argv[argc - 1];
  argv[argc - 1] = NULL;
  if (strcmp (dest, "-") && *dest != '|')
    {
      dest = globulize (dest);
      if (!dest)
	{
	  code = -1;
	  return;
	}
      if (!confirm ("output to local-file:", dest))
	{
	  code = -1;
	  free (dest);
	  return;
	}
    }
  else
    dest = strdup (dest);

  cmd = argv[0][1] == 'l' ? "NLST" : "LIST";
  mname = argv[0];
  mflag = 1;
  oldintr = signal (SIGINT, mabort);
  setjmp (jabort);
  for (i = 1; mflag && i < argc - 1; ++i)
    {
      *mode = (i == 1) ? 'w' : 'a';
      recvrequest (cmd, dest, argv[i], mode, 0);
      if (!mflag && fromatty)
	{
	  ointer = interactive;
	  interactive = 1;
	  if (confirm ("Continue with", argv[0]))
	    {
	      mflag++;
	    }
	  interactive = ointer;
	}
    }

  signal (SIGINT, oldintr);
  mflag = 0;
  free (dest);
}

/*
 * Do a shell escape
 */
void
shell (int argc, char **argv _GL_UNUSED_PARAMETER)
{
  pid_t pid;
  sighandler_t old1, old2;
  char shellnam[40], *shell, *namep;

  old1 = signal (SIGINT, SIG_IGN);
  old2 = signal (SIGQUIT, SIG_IGN);
  if ((pid = fork ()) == 0)
    {
      for (pid = 3; pid < 20; pid++)
	close (pid);
      signal (SIGINT, SIG_DFL);
      signal (SIGQUIT, SIG_DFL);
      shell = getenv ("SHELL");
      if (shell == NULL)
	shell = PATH_BSHELL;
      namep = strrchr (shell, '/');
      if (namep == NULL)
	namep = shell;		/* No slash in this name.  */
      else
	namep++;		/* Skip the slash character.  */
      strcpy (shellnam, "-");
      strncat (shellnam, namep, sizeof (shellnam) - 2);
      if (strcmp (namep, "sh") != 0)
	shellnam[0] = '+';
      if (debug)
	{
	  printf ("%s\n", shell);
	  fflush (stdout);
	}
      if (argc > 1)
	{
	  execl (shell, shellnam, "-c", altarg, (char *) 0);
	}
      else
	{
	  execl (shell, shellnam, (char *) 0);
	}
      error (0, errno, "shell");
      code = -1;
      exit (EXIT_FAILURE);
    }
  if (pid > 0)
    while (wait (0) != pid)
      ;
  signal (SIGINT, old1);
  signal (SIGQUIT, old2);
  if (pid == -1)
    {
      error (0, errno, "Try again later");
      code = -1;
    }
  else
    {
      code = 0;
    }
}

/*
 * Send new user information (re-login)
 */
void
user (int argc, char **argv)
{
  char acct[80];
  int n, aflag = 0;

  if (argc < 2)
    another (&argc, &argv, "username");
  if (argc < 2 || argc > 4)
    {
      printf ("usage: %s username [password] [account]\n", argv[0]);
      code = -1;
      return;
    }
  n = command ("USER %s", argv[1]);
  if (n == CONTINUE)
    {
      /* Is this a case of challenge-response?
       * RFC 2228 stipulates code 336 for this.
       * Suppress message in verbose mode, since
       * it has already been displayed.
       */
      if (code == 336 && !verbose)
	printf ("%s\n", reply_string + strlen ("336 "));
      /* In addition, any password given on the
       * command line is irrelevant, so ignore it.
       */
      if (argc < 3 || code == 336)
	argv[2] = getpass ("Password: ");
      if (argc < 3)
	argc++;
      n = command ("PASS %s", argv[2]);
      if (argv[2])
	memset (argv[2], 0, strlen (argv[2]));
    }
  if (n == CONTINUE)
    {
      if (argc < 4)
	{
	  printf ("Account: ");
	  fflush (stdout);
	  if (fgets (acct, sizeof (acct) - 1, stdin))
	    acct[strlen (acct) - 1] = '\0';	/* Erase newline.  */
	  else
	    acct[0] = '\0';			/* Set empty name.  */
	  argv[3] = acct;
	  argc++;
	}
      n = command ("ACCT %s", argv[3]);
      aflag++;
    }
  if (n != COMPLETE)
    {
      fprintf (stdout, "Login failed.\n");
      return;
    }
  if (!aflag && argc == 4)
    {
      command ("ACCT %s", argv[3]);
    }
}

/*
 * Print working directory.
 */
void
pwd (int argc _GL_UNUSED_PARAMETER, char **argv _GL_UNUSED_PARAMETER)
{
  int oldverbose = verbose;

  /*
   * If we aren't verbose, this doesn't do anything!
   */
  verbose = 1;
  if (command ("PWD") == ERROR && code == 500)
    {
      printf ("PWD command not recognized, trying XPWD\n");
      command ("XPWD");
    }
  verbose = oldverbose;
}

/*
 * Print local working directory.
 */
void
lpwd (int argc _GL_UNUSED_PARAMETER, char **argv _GL_UNUSED_PARAMETER)
{
  char *dir = xgetcwd ();

  if (dir)
    {
      printf ("Local directory is %s\n", dir);
      free (dir);
    }
  else
    error (0, errno, "getcwd");

  code = 0;
}

/*
 * Make a directory.
 */
void
makedir (int argc, char **argv)
{

  if (argc < 2 && !another (&argc, &argv, "directory-name"))
    {
      printf ("usage: %s directory-name\n", argv[0]);
      code = -1;
      return;
    }
  if (command ("MKD %s", argv[1]) == ERROR && code == 500)
    {
      if (verbose)
	printf ("MKD command not recognized, trying XMKD\n");
      command ("XMKD %s", argv[1]);
    }
}

/*
 * Remove a directory.
 */
void
removedir (int argc, char **argv)
{

  if (argc < 2 && !another (&argc, &argv, "directory-name"))
    {
      printf ("usage: %s directory-name\n", argv[0]);
      code = -1;
      return;
    }
  if (command ("RMD %s", argv[1]) == ERROR && code == 500)
    {
      if (verbose)
	printf ("RMD command not recognized, trying XRMD\n");
      command ("XRMD %s", argv[1]);
    }
}

/*
 * Send a line, verbatim, to the remote machine.
 */
void
quote (int argc, char **argv)
{

  if (argc < 2 && !another (&argc, &argv, "command line to send"))
    {
      printf ("usage: %s line-to-send\n", argv[0]);
      code = -1;
      return;
    }
  quote1 ("", argc, argv);
}

/*
 * Send a SITE command to the remote machine.  The line
 * is sent verbatim to the remote machine, except that the
 * word "SITE" is added at the front.
 */
void
site (int argc, char **argv)
{

  if (argc < 2 && !another (&argc, &argv, "arguments to SITE command"))
    {
      printf ("usage: %s line-to-send\n", argv[0]);
      code = -1;
      return;
    }
  quote1 ("SITE ", argc, argv);
}

/*
 * Turn argv[1..argc) into a space-separated string, then prepend initial text.
 * Send the result as a one-line command and get response.
 */
void
quote1 (char *initial, int argc, char **argv)
{
  int i, len;
  char buf[BUFSIZ];		/* must be >= sizeof(line) */

  strcpy (buf, initial);
  if (argc > 1)
    {
      len = strlen (buf);
      len += strlen (strcpy (&buf[len], argv[1]));
      for (i = 2; i < argc; i++)
	{
	  buf[len++] = ' ';
	  len += strlen (strcpy (&buf[len], argv[i]));
	}
    }
  if (command (buf) == PRELIM)
    {
      while (getreply (0) == PRELIM)
	continue;
    }
}

void
do_chmod (int argc, char **argv)
{

  if (argc < 2 && !another (&argc, &argv, "mode"))
    goto usage;
  if (argc < 3 && !another (&argc, &argv, "file-name"))
    {
    usage:
      printf ("usage: %s mode file-name\n", argv[0]);
      code = -1;
      return;
    }
  command ("SITE CHMOD %s %s", argv[1], argv[2]);
}

void
do_umask (int argc, char **argv)
{
  int oldverbose = verbose;

  verbose = 1;
  command (argc == 1 ? "SITE UMASK" : "SITE UMASK %s", argv[1]);
  verbose = oldverbose;
}

void
site_idle (int argc, char **argv)
{
  int oldverbose = verbose;

  verbose = 1;
  command (argc == 1 ? "SITE IDLE" : "SITE IDLE %s", argv[1]);
  verbose = oldverbose;
}

/*
 * Ask the other side for help.
 */
void
rmthelp (int argc, char **argv)
{
  int oldverbose = verbose;

  verbose = 1;
  command (argc == 1 ? "HELP" : "HELP %s", argv[1]);
  verbose = oldverbose;
}

/*
 * Terminate session and exit.
 */
void
quit (int argc _GL_UNUSED_PARAMETER, char **argv _GL_UNUSED_PARAMETER)
{

  if (connected)
    disconnect (0, 0);
  pswitch (1);
  if (connected)
    {
      disconnect (0, 0);
    }
  exit (EXIT_SUCCESS);
}

/*
 * Terminate session, but don't exit.
 */
void
disconnect (int argc _GL_UNUSED_PARAMETER, char **argv _GL_UNUSED_PARAMETER)
{

  if (!connected)
    return;
  command ("QUIT");
  if (cout)
    {
      fclose (cout);
    }
  cout = NULL;
  connected = 0;
  data = -1;
  if (!proxy)
    {
      macnum = 0;
    }
}

int
confirm (char *cmd, char *file)
{
  char input[BUFSIZ];

  if (!interactive)
    return (1);
  printf ("%s %s? ", cmd, file);
  fflush (stdout);
  if (fgets (input, sizeof input, stdin) == NULL)
    return (0);
  return (*input != 'n' && *input != 'N');
}

void
fatal (char *msg)
{

  error (EXIT_FAILURE, 0, "%s", msg);
}

/*
 * Glob a local file name specification with
 * the expectation of a single return value.
 * Can't control multiple values being expanded
 * from the expression, we return only the first.
 */
char *
globulize (char *cp)
{
  glob_t gl;
  int flags;

  if (!doglob)
    return strdup (cp);

  flags = GLOB_BRACE | GLOB_NOCHECK | GLOB_TILDE;
#ifdef GLOB_QUOTE
  flags |= GLOB_QUOTE;
#endif

  memset (&gl, 0, sizeof (gl));
  if (glob (cp, flags, NULL, &gl) || gl.gl_pathc == 0)
    {
      error (0, 0, "%s: not found", cp);
      globfree (&gl);
      return (0);
    }

  cp = strdup (gl.gl_pathv[0]);
  globfree (&gl);

  return cp;
}

void
account (int argc, char **argv)
{
  char acct[50], *ap;

  if (argc > 1)
    {
      ++argv;
      --argc;
      strncpy (acct, *argv, sizeof (acct) - 1);
      acct[sizeof (acct) - 1] = '\0';
      while (argc > 1)
	{
	  --argc;
	  ++argv;
	  strncat (acct, *argv, (sizeof (acct) - 1) - strlen (acct));
	}
      ap = acct;
    }
  else
    {
      ap = getpass ("Account:");
    }
  command ("ACCT %s", ap);
  if (ap)
    memset (ap, 0, strlen (ap));
}

jmp_buf abortprox;

void
proxabort (int sig _GL_UNUSED_PARAMETER)
{

  if (!proxy)
    {
      pswitch (1);
    }
  if (connected)
    {
      proxflag = 1;
    }
  else
    {
      proxflag = 0;
    }
  pswitch (0);
  longjmp (abortprox, 1);
}

void
doproxy (int argc, char **argv)
{
  struct cmd *c;
  sighandler_t oldintr;

  if (argc < 2 && !another (&argc, &argv, "command"))
    {
      printf ("usage: %s command\n", argv[0]);
      code = -1;
      return;
    }
  c = getcmd (argv[1]);
  if (c == (struct cmd *) -1)
    {
      printf ("?Ambiguous command\n");
      fflush (stdout);
      code = -1;
      return;
    }
  if (c == 0)
    {
      printf ("?Invalid command\n");
      fflush (stdout);
      code = -1;
      return;
    }
  if (!c->c_proxy)
    {
      printf ("?Invalid proxy command\n");
      fflush (stdout);
      code = -1;
      return;
    }
  if (setjmp (abortprox))
    {
      code = -1;
      return;
    }
  oldintr = signal (SIGINT, proxabort);
  pswitch (1);
  if (c->c_conn && !connected)
    {
      printf ("Not connected\n");
      fflush (stdout);
      pswitch (0);
      signal (SIGINT, oldintr);
      code = -1;
      return;
    }
  (*c->c_handler) (argc - 1, argv + 1);
  if (connected)
    {
      proxflag = 1;
    }
  else
    {
      proxflag = 0;
    }
  pswitch (0);
  signal (SIGINT, oldintr);
}

void
setcase (int argc _GL_UNUSED_PARAMETER, char **argv _GL_UNUSED_PARAMETER)
{

  mcase = !mcase;
  printf ("Case mapping %s.\n", onoff (mcase));
  code = mcase;
}

void
setcr (int argc _GL_UNUSED_PARAMETER, char **argv _GL_UNUSED_PARAMETER)
{

  crflag = !crflag;
  printf ("Carriage Return stripping %s.\n", onoff (crflag));
  code = crflag;
}

void
setntrans (int argc, char **argv)
{
  if (argc == 1)
    {
      ntflag = 0;
      printf ("Ntrans off.\n");
      code = ntflag;
      return;
    }
  ntflag++;
  code = ntflag;
  strncpy (ntin, argv[1], sizeof (ntin) - 1);
  ntin[sizeof (ntin) - 1] = '\0';
  if (argc == 2)
    {
      ntout[0] = '\0';
      return;
    }
  strncpy (ntout, argv[2], sizeof (ntout) - 1);
  ntout[sizeof (ntout) - 1] = '\0';
}

/* NOTE: dotrans() always returns a newly allocated string.
 */

char *
dotrans (char *name)
{
  char *new = xmalloc (strlen (name) + 1);
  char *cp1, *cp2 = new;
  size_t i, ostop, found;

  for (ostop = 0; *(ntout + ostop) && ostop < sizeof (ntout) - 1; ostop++)
    continue;
  for (cp1 = name; *cp1; cp1++)
    {
      found = 0;
      for (i = 0; *(ntin + i) && i < sizeof (ntin) - 1; i++)
	{
	  if (*cp1 == *(ntin + i))
	    {
	      found++;
	      if (i < ostop)
		{
		  *cp2++ = *(ntout + i);
		}
	      break;
	    }
	}
      if (!found)
	{
	  *cp2++ = *cp1;
	}
    }
  *cp2 = '\0';
  return (new);
}

void
setpassive (int argc _GL_UNUSED_PARAMETER, char **argv _GL_UNUSED_PARAMETER)
{

  passivemode = !passivemode;
  printf ("Passive mode %s.\n", onoff (passivemode));
  code = passivemode;
}

void
setnmap (int argc, char **argv)
{
  char *cp;

  if (argc == 1)
    {
      mapflag = 0;
      printf ("Nmap off.\n");
      code = mapflag;
      return;
    }
  if (argc < 3 && !another (&argc, &argv, "mapout"))
    {
      printf ("Usage: %s [mapin mapout]\n", argv[0]);
      code = -1;
      return;
    }
  mapflag = 1;
  code = 1;
  cp = strchr (altarg, ' ');
  if (proxy)
    {
      while (*++cp == ' ')
	continue;
      altarg = cp;
      cp = strchr (altarg, ' ');
    }
  *cp = '\0';

  free (mapin);
  mapin = strdup (altarg);

  while (*++cp == ' ')
    continue;
  free (mapout);
  mapout = strdup (cp);
}

static int
cp_subst (char **from_p, char **to_p, int *toks, char **tp, char **te, char *tok0, char **buf_p, int *buf_len_p)
{
  int toknum;
  char *src;
  size_t src_len;

  if (*++(*from_p) == '0')
    {
      src = tok0;
      src_len = strlen (tok0);
    }
  else if (toks[toknum = **from_p - '1'])
    {
      src = tp[toknum];
      src_len = te[toknum] - src;
    }
  else
    return 0;

  if (src_len > strlen ("$2"))
    {
      /* This substitution will be longer than the original text.
       * Allocate a larger buffer and update the cursor, pointing
       * within the new memory area.
       */
      size_t offset = *to_p - *buf_p;

      *buf_len_p += src_len - strlen ("$2");
      *buf_p = realloc (*buf_p, *buf_len_p);
      *to_p = *buf_p + offset;
    }

  while (src_len--)
    *(*to_p)++ = *src++;

  return 1;
}

/* NOTE: domap() can return a newly allocated string,
 * but need not do so every time.
 */

char *
domap (char *name)
{
  /* The string `mapout' will have its tokens expanded,
   * but is essentially the minimal output string.
   * Some brackets and some alternate strings might
   * need to be suppressed.
   */
  int buf_len = strlen (mapout) + 1;
  char *buf = xmalloc (buf_len);
  char *cp1 = name, *cp2 = mapin;
  char *tp[9], *te[9];
  int i, toks[9], toknum = 0, match = 1;

  for (i = 0; i < 9; ++i)
    {
      toks[i] = 0;
    }

  /* Tokenize the input pattern against incoming file name.
   */
  while (match && *cp1 && *cp2)
    {
      switch (*cp2)
	{
	case '\\':
	  if (*++cp2 != *cp1)
	    {
	      match = 0;
	    }
	  break;
	case '$':
	  if (*(cp2 + 1) >= '1' && (*cp2 + 1) <= '9')
	    {
	      if (*cp1 != *(++cp2 + 1))	/* Break at delimiter.  */
		{
		  toks[toknum = *cp2 - '1']++;
		  tp[toknum] = cp1;
		  while (*++cp1 && *(cp2 + 1) != *cp1)
		    ;
		  te[toknum] = cp1;
		}
	      cp2++;
	      break;
	    }
	  /* Fall through, as '$' must be used verbatim.  */
	default:
	  if (*cp2 != *cp1)
	    {
	      match = 0;
	    }
	  break;
	}
      if (match && *cp1)
	{
	  cp1++;
	}
      if (match && *cp2)
	{
	  cp2++;
	}
    }
  if (!match && *cp1)		/* last token mismatch */
    {
      toks[toknum] = 0;
    }

  /* Back substitute tokens into output template
   * string `mapout'.  All fixed characters were
   * already accounted for in presetting BUF_LEN.
   */
  cp1 = buf;
  *cp1 = '\0';
  cp2 = mapout;
  while (*cp2)
    {
      match = 0;
      switch (*cp2)
	{
	case '\\':
	  if (*(cp2 + 1))
	    {
	      *cp1++ = *++cp2;
	    }
	  break;
	case '[':
	LOOP:
	  if (*++cp2 == '$' && isdigit (*(cp2 + 1)))
	    {
	      if (cp_subst (&cp2, &cp1, toks, tp, te, name, &buf, &buf_len))
		match = 1;
	    }
	  else
	    {
	      while (*cp2 && *cp2 != ',' && *cp2 != ']')
		{
		  if (*cp2 == '\\')
		    {
		      cp2++;
		    }
		  else if (*cp2 == '$' && isdigit (*(cp2 + 1)))
                    {
                      if (cp_subst (&cp2,
                                    &cp1, toks, tp, te, name, &buf, &buf_len))
                        match = 1;
                    }
                  else if (*cp2)
		    *cp1++ = *cp2++;
		}
	      if (!*cp2)
		{
		  printf ("nmap: unbalanced brackets\n");
		  return (name);
		}
	      match = 1;
	      cp2--;
	    }
	  if (match)
	    {
	      /* Skip over all alternate text.  */
	      while (*++cp2 && *cp2 != ']')
		{
		  if (*cp2 == '\\' && *(cp2 + 1))
		    {
		      cp2++;
		    }
		}
	      if (!*cp2)
		{
		  printf ("nmap: unbalanced brackets\n");
		  return (name);
		}
	      break;
	    }
	  switch (*++cp2)
	    {
	    case ',':
	      goto LOOP;
	    case ']':
	      break;
	    default:
	      cp2--;
	      goto LOOP;
	    }
	  break;
	case '$':
	  if (isdigit (*(cp2 + 1)))
	    {
	      if (cp_subst (&cp2, &cp1, toks, tp, te, name, &buf, &buf_len))
		match = 1;
	      break;
	    }
	  /* intentional fall through */
	default:
	  *cp1++ = *cp2;
	  break;
	}
      cp2++;
    }
  *cp1 = '\0';

  if (!*buf)
    strcpy (buf, name);

  return buf;
}

void
setsunique (int argc _GL_UNUSED_PARAMETER, char **argv _GL_UNUSED_PARAMETER)
{

  sunique = !sunique;
  printf ("Store unique %s.\n", onoff (sunique));
  code = sunique;
}

void
setrunique (int argc _GL_UNUSED_PARAMETER, char **argv _GL_UNUSED_PARAMETER)
{

  runique = !runique;
  printf ("Receive unique %s.\n", onoff (runique));
  code = runique;
}

/* change directory to parent directory */
void
cdup (int argc _GL_UNUSED_PARAMETER, char **argv _GL_UNUSED_PARAMETER)
{

  if (command ("CDUP") == ERROR && code == 500)
    {
      if (verbose)
	printf ("CDUP command not recognized, trying XCUP\n");
      command ("XCUP");
    }
}

/* restart transfer at specific point */
void
restart (int argc, char **argv)
{

  if (argc != 2)
    printf ("restart: offset not specified\n");
  else
    {
      restart_point = atoll (argv[1]);
      printf ("restarting at %jd. %s\n", (intmax_t) restart_point,
	      "execute get, put or append to initiate transfer");
    }
}

/* show remote system type */
void
syst (int argc _GL_UNUSED_PARAMETER, char **argv _GL_UNUSED_PARAMETER)
{

  command ("SYST");
}

void
macdef (int argc, char **argv)
{
  char *tmp;
  int c;

  if (macnum == 16)
    {
      printf ("Limit of 16 macros have already been defined\n");
      code = -1;
      return;
    }
  if (argc < 2 && !another (&argc, &argv, "macro name"))
    {
      printf ("Usage: %s macro_name\n", argv[0]);
      code = -1;
      return;
    }
  if (interactive)
    {
      printf ("Enter macro line by line, terminating it with a null line\n");
    }
  strncpy (macros[macnum].mac_name, argv[1],
	   sizeof (macros[macnum].mac_name) - 1);
  if (macnum == 0)
    {
      macros[macnum].mac_start = macbuf;
    }
  else
    {
      macros[macnum].mac_start = macros[macnum - 1].mac_end + 1;
    }
  tmp = macros[macnum].mac_start;
  while (tmp < macbuf + sizeof (macbuf))
    {
      if ((c = getchar ()) == EOF)
	{
	  printf ("macdef:end of file encountered\n");
	  code = -1;
	  return;
	}
      if ((*tmp = c) == '\n')
	{
	  if (tmp == macros[macnum].mac_start)
	    {
	      macros[macnum++].mac_end = tmp;
	      code = 0;
	      return;
	    }
	  if (*(tmp - 1) == '\0')
	    {
	      macros[macnum++].mac_end = tmp - 1;
	      code = 0;
	      return;
	    }
	  *tmp = '\0';
	}
      tmp++;
    }
  while (1)
    {
      while ((c = getchar ()) != '\n' && c != EOF)
	/* LOOP */ ;
      if (c == EOF || getchar () == '\n')
	{
	  printf ("Macro not defined - 4k buffer exceeded\n");
	  code = -1;
	  return;
	}
    }
}

/*
 * get size of file on remote machine
 */
void
sizecmd (int argc, char **argv)
{

  if (argc < 2 && !another (&argc, &argv, "filename"))
    {
      printf ("usage: %s filename\n", argv[0]);
      code = -1;
      return;
    }
  command ("SIZE %s", argv[1]);
}

/*
 * get last modification time of file on remote machine
 */
void
modtime (int argc, char **argv)
{
  int overbose;

  if (argc < 2 && !another (&argc, &argv, "filename"))
    {
      printf ("usage: %s filename\n", argv[0]);
      code = -1;
      return;
    }
  overbose = verbose;
  if (debug == 0)
    verbose = -1;
  if (command ("MDTM %s", argv[1]) == COMPLETE)
    {
      int yy, mo, day, hour, min, sec;
      sscanf (reply_string, "%*s %04d%02d%02d%02d%02d%02d", &yy, &mo,
	      &day, &hour, &min, &sec);
      /* might want to print this in local time */
      printf ("%s\t%02d/%02d/%04d %02d:%02d:%02d GMT\n", argv[1],
	      mo, day, yy, hour, min, sec);
    }
  else
    printf ("%s\n", reply_string);
  verbose = overbose;
}

/*
 * show status on remote machine
 */
void
rmtstatus (int argc, char **argv)
{

  command (argc > 1 ? "STAT %s" : "STAT", argv[1]);
}

/*
 * get file if modtime is more recent than current file
 */
void
newer (int argc, char **argv)
{

  if (getit (argc, argv, -1, "w"))
    printf ("Local file \"%s\" is newer than remote file \"%s\"\n",
	    argv[2], argv[1]);
}
