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
 * Copyright (c) 1988, 1990, 1993
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

#include <sys/types.h>

#include <stdlib.h>
#ifdef HAVE_LOCALE_H
# include <locale.h>
#endif

#include "ring.h"
#include "defines.h"
#include "externs.h"

#include <progname.h>
#include <error.h>
#include <argp.h>
#include <unused-parameter.h>
#include <libinetutils.h>

#include <arpa/telnet.h>

#ifdef	AUTHENTICATION
# include <libtelnet/auth.h>
#endif
#ifdef	ENCRYPTION
# include <libtelnet/encrypt.h>
#endif

/* These values need to be the same as defined in libtelnet/kerberos5.c */
/* Either define them in both places, or put in some common header file. */
#define OPTS_FORWARD_CREDS           0x00000002
#define OPTS_FORWARDABLE_CREDS       0x00000001

#if 0
# define FORWARD
#endif

/*
 * Initialize variables.
 */
void
tninit (void)
{
  init_terminal ();

  init_network ();

  init_telnet ();

  init_sys ();

#if defined TN3270
  init_3270 ();
#endif
}

int family = 0;
char *user;
#ifdef	FORWARD
extern int forward_flags;
#endif /* FORWARD */

enum {
  OPTION_NOASYNCH = 256,
  OPTION_NOASYNCTTY,
  OPTION_NOASYNCNET
};

#if defined KERBEROS || defined SHISHI
extern char *dest_realm;
#endif

static struct argp_option argp_options[] = {
#define GRID 10
  { NULL, 0, NULL, 0,
    "General options:", GRID },

  { "ipv4", '4', NULL, 0,
    "use only IPv4", GRID+1 },
  { "ipv6", '6', NULL, 0,
    "use only IPv6", GRID+1 },
  /* FIXME: Called "8bit" in r* utils */
  { "binary", '8', NULL, 0,
    "use an 8-bit data transmission", GRID+1 },
  { "login", 'a', NULL, 0,
    "attempt automatic login", GRID+1 },
  { "no-rc", 'c', NULL, 0,
    "do not read the user's .telnetrc file", GRID+1 },
  { "debug", 'd', NULL, 0,
    "turn on debugging", GRID+1 },
  { "escape", 'e', "CHAR", 0,
    "use CHAR as an escape character", GRID+1 },
  { "no-escape", 'E', NULL, 0,
    "use no escape character", GRID+1 },
  { "no-login", 'K', NULL, 0,
    "do not automatically login to the remote system", GRID+1 },
  { "user", 'l', "USER", 0,
    "attempt automatic login as USER", GRID+1 },
  { "binary-output", 'L', NULL, 0, /* FIXME: Why L?? */
    "use an 8-bit data transmission for output only", GRID+1 },
  { "trace", 'n', "FILE", 0,
    "record trace information into FILE", GRID+1 },
  { "rlogin", 'r', NULL, 0,
    "use a user-interface similar to rlogin", GRID+1 },
#undef GRID

#ifdef ENCRYPTION
# define GRID 20
  { NULL, 0, NULL, 0,
    "Encryption control:", GRID },
  { "encrypt", 'x', NULL, 0,
    "encrypt the data stream, if possible", GRID+1 },
# undef GRID
#endif

#ifdef AUTHENTICATION
# define GRID 30
  { NULL, 0, NULL, 0,
    "Authentication and Kerberos options:", GRID },
  { "disable-auth", 'X', "ATYPE", 0,
    "disable type ATYPE authentication", GRID+1 },
# if defined KERBEROS || defined SHISHI
  { "realm", 'k', "REALM", 0,
    "obtain tickets for the remote host in REALM "
    "instead of the remote host's realm", GRID+1 },
# endif
# if defined KRB5 && defined FORWARD
  { "fwd-credentials", 'f', NULL, 0,
    "allow the local credentials to be forwarded", GRID+1 },
  { NULL, 'F', NULL, 0,
    "forward a forwardable copy of the local credentials "
    "to the remote system", GRID+1 },
# endif
# undef GRID
#endif

#if defined TN3270 && (defined unix || defined __unix || defined __unix__)
# define GRID 40
  { NULL, 0, NULL, 0,
    "TN3270 support:", GRID },
  /* FIXME: Do we need it? */
  { "transcom", 't', "ARG", 0, "", GRID+1 },
  { "noasynch", OPTION_NOASYNCH, NULL, 0, "", GRID+1 },
  { "noasynctty", OPTION_NOASYNCTTY, NULL, 0, "", GRID+1 },
  { "noasyncnet", OPTION_NOASYNCNET, NULL, 0, "", GRID+1 },
# undef GRID
#endif /* TN3270 && (unix || __unix || __unix__) */
  { NULL, 0, NULL, 0, NULL, 0 }
};

