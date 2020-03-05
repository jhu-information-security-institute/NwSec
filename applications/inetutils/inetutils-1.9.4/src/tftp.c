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
 * Copyright (c) 1983, 1993
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

/* Many bug fixes are from Jim Guyton <guyton@rand-unix> */

#include <config.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/time.h>
#include <time.h>

#include <netinet/in.h>

#include <arpa/tftp.h>
#include <arpa/inet.h>

#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <netdb.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef HAVE_LOCALE_H
# include <locale.h>
#endif
#ifdef HAVE_IDNA_H
# include <idna.h>
#endif

#include <argp.h>
#include <unused-parameter.h>

#include <libinetutils.h>

#include "xalloc.h"
#include "progname.h"
#include "tftpsubs.h"

/* Some systems define PKTSIZE in <arpa/tftp.h>.  */
#ifndef PKTSIZE
#define PKTSIZE    SEGSIZE+4
#endif

char ackbuf[PKTSIZE];
int timeout;
jmp_buf timeoutbuf;

static void nak (int);
static int makerequest (int, const char *, struct tftphdr *, const char *);
static void printstats (const char *, unsigned long);
static void startclock (void);
static void stopclock (void);
static void timer (int);
static void tpacket (const char *, struct tftphdr *, int);

#define TIMEOUT		5	/* secs between rexmt's */

static int rexmtval = TIMEOUT;
static int maxtimeout = 5 * TIMEOUT;

static struct sockaddr_storage peeraddr;	/* filled in by main */
static socklen_t peerlen;
static int f = -1;				/* the opened socket */
static int port; /* Port number in host byte order of the server. */
static int trace;
static int verbose;
static int connected;
static int fromatty;

char mode[32];
char line[200];
int margc;
char *margv[20];
char *prompt = "tftp";
jmp_buf toplevel;
void intr (int signo);

void get (int, char **);
void help (int, char **);
void modecmd (int, char **);
void put (int, char **);
void quit (int, char **);
void setascii (int, char **);
void setbinary (int, char **);
void setpeer (int, char **);
void setrexmt (int, char **);
void settimeout (int, char **);
void settrace (int, char **);
void setverbose (int, char **);
void status (int, char **);

static void command (void);

static void getusage (char *);
static void makeargv (void);
static void putusage (char *);
static void settftpmode (char *);
static in_port_t get_port (struct sockaddr_storage *);
static void set_port (struct sockaddr_storage *, in_port_t);

#define HELPINDENT (sizeof("connect"))

struct cmd
{
  char *name;
  char *help;
  void (*handler) (int, char **);
};

char vhelp[] = "toggle verbose mode";
char thelp[] = "toggle packet tracing";
char chelp[] = "connect to remote tftp";
char qhelp[] = "exit tftp";
char hhelp[] = "print help information";
char shelp[] = "send file";
char rhelp[] = "receive file";
char mhelp[] = "set file transfer mode";
char sthelp[] = "show current status";
char xhelp[] = "set per-packet retransmission timeout";
char ihelp[] = "set total retransmission timeout";
char ashelp[] = "set mode to netascii";
char bnhelp[] = "set mode to octet";

struct cmd cmdtab[] = {
  {"connect", chelp, setpeer},
  {"mode", mhelp, modecmd},
  {"put", shelp, put},
  {"get", rhelp, get},
  {"quit", qhelp, quit},
  {"verbose", vhelp, setverbose},
  {"trace", thelp, settrace},
  {"status", sthelp, status},
  {"binary", bnhelp, setbinary},
  {"ascii", ashelp, setascii},
  {"rexmt", xhelp, setrexmt},
  {"timeout", ihelp, settimeout},
  {"?", hhelp, help},
  {NULL, NULL, NULL}
};

struct cmd *getcmd (register char *name);
char *tail (char *filename);


const char args_doc[] = "[HOST [PORT]]";
const char doc[] = "Trivial file transfer protocol client";

static struct argp_option argp_options[] = {
  {"verbose", 'v', NULL, 0, "verbose output", 1},
  {NULL, 0, NULL, 0, NULL, 0}
};

char *hostport_argv[3] = { "connect" };
int hostport_argc = 1;