static error_t
parse_opt (int key, char *arg, struct argp_state *state _GL_UNUSED_PARAMETER)
{
  switch (key)
    {
    case '4':
      family = 4;
      break;

    case '6':
      family = 6;
      break;

    case '8':
      eight = 3;		/* binary output and input */
      break;

    case 'E':
      rlogin = escape = _POSIX_VDISABLE;
      break;

    case 'K':
#ifdef	AUTHENTICATION
      autologin = 0;
#endif
      break;

    case 'L':
      eight |= 2;		/* binary output only */
      break;

#ifdef	AUTHENTICATION
    case 'X':
      auth_disable_name (arg);
      break;
#endif

    case 'a':
      autologin = 1;
      break;

    case 'c':
      skiprc = 1;
      break;

    case 'd':
      debug = 1;
      break;

    case 'e':
      set_escape_char (arg);
      break;

#if defined AUTHENTICATION && defined KRB5 && defined FORWARD
    case 'f':
      if (forward_flags & OPTS_FORWARD_CREDS)
	argp_error (state, "Only one of -f and -F allowed.", prompt);
      forward_flags |= OPTS_FORWARD_CREDS;
      break;

    case 'F':
      if (forward_flags & OPTS_FORWARD_CREDS)
	argp_error (state, "Only one of -f and -F allowed");
      forward_flags |= OPTS_FORWARD_CREDS;
      forward_flags |= OPTS_FORWARDABLE_CREDS;
      break;
#endif

#if defined AUTHENTICATION && \
      ( defined KERBEROS || defined SHISHI )
    case 'k':
      dest_realm = arg;
      break;
#endif

    case 'l':
      autologin = 1;
      user = arg;
      break;

    case 'n':
      SetNetTrace (arg);
      break;

    case 'r':
      rlogin = '~';
      break;

#if defined TN3270 && (defined unix || defined __unix || defined __unix__)
    case 't':
      /* FIXME: Buffer!!! */
      transcom = tline;
      strcpy (transcom, arg);
      break;

    case OPTION_NOASYNCH:
      noasynchtty = noasynchtty = 1;
      break;

    case OPTION_NOASYNCTTY:
      noasynchtty = 1;
      break;

    case OPTION_NOASYNCNET:
      noasynchnet = 1;
      break;
#endif /* TN3270 && (unix || __unix || __unix__) */

#ifdef	ENCRYPTION
    case 'x':
      encrypt_auto (1);
      decrypt_auto (1);
      break;
#endif

    default:
      return ARGP_ERR_UNKNOWN;
    }

  return 0;
}


const char args_doc[] = "[HOST [PORT]]";
const char doc[] = "Login to remote system HOST "
                   "(optionally, on service port PORT)";
static struct argp argp =
  { argp_options, parse_opt, args_doc, doc, NULL, NULL, NULL };



/*
 * main.  Parse arguments, invoke the protocol or command parser.
 */
int
main (int argc, char *argv[])
{
  int index;

  set_program_name (argv[0]);

#ifdef HAVE_SETLOCALE
  setlocale (LC_ALL, "");
#endif

  tninit ();			/* Clear out things */
#if defined CRAY && !defined __STDC__
  _setlist_init ();		/* Work around compiler bug */
#endif

  TerminalSaveState ();

  if ((prompt = strrchr (argv[0], '/')))
    ++prompt;
  else
    prompt = argv[0];

  user = NULL;

  rlogin = (strncmp (prompt, "rlog", 4) == 0) ? '~' : _POSIX_VDISABLE;
  autologin = -1;

  /* Parse command line */
  iu_argp_init ("telnet", default_program_authors);
  argp_parse (&argp, argc, argv, 0, &index, NULL);

  if (autologin == -1)
    autologin = (rlogin == _POSIX_VDISABLE) ? 0 : 1;

  argc -= index;
  argv += index;

  if (argc)
    {
      /* The command line contains at least one argument.
       */
      char *args[8], **argp = args;

      if (argc > 2)
	error (EXIT_FAILURE, 0, "too many arguments");
      *argp++ = prompt;
      if (user)
	{
	  *argp++ = "-l";
	  *argp++ = user;
	}
      if (family == 4)
	*argp++ = "-4";
      else if (family == 6)
	*argp++ = "-6";

      *argp++ = argv[0];	/* host */
      if (argc > 1)
	*argp++ = argv[1];	/* port */
      *argp = 0;

      if (setjmp (toplevel) != 0)
	Exit (0);
      if (tn (argp - args, args) == 1)	/* Returns only on error.  */
	return (0);
      else
	return (1);
      /* NOT REACHED */
    }

  /* Built-in parser loop; sub-commands jump to `toplevel' mark.  */
  setjmp (toplevel);
  for (;;)
    {
#ifdef TN3270
      if (shell_active)
	shell_continue ();
      else
#endif
	command (1, 0, 0);
    }
  /* NOT REACHED */
}