static in_port_t
get_port (struct sockaddr_storage *ss)
{
  switch (ss->ss_family)
    {
    case AF_INET6:
      return ntohs (((struct sockaddr_in6 *) ss)->sin6_port);
      break;
    case AF_INET:
    default:
      return ntohs (((struct sockaddr_in *) ss)->sin_port);
      break;
    }
}

static void
set_port (struct sockaddr_storage *ss, in_port_t port)
{
  switch (ss->ss_family)
    {
    case AF_INET6:
      ((struct sockaddr_in6 *) ss)->sin6_port = htons(port);
      break;
    case AF_INET:
    default:
      ((struct sockaddr_in *) ss)->sin_port = htons(port);
      break;
    }
}

void recvfile (int, char *, char *);
void send_file (int, char *, char *);

static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  switch (key)
    {
    case 'v':		/* Verbose.  */
      verbose++;
      break;

    case ARGP_KEY_ARG:
      if (state->arg_num >= 2 || hostport_argc >= 3)
	/* Too many arguments. */
	argp_usage (state);
      hostport_argv[hostport_argc++] = arg;
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }

  return 0;
}

static struct argp argp =
  {argp_options, parse_opt, args_doc, doc, NULL, NULL, NULL};

int
main (int argc, char *argv[])
{
  struct servent *sp;

  set_program_name (argv[0]);
#ifdef HAVE_SETLOCALE
  setlocale (LC_ALL, "");
#endif
  iu_argp_init ("tftp", default_program_authors);
  argp_parse (&argp, argc, argv, 0, NULL, NULL);

  /* Initiate a default port.  */
  sp = getservbyname ("tftp", "udp");
  if (sp == 0)
    port = 69;
  else
    port = ntohs (sp->s_port);

  fromatty = isatty (STDIN_FILENO);

  strcpy (mode, "netascii");
  signal (SIGINT, intr);
  if (hostport_argc > 1)
    {
      if (setjmp (toplevel) != 0)
	exit (EXIT_SUCCESS);
      setpeer (hostport_argc, hostport_argv);
    }
  if (setjmp (toplevel) != 0)
    putchar ('\n');
  command ();
}

char *hostname;

#define RESOLVE_OK            0
#define RESOLVE_FAIL         -1

/* Resolve NAME. Fill in peeraddr, hostname and set connected on success.
   Return value: RESOLVE_OK success
                 RESOLVE_FAIL error
 */
static int
resolve_name (char *name)
{
  int err;
  char *rname;
  struct sockaddr_storage ss;
  struct addrinfo hints, *ai, *aiptr;

#ifdef HAVE_IDN
  err = idna_to_ascii_lz (name, &rname, 0);
  if (err)
    {
      fprintf (stderr, "tftp: %s: %s\n", name, idna_strerror (err));
      return RESOLVE_FAIL;
    }
#else /* !HAVE_IDN */
  rname = name;
#endif

  memset (&hints, 0, sizeof (hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_CANONNAME;
#ifdef AI_IDN
  hints.ai_flags |= AI_IDN;
#endif
#ifdef AI_CANONIDN
  hints.ai_flags |= AI_CANONIDN;
#endif

  err = getaddrinfo (rname, "tftp", &hints, &aiptr);
  if (err)
    {
      fprintf (stderr, "tftp: %s: %s\n", rname, gai_strerror (err));
      return RESOLVE_FAIL;
    }

#ifdef HAVE_IDN
  free (rname);
#endif

  if (f >= 0)
    {
      close (f);
      f = -1;
    }

  for (ai = aiptr; ai; ai = ai->ai_next)
    {
      f = socket (ai->ai_family, ai->ai_socktype, ai->ai_protocol);
      if (f < 0)
	continue;

      memset (&ss, 0, sizeof (ss));
      ss.ss_family = ai->ai_family;
#if HAVE_STRUCT_SOCKADDR_STORAGE_SS_LEN
      ss.ss_len = ai->ai_addrlen;
#endif
      if (bind (f, (struct sockaddr *) &ss, ai->ai_addrlen))
        {
	  close (f);
	  f = -1;
	  continue;
	}

      /* Successfully resolved hostname. */
      peerlen = ai->ai_addrlen;
      memcpy (&peeraddr, ai->ai_addr, ai->ai_addrlen);
      connected = 1;
      free (hostname);
      if (ai->ai_canonname)
	hostname = xstrdup (ai->ai_canonname);
      else
	hostname = xstrdup ("<dummy>");
      break;
    }

  freeaddrinfo (aiptr);

  if (ai == NULL)
    return RESOLVE_FAIL;
  else
    return RESOLVE_OK;
}

/* Prompt for more arguments from the user with PROMPT, putting the results
   into ARGC & ARGV, with an initial argument of ARG0.  Global variables
   LINE, MARGC, and MARGV are changed.  */
static void
get_args (char *arg0, char *prompt, int *argc, char ***argv)
{
  size_t arg0_len = strlen (arg0);

  strcpy (line, arg0);
  strcat (line, " ");

  printf ("%s", prompt);
  if (fgets (line + arg0_len + 1, sizeof line - arg0_len - 1, stdin))
    {
      makeargv ();
      *argc = margc;
      *argv = margv;
    }
  else
    {
      *argv[0] = arg0;
      *argc = 1;		/* Will produce a usage printout.  */
    }
}

void
setpeer (int argc, char *argv[])
{
  if (argc < 2)
    get_args ("Connect", "(to) ", &argc, &argv);

  if (argc < 2 || argc > 3)
    {
      printf ("usage: %s host-name [port]\n", argv[0]);
      return;
    }

  switch (resolve_name (argv[1]))
    {
    case RESOLVE_OK:
      break;

    case RESOLVE_FAIL:
      return;
    }

  if (argc == 3)
    {
      /* Take a user-specified port number.  */
      port = atoi (argv[2]);
      if (port <= 0)
	{
	  printf ("%s: bad port number\n", argv[2]);
	  connected = 0;
	  return;
	}
    }
  else
    {
      /* Use the standard TFTP port.  */
      struct servent *sp;
      sp = getservbyname ("tftp", "udp");
      if (sp == 0)
	error (EXIT_FAILURE, 0, "udp/tftp: unknown service\n");
      port = ntohs (sp->s_port);
    }
  connected = 1;
}

struct modes
{
  char *m_name;
  char *m_mode;
} modes[] =
  {
    {"ascii", "netascii"},
    {"netascii", "netascii"},
    {"binary", "octet"},
    {"image", "octet"},
    {"octet", "octet"},
    {0, 0}
  };

void
modecmd (int argc, char *argv[])
{
  register struct modes *p;
  char *sep;

  if (argc < 2)
    {
      printf ("Using %s mode to transfer files.\n", mode);
      return;
    }
  if (argc == 2)
    {
      for (p = modes; p->m_name; p++)
	if (strcmp (argv[1], p->m_name) == 0)
	  break;
      if (p->m_name)
	{
	  settftpmode (p->m_mode);
	  return;
	}
      printf ("%s: unknown mode\n", argv[1]);
      /* drop through and print usage message */
    }

  printf ("usage: %s [", argv[0]);
  sep = " ";
  for (p = modes; p->m_name; p++)
    {
      printf ("%s%s", sep, p->m_name);
      if (*sep == ' ')
	sep = " | ";
    }
  printf (" ]\n");
  return;
}

void
setbinary (int argc _GL_UNUSED_PARAMETER, char *argv[] _GL_UNUSED_PARAMETER)
{
  settftpmode ("octet");
}

void
setascii (int argc _GL_UNUSED_PARAMETER, char *argv[] _GL_UNUSED_PARAMETER)
{
  settftpmode ("netascii");
}

static void
settftpmode (char *newmode)
{
  strcpy (mode, newmode);
  if (verbose)
    printf ("mode set to %s\n", mode);
}

/*
 * Send file(s).
 */
void
put (int argc, char *argv[])
{
  int fd;
  register int n;
  register char *cp, *targ;

  if (argc < 2)
    get_args ("send", "(file) ", &argc, &argv);

  if (argc < 2)
    {
      putusage (argv[0]);
      return;
    }
  targ = argv[argc - 1];
  if (strchr (argv[argc - 1], ':'))
    {
      char *cp;

      for (n = 1; n < argc - 1; n++)
	if (strchr (argv[n], ':'))
	  {
	    putusage (argv[0]);
	    return;
	  }
      cp = argv[argc - 1];
      /* Is host string escaped using square brackets?  */
      if (cp[0] == '[')
	{ /* Locate far end.  */
	  cp = strchr (cp, ']');
	  if (cp == NULL)
	    return;	/* Unpaired bracket.  Fail!  */

	  /* Erase far end and capture valid host string.  */
	  *cp = 0;
	  targ = cp + 1;	/* Position beyond bracket.  */
	  cp = argv[argc - 1] + 1;
	  /* Verify presence of a colon. Else backup
	   * in order to accept a strange file name.  */
	  if (targ[0] == ':')
	    ++targ;
	  else
	    {
	      targ = argv[argc - 1];
	      cp = NULL;	/* Invalidate host name string.  */
	    }
	}
      else
	{
	  targ = strchr (cp, ':');
	  *targ++ = 0;
	  /* Test whether host string lacks content.
	   * Then the host string will be ignored.  */
	  if (strlen (cp) == 0)
	    cp = NULL;	/* Invalidate host name string.  */
	}
      /* If a remote host was stated, resolve it!  */
      if (cp != NULL && resolve_name (cp) != RESOLVE_OK)
	return;
    }
  if (!connected)
    {
      printf ("No target machine specified.\n");
      return;
    }
  if (argc < 4)
    {
      cp = argc == 2 ? tail (targ) : argv[1];
      fd = open (cp, O_RDONLY);
      if (fd < 0)
	{
	  fprintf (stderr, "tftp: ");
	  perror (cp);
	  return;
	}
      if (verbose)
	printf ("putting %s to %s:%s [%s]\n", cp, hostname, targ, mode);
      set_port (&peeraddr, port);
      send_file (fd, targ, mode);
      return;
    }
  /* this assumes the target is a directory */
  /* on a remote unix system.  hmmmm.  */
  cp = strchr (targ, '\0');
  *cp++ = '/';
  for (n = 1; n < argc - 1; n++)
    {
      strcpy (cp, tail (argv[n]));
      fd = open (argv[n], O_RDONLY);
      if (fd < 0)
	{
	  fprintf (stderr, "tftp: ");
	  perror (argv[n]);
	  continue;
	}
      if (verbose)
	printf ("putting %s to %s:%s [%s]\n", argv[n], hostname, targ, mode);
      set_port (&peeraddr, port);
      send_file (fd, targ, mode);
    }
}

static void
putusage (char *s)
{
  printf ("usage: %s file ... host:target, or\n", s);
  printf ("       %s file ... target (when already connected)\n", s);
}

/*
 * Receive file(s).
 */
void
get (int argc, char *argv[])
{
  int fd;
  register int n;
  register char *cp;
  char *src;

  if (argc < 2)
    get_args ("get", "(files) ", &argc, &argv);

  if (argc < 2)
    {
      getusage (argv[0]);
      return;
    }
  if (!connected)
    {
      for (n = 1; n < argc; n++)
	if (strchr (argv[n], ':') == 0)
	  {
	    getusage (argv[0]);
	    return;
	  }
    }
  for (n = 1; n < argc; n++)
    {
      src = strchr (argv[n], ':');
      if (src == NULL)
	src = argv[n];
      else if (src == argv[n])
	{
	  /* Degenerate case: silently drop initial colon.
	   * No usable host name.  */
	  ++src;
	}
      else
	{ /* Parse the host name string; remove square brackets.  */
	  cp = argv[n];

	  if (cp[0] == '[')
	    {
	      cp = strchr (argv[n], ']');
	      if (cp)
		{
		  /* Calculate host string and sorce file name.  */
		  src = cp + 1;
		  *cp = 0;
		  if (*src == ':')
		    ++src;
		  cp = argv[n] + 1;
		}
	    }
	  else
	    { /* No escaping; break string at first colon.  */
	      *src++ = 0;
	    }
	  if (cp != NULL && resolve_name (cp) != RESOLVE_OK)
	    continue;
	}

      if (argc < 4)
	{
	  cp = argc == 3 ? argv[2] : tail (src);
	  fd = creat (cp, 0644);
	  if (fd < 0)
	    {
	      fprintf (stderr, "tftp: ");
	      perror (cp);
	      return;
	    }
	  if (verbose)
	    printf ("getting from %s:%s to %s [%s]\n",
		    hostname, src, cp, mode);
	  set_port (&peeraddr, port);
	  recvfile (fd, src, mode);
	  break;
	}
      cp = tail (src);		/* new .. jdg */
      fd = creat (cp, 0644);
      if (fd < 0)
	{
	  fprintf (stderr, "tftp: ");
	  perror (cp);
	  continue;
	}
      if (verbose)
	printf ("getting from %s:%s to %s [%s]\n", hostname, src, cp, mode);
      set_port (&peeraddr, port);
      recvfile (fd, src, mode);
    }
}

static void
getusage (char *s)
{
  printf ("usage: %s host:file host:file ... file, or\n", s);
  printf ("       %s file file ... file if connected\n", s);
}

void
setrexmt (int argc, char *argv[])
{
  int t;

  if (argc < 2)
    get_args ("Rexmt-timeout", "(value) ", &argc, &argv);

  if (argc != 2)
    {
      printf ("usage: %s value\n", argv[0]);
      return;
    }
  t = atoi (argv[1]);
  if (t < 0)
    printf ("%s: bad value\n", argv[1]);
  else
    rexmtval = t;
}

void
settimeout (int argc, char *argv[])
{
  int t;

  if (argc < 2)
    get_args ("Maximum-timeout", "(value) ", &argc, &argv);

  if (argc != 2)
    {
      printf ("usage: %s value\n", argv[0]);
      return;
    }
  t = atoi (argv[1]);
  if (t < 0)
    printf ("%s: bad value\n", argv[1]);
  else
    maxtimeout = t;
}

void
status (int argc _GL_UNUSED_PARAMETER, char *argv[] _GL_UNUSED_PARAMETER)
{
  if (connected)
    printf ("Connected to %s.\n", hostname);
  else
    printf ("Not connected.\n");
  printf ("Mode: %s Verbose: %s Tracing: %s\n", mode,
	  verbose ? "on" : "off", trace ? "on" : "off");
  printf ("Rexmt-interval: %d seconds, Max-timeout: %d seconds\n",
	  rexmtval, maxtimeout);
}

void
intr (int signo _GL_UNUSED_PARAMETER)
{
  signal (SIGALRM, SIG_IGN);
  alarm (0);
  longjmp (toplevel, -1);
}

char *
tail (char *filename)
{
  register char *s;

  while (*filename)
    {
      s = strrchr (filename, '/');
      if (s == NULL)
	break;
      if (s[1])
	return (s + 1);
      *s = '\0';
    }
  return filename;
}

/*
 * Command parser.
 */
static void
command (void)
{
  register struct cmd *c;

  for (;;)
    {
      if (fromatty)
	printf ("%s> ", prompt);
      if (fgets (line, sizeof line, stdin) == 0)
	{
	  if (feof (stdin))
	    exit (EXIT_SUCCESS);
	  else
	    continue;
	}
      if (line[0] == 0)
	continue;
      makeargv ();
      if (margc == 0)
	continue;
      c = getcmd (margv[0]);
      if (c == (struct cmd *) -1)
	{
	  printf ("?Ambiguous command\n");
	  continue;
	}
      if (c == 0)
	{
	  printf ("?Invalid command\n");
	  continue;
	}
      (*c->handler) (margc, margv);
    }
}

struct cmd *
getcmd (register char *name)
{
  register char *p, *q;
  register struct cmd *c, *found;
  register int nmatches, longest;

  longest = 0;
  nmatches = 0;
  found = 0;
  for (c = cmdtab; (p = c->name) != NULL; c++)
    {
      for (q = name; *q == *p++; q++)
	if (*q == 0)		/* exact match? */
	  return (c);

      if (!*q)
	{			/* the name was a prefix */
	  if (q - name > longest)
	    {
	      longest = q - name;
	      nmatches = 1;
	      found = c;
	    }
	  else if (q - name == longest)
	    nmatches++;
	}
    }
  if (nmatches > 1)
    return (struct cmd *) -1;
  return found;
}

/*
 * Slice a string up into argc/argv.
 */
static void
makeargv (void)
{
  register char *cp;
  register char **argp = margv;

  margc = 0;
  for (cp = line; *cp;)
    {
      while (isspace (*cp))
	cp++;
      if (*cp == '\0')
	break;
      *argp++ = cp;
      margc += 1;
      while (*cp != '\0' && !isspace (*cp))
	cp++;
      if (*cp == '\0')
	break;
      *cp++ = '\0';
    }
  *argp++ = 0;
}

void
quit (int argc _GL_UNUSED_PARAMETER, char *argv[] _GL_UNUSED_PARAMETER)
{
  exit (EXIT_SUCCESS);
}

/*
 * Help command.
 */
void
help (int argc, char *argv[])
{
  register struct cmd *c;

  if (argc == 1)
    {
      printf ("Commands may be abbreviated.  Commands are:\n\n");
      for (c = cmdtab; c->name; c++)
	printf ("%-*s\t%s\n", (int) HELPINDENT, c->name, c->help);
      return;
    }

  while (--argc > 0)
    {
      register char *arg;

      arg = *++argv;
      c = getcmd (arg);
      if (c == (struct cmd *) -1)
	printf ("?Ambiguous help command %s\n", arg);
      else if (c == (struct cmd *) 0)
	printf ("?Invalid help command %s\n", arg);
      else
	printf ("%s\n", c->help);
    }
}

void
settrace (int argc _GL_UNUSED_PARAMETER, char *argv[] _GL_UNUSED_PARAMETER)
{
  trace = !trace;
  printf ("Packet tracing %s.\n", trace ? "on" : "off");
}

void
setverbose (int argc _GL_UNUSED_PARAMETER, char *argv[] _GL_UNUSED_PARAMETER)
{
  verbose = !verbose;
  printf ("Verbose mode %s.\n", verbose ? "on" : "off");
}

/*
 * Send the requested file.
 */
void
send_file (int fd, char *name, char *mode)
{
  register struct tftphdr *ap;	/* data and ack packets */
  struct tftphdr *r_init (void), *dp;
  register int n;
  volatile int block, size, convert;
  volatile unsigned long amount;
  struct sockaddr_storage from;
  socklen_t fromlen;
  FILE *file;

  startclock ();		/* start stat's clock */
  dp = r_init ();		/* reset fillbuf/read-ahead code */
  ap = (struct tftphdr *) ackbuf;
  file = fdopen (fd, "r");
  convert = !strcmp (mode, "netascii");
  block = 0;
  amount = 0;

  signal (SIGALRM, timer);
  do
    {
      if (block == 0)
	size = makerequest (WRQ, name, dp, mode) - 4;
      else
	{
	  /*      size = read(fd, dp->th_data, SEGSIZE);   */
	  size = readit (file, &dp, convert);
	  if (size < 0)
	    {
	      nak (errno + 100);
	      break;
	    }
	  dp->th_opcode = htons ((unsigned short) DATA);
	  dp->th_block = htons ((unsigned short) block);
	}
      timeout = 0;
      setjmp (timeoutbuf);

    send_data:
      if (trace)
	tpacket ("sent", dp, size + 4);
      n = sendto (f, (const char *) dp, size + 4, 0,
		  (struct sockaddr *) &peeraddr, peerlen);
      if (n != size + 4)
	{
	  perror ("tftp: sendto");
	  goto abort;
	}
      read_ahead (file, convert);

      for (;;)
	{
	  alarm (rexmtval);
	  do
	    {
	      fromlen = sizeof (from);
	      n = recvfrom (f, ackbuf, sizeof (ackbuf), 0,
			    (struct sockaddr *) &from, &fromlen);
	    }
	  while (n <= 0);
	  alarm (0);
	  if (n < 0)
	    {
	      perror ("tftp: recvfrom");
	      goto abort;
	    }
	  set_port (&peeraddr, get_port (&from));
	  if (trace)
	    tpacket ("received", ap, n);
	  /* should verify packet came from server */
	  ap->th_opcode = ntohs (ap->th_opcode);
	  ap->th_block = ntohs (ap->th_block);
	  if (ap->th_opcode == ERROR)
	    {
	      printf ("Error code %d: %s\n", ap->th_code, ap->th_msg);
	      goto abort;
	    }
	  if (ap->th_opcode == ACK)
	    {
	      int j;

	      if (ap->th_block == block)
		break;

	      /* On an error, try to synchronize
	       * both sides.
	       */
	      j = synchnet (f);
	      if (j && trace)
		printf ("discarded %d packets\n", j);

	      if (ap->th_block == (block - 1))
		goto send_data;
	    }
	}
      if (block > 0)
	amount += size;
      block++;
    }
  while (size == SEGSIZE || block == 1);

abort:
  fclose (file);
  stopclock ();
  if (amount > 0)
    printstats ("Sent", amount);
}

/*
 * Receive a file.
 */
void
recvfile (int fd, char *name, char *mode)
{
  register struct tftphdr *ap;
  struct tftphdr *dp, *w_init (void);
  register int n;
  volatile int block, size, firsttrip;
  volatile unsigned long amount;
  struct sockaddr_storage from;
  socklen_t fromlen;
  FILE *file;
  volatile int convert;		/* true if converting crlf -> lf */

  startclock ();
  dp = w_init ();
  ap = (struct tftphdr *) ackbuf;
  file = fdopen (fd, "w");
  convert = !strcmp (mode, "netascii");
  block = 1;
  firsttrip = 1;
  amount = 0;

  signal (SIGALRM, timer);
  do
    {
      if (firsttrip)
	{
	  size = makerequest (RRQ, name, ap, mode);
	  firsttrip = 0;
	}
      else
	{
	  ap->th_opcode = htons ((unsigned short) ACK);
	  ap->th_block = htons ((unsigned short) (block));
	  size = 4;
	  block++;
	}
      timeout = 0;
      setjmp (timeoutbuf);

    send_ack:
      if (trace)
	tpacket ("sent", ap, size);
      if (sendto (f, ackbuf, size, 0, (struct sockaddr *) &peeraddr,
		  peerlen) != size)
	{
	  alarm (0);
	  perror ("tftp: sendto");
	  goto abort;
	}
      write_behind (file, convert);

      for (;;)
	{
	  alarm (rexmtval);
	  do
	    {
	      fromlen = sizeof (from);
	      n = recvfrom (f, (char *) dp, PKTSIZE, 0,
			    (struct sockaddr *) &from, &fromlen);
	    }
	  while (n <= 0);

	  alarm (0);
	  if (n < 0)
	    {
	      perror ("tftp: recvfrom");
	      goto abort;
	    }
	  set_port (&peeraddr, get_port (&from));
	  if (trace)
	    tpacket ("received", dp, n);
	  /* should verify client address */
	  dp->th_opcode = ntohs (dp->th_opcode);
	  dp->th_block = ntohs (dp->th_block);
	  if (dp->th_opcode == ERROR)
	    {
	      printf ("Error code %d: %s\n", dp->th_code, dp->th_msg);
	      goto abort;
	    }
	  if (dp->th_opcode == DATA)
	    {
	      int j;

	      if (dp->th_block == block)
		break;		/* have next packet */

	      /* On an error, try to synchronize
	       * both sides.
	       */
	      j = synchnet (f);
	      if (j && trace)
		printf ("discarded %d packets\n", j);

	      if (dp->th_block == (block - 1))
		goto send_ack;	/* resend ack */
	    }
	}
      /*      size = write(fd, dp->th_data, n - 4); */
      size = writeit (file, &dp, n - 4, convert);
      if (size < 0)
	{
	  nak (errno + 100);
	  break;
	}
      amount += size;
    }
  while (size == SEGSIZE);

abort:				/* ok to ack, since user */
  ap->th_opcode = htons ((unsigned short) ACK);	/* has seen err msg */
  ap->th_block = htons ((unsigned short) block);
  sendto (f, ackbuf, 4, 0, (struct sockaddr *) &peeraddr, peerlen);
  write_behind (file, convert);	/* flush last buffer */
  fclose (file);
  stopclock ();
  if (amount > 0)
    printstats ("Received", amount);
}

static int
makerequest (int request, const char *name, struct tftphdr *tp,
	     const char *mode)
{
  register char *cp;
  size_t arglen, len;

  tp->th_opcode = htons ((unsigned short) request);
#if HAVE_STRUCT_TFTPHDR_TH_U
  /*
   * GNU and BSD essentially, i.e. modulo a macro, define
   * 'tftphdr.th_stuff' as a character array of length
   * naught with GNU, and one with BSD!
   *
   * When compiling with stack protectors, like during
   * hardened builds in Debian, every useful file name
   * will overflow that limit.  However, our code ensures
   * '*tp' to be of length PKTSIZE.  Assigning CP via an
   * offset calculation avoids this issue.
   */
  cp = (char *) tp + (tp->th_stuff - (char *) tp);
#else
  cp = (char *) &(tp->th_stuff);
#endif

  /* Available space for naming the target file.  */
  len = PKTSIZE - sizeof (struct tftphdr) - sizeof ("netascii");
  arglen = strlen (name);

  strncpy (cp, name, len);

  cp += (arglen < len) ? arglen : len;
  *cp++ = '\0';

  /* Mode is either "netascii" or "octet", so is always fits
   * based on our choice of LEN.  */
  strcpy (cp, mode);
  cp += strlen (mode);
  *cp++ = '\0';
  return cp - (char *) tp;
}

struct errmsg
{
  int e_code;
  const char *e_msg;
} errmsgs[] =
  {
    {EUNDEF, "Undefined error code"},
    {ENOTFOUND, "File not found"},
    {EACCESS, "Access violation"},
    {ENOSPACE, "Disk full or allocation exceeded"},
    {EBADOP, "Illegal TFTP operation"},
    {EBADID, "Unknown transfer ID"},
    {EEXISTS, "File already exists"},
    {ENOUSER, "No such user"},
    {-1, 0}
  };

/*
 * Send a nak packet (error message).
 * Error code passed in is one of the
 * standard TFTP codes, or a UNIX errno
 * offset by 100.
 */
static void
nak (int error)
{
  register struct errmsg *pe;
  register struct tftphdr *tp;
  int length;

  tp = (struct tftphdr *) ackbuf;
  tp->th_opcode = htons ((unsigned short) ERROR);
  tp->th_code = htons ((unsigned short) error);
  for (pe = errmsgs; pe->e_code >= 0; pe++)
    if (pe->e_code == error)
      break;

  if (pe->e_code < 0)
    {
      pe->e_msg = strerror (error - 100);
      tp->th_code = EUNDEF;
    }
  strcpy (tp->th_msg, pe->e_msg);
  length = strlen (pe->e_msg) + 4;
  if (trace)
    tpacket ("sent", tp, length);
  if (sendto (f, ackbuf, length, 0, (struct sockaddr *) &peeraddr,
	      peerlen) != length)
    perror ("nak");
}

static void
tpacket (const char *s, struct tftphdr *tp, int n)
{
  static char *opcodes[] = { "#0", "RRQ", "WRQ", "DATA", "ACK", "ERROR" };
  register char *cp, *file;
  unsigned short op = ntohs (tp->th_opcode);

  if (op < RRQ || op > ERROR)
    printf ("%s opcode=%x ", s, op);
  else
    printf ("%s %s ", s, opcodes[op]);
  switch (op)
    {
    case RRQ:
    case WRQ:
      n -= 2;
#if HAVE_STRUCT_TFTPHDR_TH_U
      file = cp = tp->th_stuff;
#else
      file = cp = (char *) &(tp->th_stuff);
#endif
      cp = strchr (cp, '\0');
      printf ("<file=%s, mode=%s>\n", file, cp + 1);
      break;

    case DATA:
      printf ("<block=%d, %d bytes>\n", ntohs (tp->th_block), n - 4);
      break;

    case ACK:
      printf ("<block=%d>\n", ntohs (tp->th_block));
      break;

    case ERROR:
      printf ("<code=%d, msg=%s>\n", ntohs (tp->th_code), tp->th_msg);
      break;
    }
}

struct timeval tstart;
struct timeval tstop;

static void
startclock (void)
{
  gettimeofday (&tstart, NULL);
}

static void
stopclock (void)
{
  gettimeofday (&tstop, NULL);
}

static void
printstats (const char *direction, unsigned long amount)
{
  double delta;

  /* compute delta in 1/10's second units */
  delta = ((tstop.tv_sec * 10.) + (tstop.tv_usec / 100000)) -
    ((tstart.tv_sec * 10.) + (tstart.tv_usec / 100000));
  delta = delta / 10.;		/* back to seconds */
  printf ("%s %d bytes in %.1f seconds", direction, (int) amount, delta);
  if (verbose)
    printf (" [%.0f bits/sec]", (amount * 8.) / delta);
  putchar ('\n');
}

static void
timer (int sig _GL_UNUSED_PARAMETER)
{
  timeout += rexmtval;
  if (timeout >= maxtimeout)
    {
      printf ("Transfer timed out.\n");
      longjmp (toplevel, -1);
    }
  longjmp (timeoutbuf, 1);
}
