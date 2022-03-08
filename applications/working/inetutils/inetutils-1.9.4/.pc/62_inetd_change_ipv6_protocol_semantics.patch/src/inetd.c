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
 * Copyright (c) 1983, 1991, 1993, 1994
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
 * Inetd - Internet super-server
 *
 * This program invokes all internet services as needed.  Connection-oriented
 * services are invoked each time a connection is made, by creating a process.
 * This process is passed the connection as file descriptor 0 and is expected
 * to do a getpeername to find out the source host and port.
 *
 * Datagram oriented services are invoked when a datagram
 * arrives; a process is created and passed a pending message
 * on file descriptor 0.  Datagram servers may either connect
 * to their peer, freeing up the original socket for inetd
 * to receive further messages on, or ``take over the socket'',
 * processing all arriving datagrams and, eventually, timing
 * out.	 The first type of server is said to be ``multi-threaded'';
 * the second type of server ``single-threaded''.
 *
 * Inetd uses a configuration file which is read at startup
 * and, possibly, at some later time in response to a hangup signal.
 * The configuration file is ``free format'' with fields given in the
 * order shown below.  Continuation lines for an entry must being with
 * a space or tab.  All fields must be present in each entry.
 *
 *	service name			must be in /etc/services or must
 *					name a tcpmux service
 *	socket type			stream/dgram/raw/rdm/seqpacket
 *	protocol			must be in /etc/protocols
 *	wait/nowait[.max]		single-threaded/multi-threaded
 *                                      [with an optional fork limit]
 *	user[:group] or user[.group]	user (and group) to run daemon as
 *	server program			full path name
 *	server program arguments	arguments starting with argv[0]
 *
 * TCP services without official port numbers are handled with the
 * RFC1078-based tcpmux internal service. Tcpmux listens on port 1 for
 * requests. When a connection is made from a foreign host, the service
 * requested is passed to tcpmux, which looks it up in the servtab list
 * and returns the proper entry for the service. Tcpmux returns a
 * negative reply if the service doesn't exist, otherwise the invoked
 * server is expected to return the positive reply if the service type in
 * inetd.conf file has the prefix "tcpmux/". If the service type has the
 * prefix "tcpmux/+", tcpmux will return the positive reply for the
 * process; this is for compatibility with older server code, and also
 * allows you to invoke programs that use stdin/stdout without putting any
 * special server code in them. Services that use tcpmux are "nowait"
 * because they do not have a well-known port and hence cannot listen
 * for new requests.
 *
 * Comment lines are indicated by a `#' in column 1.
 */

#include <config.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <time.h>
#ifdef HAVE_SYS_RESOURCE_H
# include <sys/resource.h>
#endif

#include <netinet/in.h>
#include <arpa/inet.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <argp.h>
#include <argp-version-etc.h>
#include <progname.h>
#include <sys/select.h>
#include <grp.h>

#include "libinetutils.h"
#include "argcv.h"
#include "version-etc.h"
#include "unused-parameter.h"

#ifndef EAI_ADDRFAMILY
# define EAI_ADDRFAMILY 1
#endif

#define TOOMANY		1000	/* don't start more than TOOMANY */
#define CNT_INTVL	60	/* servers in CNT_INTVL sec. */
#define RETRYTIME	(60*10)	/* retry after bind or server fail */

#ifndef SIGCHLD
# define SIGCHLD	SIGCLD
#endif
#define SIGBLOCK	(sigmask(SIGCHLD)|sigmask(SIGHUP)|sigmask(SIGALRM))

bool debug = false;
int nsock, maxsock;
fd_set allsock;
int options;
int timingout;
unsigned toomany = TOOMANY;
char **Argv;
char *LastArg;

char **config_files;

static bool env_option = false;	       /* Set environment variables */
static bool resolve_option = false;    /* Resolve IP addresses */
static bool pidfile_option = true;     /* Record the PID in a file */
static const char *pid_file = PATH_INETDPID;

const char args_doc[] = "[CONF-FILE [CONF-DIR]]...";
const char doc[] = "Internet super-server.";

/* Define keys for long options that do not have short counterparts. */
enum {
  OPT_ENVIRON = 256,
  OPT_RESOLVE
};

const char *program_authors[] = {
  "Alain Magloire", "Alfred M. Szmidt", "Debarshi Ray",
  "Jakob 'sparky' Kaivo", "Jeff Bailey",
  "Jeroen Dekkers", "Marcus Brinkmann", "Sergey Poznyakoff",
  "others", NULL };

static struct argp_option argp_options[] = {
#define GRP 0
  {"debug", 'd', NULL, 0,
   "turn on debugging, run in foreground mode", GRP+1},
  {"environment", OPT_ENVIRON, NULL, 0,
   "pass local and remote socket information in environment variables", GRP+1},
  { "pidfile", 'p', "PIDFILE", OPTION_ARG_OPTIONAL,
    "override pidfile (default: \"" PATH_INETDPID "\")",
    GRP+1 },
  {"rate", 'R', "NUMBER", 0,
   "maximum invocation rate (per minute)", GRP+1},
  {"resolve", OPT_RESOLVE, NULL, 0,
   "resolve IP addresses when setting environment variables "
   "(see --environment)", GRP+1},
#undef GRP
  {NULL, 0, NULL, 0, NULL, 0}
};

static error_t
parse_opt (int key, char *arg,
	   struct argp_state *state _GL_UNUSED_PARAMETER)
{
  char *p;
  int number;

  switch (key)
    {
    case 'd':
      debug = true;
      options |= SO_DEBUG;
      break;

    case OPT_ENVIRON:
      env_option = true;
      break;

    case 'p':
      if (arg && strlen (arg))
	pid_file = arg;
      else
	pidfile_option = false;
      break;

    case 'R':
      number = strtol (arg, &p, 0);
      if (number < 1 || *p)
        syslog (LOG_ERR, "-R %s: bad value for service invocation rate", arg);
      else
        toomany = number;
      break;

    case OPT_RESOLVE:
      resolve_option = true;
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }

  return 0;
}

static struct argp argp =
  {argp_options, parse_opt, args_doc, doc, NULL, NULL, NULL};


struct servtab
{
  const char *se_file;
  int se_line;
  char *se_node;                /* node name */
  char *se_service;		/* name of service */
  int se_socktype;		/* type of socket to use */
  char *se_proto;		/* protocol used */
  pid_t se_wait;		/* single threaded server */
  unsigned se_max;              /* Maximum number of instances per CNT_INTVL */
  short se_checked;		/* looked at during merge */
  char *se_user;		/* user name to run as */
  char *se_group;		/* group name to run as */
  struct biltin *se_bi;		/* if built-in, description */
  char *se_server;		/* server program */
  char **se_argv;		/* program arguments */
  size_t se_argc;		/* number of arguments */
  int se_fd;			/* open descriptor */
  int se_type;			/* type */
  sa_family_t se_family;	/* address family of the socket */
  char se_v4mapped;		/* 1 = accept v4mapped connection, 0 = don't */
  struct sockaddr_storage se_ctrladdr;	/* bound address */
  socklen_t se_addrlen;		/* exact address length in use */
  unsigned se_refcnt;
  unsigned se_count;			/* number started since se_time */
  struct timeval se_time;	/* start of se_count */
  struct servtab *se_next;
} *servtab;

#define NORM_TYPE	0
#define MUX_TYPE	1
#define MUXPLUS_TYPE	2
#define ISMUX(sep)	(((sep)->se_type == MUX_TYPE) ||	\
			 ((sep)->se_type == MUXPLUS_TYPE))
#define ISMUXPLUS(sep)	((sep)->se_type == MUXPLUS_TYPE)


/* Built-in services */
void chargen_dg (int, struct servtab *);
void chargen_stream (int, struct servtab *);
void daytime_dg (int, struct servtab *);
void daytime_stream (int, struct servtab *);
void discard_dg (int, struct servtab *);
void discard_stream (int, struct servtab *);
void echo_dg (int, struct servtab *);
void echo_stream (int, struct servtab *);
void machtime_dg (int, struct servtab *);
void machtime_stream (int, struct servtab *);
void tcpmux (int s, struct servtab *sep);

struct biltin
{
  const char *bi_service;	/* internally provided service name */
  int bi_socktype;		/* type of socket supported */
  short bi_fork;		/* 1 if should fork before call */
  short bi_wait;		/* 1 if should wait for child */
  void (*bi_fn) (int s, struct servtab *);	/*function which performs it */
} biltins[] =
  {
    /* Echo received data */
    {"echo", SOCK_STREAM, 1, 0, echo_stream},
    {"echo", SOCK_DGRAM, 0, 0, echo_dg},
    /* Internet /dev/null */
    {"discard", SOCK_STREAM, 1, 0, discard_stream},
    {"discard", SOCK_DGRAM, 0, 0, discard_dg},
    /* Return 32 bit time since 1900 */
    {"time", SOCK_STREAM, 0, 0, machtime_stream},
    {"time", SOCK_DGRAM, 0, 0, machtime_dg},
    /* Return human-readable time */
    {"daytime", SOCK_STREAM, 0, 0, daytime_stream},
    {"daytime", SOCK_DGRAM, 0, 0, daytime_dg},
    /* Familiar character generator */
    {"chargen", SOCK_STREAM, 1, 0, chargen_stream},
    {"chargen", SOCK_DGRAM, 0, 0, chargen_dg},
    {"tcpmux", SOCK_STREAM, 1, 0, tcpmux},
    {NULL, 0, 0, 0, NULL}
  };

#define NUMINT	(sizeof(intab) / sizeof(struct inent))

struct biltin *
bi_lookup (const struct servtab *sep)
{
  struct biltin *bi;

  for (bi = biltins; bi->bi_service; bi++)
    if (bi->bi_socktype == sep->se_socktype
	&& strcmp (bi->bi_service, sep->se_service) == 0)
      return bi;
  return NULL;
}


/* Signal handling */

#if defined HAVE_SIGACTION
# define SIGSTATUS sigset_t
# define sigstatus_empty(s) sigemptyset(&s)
# define inetd_pause(s) sigsuspend(&s)
#else
# define SIGSTATUS long
# define sigstatus_empty(s) s = 0
# define inetd_pause(s) sigpause (s)
#endif

void
signal_set_handler (int signo, void (*handler) ())
{
#if defined HAVE_SIGACTION
  struct sigaction sa;
  memset ((char *) &sa, 0, sizeof (sa));
  sigemptyset (&sa.sa_mask);
  sigaddset (&sa.sa_mask, signo);
# ifdef SA_RESTART
  sa.sa_flags = SA_RESTART;
# endif
  sa.sa_handler = handler;
  sigaction (signo, &sa, NULL);
#elif defined(HAVE_SIGVEC)
  struct sigvec sv;
  memset (&sv, 0, sizeof (sv));
  sv.sv_mask = SIGBLOCK;
  sv.sv_handler = handler;
  sigvec (signo, &sv, NULL);
#else /* !HAVE_SIGVEC */
  signal (signo, handler);
#endif /* HAVE_SIGACTION */
}

void
signal_block (SIGSTATUS * old_status)
{
#ifdef HAVE_SIGACTION
  sigset_t sigs;

  sigemptyset (&sigs);
  sigaddset (&sigs, SIGCHLD);
  sigaddset (&sigs, SIGHUP);
  sigaddset (&sigs, SIGALRM);
  sigprocmask (SIG_BLOCK, &sigs, old_status);
#else
  long omask = sigblock (SIGBLOCK);
  if (old_status)
    *old_status = omask;
#endif
}

void
signal_unblock (SIGSTATUS * status)
{
#ifdef HAVE_SIGACTION
  if (status)
    sigprocmask (SIG_SETMASK, status, 0);
  else
    {
      sigset_t empty;
      sigemptyset (&empty);
      sigprocmask (SIG_SETMASK, &empty, 0);
    }
#else
  sigsetmask (status ? *status : 0);
#endif
}

void
run_service (int ctrl, struct servtab *sep)
{
  struct passwd *pwd;
  struct group *grp = NULL;
  char buf[50];

  if (sep->se_bi)
    {
      (*sep->se_bi->bi_fn) (ctrl, sep);
    }
  else
    {
      if (debug)
	fprintf (stderr, "%d execl %s\n", (int) getpid (), sep->se_server);
      dup2 (ctrl, 0);
      close (ctrl);
      dup2 (0, 1);
      dup2 (0, 2);
      pwd = getpwnam (sep->se_user);
      if (pwd == NULL)
	{
	  syslog (LOG_ERR, "%s/%s: %s: No such user",
		  sep->se_service, sep->se_proto, sep->se_user);
	  if (sep->se_socktype != SOCK_STREAM)
	    recv (0, buf, sizeof buf, 0);
	  _exit (EXIT_FAILURE);
	}
      if (sep->se_group && *sep->se_group)
	{
	  grp = getgrnam (sep->se_group);
	  if (grp == NULL)
	    {
	      syslog (LOG_ERR, "%s/%s: %s: No such group",
		      sep->se_service, sep->se_proto, sep->se_group);
	      if (sep->se_socktype != SOCK_STREAM)
		recv (0, buf, sizeof buf, 0);
	      _exit (EXIT_FAILURE);
	    }
	}
      if (pwd->pw_uid)
	{
	  if (grp && grp->gr_gid)
	    {
	      if (setgid (grp->gr_gid) < 0)
		{
		  syslog (LOG_ERR, "%s: can't set gid %d: %m",
			  sep->se_service, grp->gr_gid);
		  _exit (EXIT_FAILURE);
		}
	    }
	  else if (setgid (pwd->pw_gid) < 0)
	    {
	      syslog (LOG_ERR, "%s: can't set gid %d: %m",
		      sep->se_service, pwd->pw_gid);
	      _exit (EXIT_FAILURE);
	    }
#ifdef HAVE_INITGROUPS
	  initgroups (pwd->pw_name,
		      (grp && grp->gr_gid) ? grp->gr_gid : pwd->pw_gid);
#endif
	  if (setuid (pwd->pw_uid) < 0)
	    {
	      syslog (LOG_ERR, "%s: can't set uid %d: %m",
		      sep->se_service, pwd->pw_uid);
	      _exit (EXIT_FAILURE);
	    }
	}
      execv (sep->se_server, sep->se_argv);
      if (sep->se_socktype != SOCK_STREAM)
	recv (0, buf, sizeof buf, 0);
      syslog (LOG_ERR, "cannot execute %s: %m", sep->se_server);
      _exit (EXIT_FAILURE);
    }
}

void
reapchild (int signo _GL_UNUSED_PARAMETER)
{
  int status;
  pid_t pid;
  struct servtab *sep;

  for (;;)
    {
#ifdef HAVE_WAIT3
      pid = wait3 (&status, WNOHANG, NULL);
#else
      pid = wait (&status);
#endif
      if (pid <= 0)
	break;
      if (debug)
	fprintf (stderr, "%d reaped, status %#x\n", (int) pid, status);
      for (sep = servtab; sep; sep = sep->se_next)
	if (sep->se_wait == pid)
	  {
	    if (status)
	      syslog (LOG_WARNING, "%s: exit status 0x%x",
		      sep->se_server, status);
	    if (debug)
	      fprintf (stderr, "restored %s, fd %d\n",
		       sep->se_service, sep->se_fd);
	    FD_SET (sep->se_fd, &allsock);
	    nsock++;
	    sep->se_wait = 1;
	  }
    }
}



char *
newstr (const char *cp)
{
  char *s;
  s = strdup (cp ? cp : "");
  if (s != NULL)
    return s;
  syslog (LOG_ERR, "strdup: %m");
  exit (-1);
}

void
dupmem (void **pptr, size_t size)
{
  void *ptr = malloc (size);
  if (!ptr)
    {
      syslog (LOG_ERR, "dupmem: %m");
      exit (-1);
    }
  memcpy (ptr, *pptr, size);
  *pptr = ptr;
}

void
dupstr (char **pstr)
{
  if (*pstr)
    dupmem ((void**)pstr, strlen (*pstr) + 1);
}


/*
 * print_service:
 *	Dump relevant information to stderr
 */
void
print_service (const char *action, struct servtab *sep)
{
  fprintf (stderr,
	   "%s:%d: %s: %s:%s proto=%s, wait=%d, max=%u, "
	   "user=%s group=%s builtin=%s server=%s\n",
	   sep->se_file, sep->se_line,
	   action,
	   ISMUX (sep) ? (ISMUXPLUS (sep) ? "tcpmuxplus" : "tcpmux")
		      : (sep->se_node ? sep->se_node : "*"),
	   sep->se_service, sep->se_proto,
	   (int) sep->se_wait, sep->se_max,
	   sep->se_user, sep->se_group,
	   sep->se_bi ? sep->se_bi->bi_service : "no",
	   sep->se_server);
}


/* Configuration */
int
setup (struct servtab *sep)
{
  int err;
  int on = 1;

 tryagain:
  sep->se_fd = socket (sep->se_family, sep->se_socktype, 0);
  if (sep->se_fd < 0)
    {
      /* If we don't support creating AF_INET6 sockets, create AF_INET
	 sockets.  */
      if (errno == EAFNOSUPPORT && sep->se_family == AF_INET6
	  && sep->se_v4mapped)
	{
	  /* Fall back to IPv4 silently.  */
	  sep->se_family = AF_INET;
	  goto tryagain;
	}

      if (debug)
	fprintf (stderr, "socket failed on %s/%s: %s\n",
		 sep->se_service, sep->se_proto, strerror (errno));
      syslog (LOG_ERR, "%s/%s: socket: %m",
	      sep->se_service, sep->se_proto);
      return 1;
    }
#ifdef IPV6
  if (sep->se_family == AF_INET6)
    {
      /* Reverse the value of SEP->se_v4mapped, since otherwise if
	 using `tcp6' as a protocol type, all connections will be
	 mapped to IPv6, and with `tcp6only', IPv4 gets mapped to
	 IPv6.  */
      int val = sep->se_v4mapped ? 0 : 1;
      if (setsockopt (sep->se_fd, IPPROTO_IPV6, IPV6_V6ONLY,
		      (char *) &val, sizeof (val)) < 0)
	syslog (LOG_ERR, "setsockopt (IPV6_V6ONLY): %m");
    }
#endif
  if (strncmp (sep->se_proto, "tcp", 3) == 0 && (options & SO_DEBUG))
    {
      if (setsockopt (sep->se_fd, SOL_SOCKET, SO_DEBUG,
		      (char *) &on, sizeof (on)) < 0
	  && errno != EACCES)	/* Ignore insufficient permission.  */
	syslog (LOG_ERR, "setsockopt (SO_DEBUG): %m");
    }

  err = setsockopt (sep->se_fd, SOL_SOCKET, SO_REUSEADDR,
		    (char *) &on, sizeof (on));
  if (err < 0)
    syslog (LOG_ERR, "setsockopt (SO_REUSEADDR): %m");

  err = bind (sep->se_fd, (struct sockaddr *) &sep->se_ctrladdr,
	      sep->se_addrlen);
  if (err < 0)
    {
      /* If we can't bind with AF_INET6 try again with AF_INET.  */
      if ((errno == EADDRNOTAVAIL || errno == EAFNOSUPPORT)
	  && sep->se_family == AF_INET6 && sep->se_v4mapped)
	{
	  /* Fall back to IPv4 silently.  */
	  sep->se_family = AF_INET;
	  close (sep->se_fd);
	  goto tryagain;
	}
      if (sep->se_node)
	{
	  if (debug)
	    fprintf (stderr, "bind failed for %s %s/%s: %s\n",
		     sep->se_node, sep->se_service, sep->se_proto,
		     strerror (errno));
	  syslog (LOG_ERR,"%s %s/%s: bind: %m",
		  sep->se_node, sep->se_service, sep->se_proto);
	}
      else
	{
	  if (debug)
	    fprintf (stderr, "bind failed for %s/%s: %s\n",
		     sep->se_service, sep->se_proto, strerror (errno));
	  syslog (LOG_ERR,"%s/%s: bind: %m", sep->se_service, sep->se_proto);
	}
      close (sep->se_fd);
      sep->se_fd = -1;
      if (!timingout)
	{
	  timingout = 1;
	  alarm (RETRYTIME);
	}
      return 1;
    }
  return 0;
}

void
servent_setup (struct servtab *sep)
{
  sep->se_checked = 1;
  if (sep->se_fd == -1 && setup (sep) == 0)
    {
      if (sep->se_socktype == SOCK_STREAM)
	listen (sep->se_fd, 10);
      FD_SET (sep->se_fd, &allsock);
      nsock++;
      if (sep->se_fd > maxsock)
	maxsock = sep->se_fd;
      if (debug)
	fprintf (stderr, "registered %s on %d\n", sep->se_server, sep->se_fd);
    }
}

void
retry (int signo _GL_UNUSED_PARAMETER)
{
  struct servtab *sep;

  timingout = 0;
  for (sep = servtab; sep; sep = sep->se_next)
    if (sep->se_fd == -1 && !ISMUX (sep))
      setup (sep);
}

/*
 * Finish with a service and its socket.
 */
void
close_sep (struct servtab *sep)
{
  if (sep->se_fd >= 0)
    {
      nsock--;
      FD_CLR (sep->se_fd, &allsock);
      close (sep->se_fd);
      sep->se_fd = -1;
    }
  sep->se_count = 0;
  /*
   * Don't keep the pid of this running deamon: when reapchild()
   * reaps this pid, it would erroneously increment nsock.
   */
  if (sep->se_wait > 1)
    sep->se_wait = 1;
}

struct servtab *
enter (struct servtab *cp)
{
  struct servtab *sep;
  SIGSTATUS sigstatus;
  size_t i;

  /* Checking/Removing duplicates */
  for (sep = servtab; sep; sep = sep->se_next)
    if (memcmp (&sep->se_ctrladdr, &cp->se_ctrladdr,
		sizeof (sep->se_ctrladdr)) == 0
	&& strcmp (sep->se_service, cp->se_service) == 0
	&& strcmp (sep->se_proto, cp->se_proto) == 0
	&& ISMUX (sep) == ISMUX (cp))
      break;
  if (sep != 0)
    {
      signal_block (&sigstatus);
      /*
       * sep->se_wait may be holding the pid of a daemon
       * that we're waiting for.  If so, don't overwrite
       * it unless the config file explicitly says don't
       * wait.
       */
      if (cp->se_bi == 0 && (sep->se_wait == 1 || cp->se_wait == 0))
	sep->se_wait = cp->se_wait;
#define SWAP(a, b) { char *c = a; a = b; b = c; }
      if (cp->se_user)
	SWAP (sep->se_user, cp->se_user);
      if (cp->se_group)
	SWAP (sep->se_group, cp->se_group);
      if (cp->se_server)
	SWAP (sep->se_server, cp->se_server);
      argcv_free (sep->se_argc, sep->se_argv);
      sep->se_argc = cp->se_argc;
      sep->se_argv = cp->se_argv;
      cp->se_argc = 0;
      cp->se_argv = NULL;
      sep->se_checked = 1;
      signal_unblock (&sigstatus);
      if (debug)
	print_service ("REDO", sep);
      return sep;
    }

  if (debug)
    print_service ("ADD ", cp);

  sep = (struct servtab *) malloc (sizeof (*sep));
  if (sep == NULL)
    {
      syslog (LOG_ERR, "Out of memory.");
      exit (-1);
    }
  *sep = *cp;
  dupstr (&sep->se_node);
  dupstr (&sep->se_service);
  dupstr (&sep->se_proto);
  dupstr (&sep->se_user);
  dupstr (&sep->se_group);
  dupstr (&sep->se_server);
  dupmem ((void**)&sep->se_argv, sep->se_argc * sizeof (sep->se_argv[0]));
  for (i = 0; i < sep->se_argc; i++)
    dupstr (&sep->se_argv[i]);

  sep->se_fd = -1;
  signal_block (&sigstatus);
  sep->se_next = servtab;
  servtab = sep;
  signal_unblock (&sigstatus);
  return sep;
}

#define IPV4_NUMCHARS ".0123456789"
#define IPV6_NUMCHARS ".:0123456789abcdefABCDEF"

int
inetd_getaddrinfo (struct servtab *sep, int proto, struct addrinfo **result)
{
  struct addrinfo hints;
#ifdef IPV6
  bool numeric_address = false;

  /* In case a numerical address is supplied, which does not
     apply to the indicated domain, a non-local resolver
     will wait in vain until time out occurs, thus blocking.
     Avoid this by falling back to numerical host resolving
     when address string seems to be numerical.  */

  /* Purely numeric address?  Separate criteria for IPv4 and IPv6,
     since IPv6 allows hexadecimal coding and IPv4 mapping!  */
  if (sep->se_node
      && (strspn (sep->se_node, IPV4_NUMCHARS) == strlen (sep->se_node)
	  || (strchr (sep->se_node, ':')
	      && strspn (sep->se_node, IPV6_NUMCHARS)) ) )
    numeric_address = true;
  else
    if (debug && sep->se_node)
      fprintf (stderr, "Resolving address: %s\n", sep->se_node);
#endif /* IPV6 */

  memset (&hints, 0, sizeof (hints));

  hints.ai_flags = AI_PASSIVE;
#ifdef AI_V4MAPPED
  if (sep->se_v4mapped && (sep->se_family != AF_INET))
    hints.ai_flags |= AI_V4MAPPED;
#endif
#ifdef IPV6
  if (numeric_address)
    hints.ai_flags |= AI_NUMERICHOST;
#endif
  hints.ai_family = sep->se_family;
  hints.ai_socktype = sep->se_socktype;
  hints.ai_protocol = proto;

  return getaddrinfo (sep->se_node, sep->se_service, &hints, result);
}

int
expand_enter (struct servtab *sep)
{
  int err;
  struct addrinfo *result, *rp;
  struct protoent *proto;
  struct servtab *cp;

  /* Make sure that tcp6 etc also work.  */
  if (strncmp (sep->se_proto, "tcp", 3) == 0)
    proto = getprotobyname ("tcp");
  else if (strncmp (sep->se_proto, "udp", 3) == 0)
    proto = getprotobyname ("udp");
  else
    proto = getprotobyname (sep->se_proto);

  if (!proto)
    {
      syslog (LOG_ERR, "%s: Unknown protocol", sep->se_proto);
      return 1;
    }

  err = inetd_getaddrinfo (sep, proto->p_proto, &result);
#if IPV6
  if (err == EAI_ADDRFAMILY
      && sep->se_family == AF_INET6 && sep->se_v4mapped)
    {
      /* Fall back to IPv4 silently.  */
      sep->se_family = AF_INET;
      err = inetd_getaddrinfo (sep, proto->p_proto, &result);
    }
#endif
  if (err)
    {
      const char *errmsg;

      if (err == EAI_SYSTEM)
	errmsg = strerror (errno);
      else
	errmsg = gai_strerror (err);

      if (sep->se_node)
	{
	  if (debug)
	    fprintf (stderr, "resolution of %s %s/%s failed: %s\n",
		     sep->se_node, sep->se_service, sep->se_proto, errmsg);
	  syslog (LOG_ERR, "%s %s/%s: getaddrinfo: %s",
		  sep->se_node, sep->se_service, sep->se_proto, errmsg);
	}
      else
	{
	  if (debug)
	    fprintf (stderr, "resolution of %s/%s failed: %s\n",
		     sep->se_service, sep->se_proto, errmsg);
	  syslog (LOG_ERR, "%s/%s: getaddrinfo: %s",
		  sep->se_service, sep->se_proto, errmsg);
	}
      return 1;
    }

  for (rp = result; rp != NULL; rp = rp->ai_next)
    {
      memset (&sep->se_ctrladdr, 0, sizeof (sep->se_ctrladdr));
      memcpy (&sep->se_ctrladdr, rp->ai_addr, rp->ai_addrlen);
      sep->se_addrlen = rp->ai_addrlen;
      cp = enter (sep);
      servent_setup (cp);
    }

  freeaddrinfo (result);

  return 0;
}


/* Configuration parser */
char *global_serv_node;
char *serv_node;
size_t serv_node_offset;

char *linebuf = NULL;
size_t linebufsize = 0;

FILE *
setconfig (const char *file)
{
  return fopen (file, "r");
}

void
endconfig (FILE *fconfig)
{
  if (fconfig)
    fclose (fconfig);
}

void
freeconfig (struct servtab *cp)
{
  free (cp->se_node);
  free (cp->se_service);
  free (cp->se_proto);
  free (cp->se_user);
  free (cp->se_group);
  free (cp->se_server);
  argcv_free (cp->se_argc, cp->se_argv);
}

#define INETD_SERVICE      0	/* service name */
#define INETD_SOCKET       1	/* socket type */
#define INETD_PROTOCOL     2	/* protocol */
#define INETD_WAIT         3	/* wait/nowait */
#define INETD_USER         4	/* user */
#define INETD_SERVER_PATH  5	/* server program path */
#define INETD_SERVER_ARGS  6	/* server program arguments */

#define INETD_FIELDS_MIN   6	/* Minimum number of fields in entry */

struct servtab *
next_node_sep (struct servtab *sep)
{
  if (serv_node)
    {
      size_t i = strcspn (serv_node + serv_node_offset, ",");
      sep->se_node = malloc (i + 1);
      if (!sep->se_node)
	{
	  syslog (LOG_ERR, "malloc: %m");
	  exit (-1);
	}
      memcpy (sep->se_node, serv_node + serv_node_offset, i);
      sep->se_node[i] = 0;
      serv_node_offset += i;
      if (serv_node[serv_node_offset])
	serv_node_offset++;
      else
	{
	  free (serv_node);
	  serv_node = NULL;
	}
    }
  return sep;
}

struct servtab *
getconfigent (FILE *fconfig, const char *file, size_t *line)
{
  static struct servtab serv;
  struct servtab *sep = &serv;
  int argc = 0;
  size_t i;
  char **argv = NULL;
  char *node, *service;
  static char TCPMUX_TOKEN[] = "tcpmux/";
#define MUX_LEN		(sizeof(TCPMUX_TOKEN)-1)

  if (serv_node)
    return next_node_sep (sep);

  memset ((caddr_t) sep, 0, sizeof *sep);

  while (1)
    {
      argcv_free (argc, argv);
      freeconfig (sep);
      memset ((caddr_t) sep, 0, sizeof *sep);

      do
	{
	  ssize_t n = getline (&linebuf, &linebufsize, fconfig);
	  if (n < 0)
	    return 0;
	  else if (n == 0)
	    continue;

	  if (linebuf[n-1] == '\n')
	    linebuf[n-1] = 0;
	  ++ *line;
	}
      while (*linebuf == '#' || *linebuf == 0);

      if (argcv_get (linebuf, "", &argc, &argv))
	continue;

      if (argc < INETD_FIELDS_MIN)
	{
	  if (argc == 1 && argv[0][strlen (argv[0]) - 1] == ':')
	    {
	      argv[0][strlen (argv[0]) - 1] = 0;
	      free (global_serv_node);
	      if (strcmp (argv[0], "*"))
		global_serv_node = newstr (argv[0]);
	    }
	  else
	    syslog (LOG_ERR, "%s:%lu: not enough fields",
		    file, (unsigned long) *line);
	  continue;
	}

      sep->se_file = file;
      sep->se_line = *line;

      node = argv[INETD_SERVICE];
      service = strrchr (node, ':');
      if (!service)
        {
	  if (global_serv_node)
	    {
	      node = global_serv_node;
	      serv_node = newstr (node);
	      serv_node_offset = 0;
	    }
	  else
	      node = NULL;

	  service = argv[INETD_SERVICE];
        }
      else
        {
          *service++ = 0;
          if (strcmp (node, "*") == 0)
            node = NULL;
	  else
	    {
	      serv_node = newstr (node);
	      serv_node_offset = 0;
	    }
        }

      if (strncmp (service, TCPMUX_TOKEN, MUX_LEN) == 0)
	{
	  char *c = service + MUX_LEN;
	  if (*c == '+')
	    {
	      sep->se_type = MUXPLUS_TYPE;
	      c++;
	    }
	  else
	    sep->se_type = MUX_TYPE;
	  sep->se_service = newstr (c);
	}
      else
	{
	  sep->se_service = newstr (service);
	  sep->se_type = NORM_TYPE;
	}

      if (strcmp (argv[INETD_SOCKET], "stream") == 0)
	sep->se_socktype = SOCK_STREAM;
      else if (strcmp (argv[INETD_SOCKET], "dgram") == 0)
	sep->se_socktype = SOCK_DGRAM;
      else if (strcmp (argv[INETD_SOCKET], "rdm") == 0)
	sep->se_socktype = SOCK_RDM;
      else if (strcmp (argv[INETD_SOCKET], "seqpacket") == 0)
	sep->se_socktype = SOCK_SEQPACKET;
      else if (strcmp (argv[INETD_SOCKET], "raw") == 0)
	sep->se_socktype = SOCK_RAW;
      else
	{
	  syslog (LOG_WARNING, "%s:%lu: bad socket type",
		  file, (unsigned long) *line);
	  sep->se_socktype = -1;
	}

      sep->se_proto = newstr (argv[INETD_PROTOCOL]);

#ifdef IPV6
      /* We default to IPv4. */
      sep->se_family = AF_INET;
      sep->se_v4mapped = 1;

      if ((strncmp (sep->se_proto, "tcp", 3) == 0)
	  || (strncmp (sep->se_proto, "udp", 3) == 0))
	{
	  if (sep->se_proto[3] == '6')
	    {
	      sep->se_family = AF_INET6;
	      sep->se_v4mapped = 0;
	      /* Check for tcp6only and udp6only.  */
	      if (strcmp (&sep->se_proto[3], "6only") == 0)
	        sep->se_v4mapped = 0;
	    }
	  else if (sep->se_proto[3] == '4')
	    {
	      sep->se_family = AF_INET;
	    }
	}
#else
      if ((strncmp (sep->se_proto, "tcp6", 4) == 0)
	  || (strncmp (sep->se_proto, "udp6", 4) == 0))
	{
	  syslog (LOG_ERR, "%s:%lu: %s: IPv6 support isn't enabled",
		  file, (unsigned long) *line, sep->se_proto);
	  continue;
	}

      sep->se_family = AF_INET;
#endif
      {
	char *p, *q;

	p = strchr(argv[INETD_WAIT], '.');
	if (p)
	  *p++ = 0;
	if (strcmp (argv[INETD_WAIT], "wait") == 0)
	  sep->se_wait = 1;
	else if (strcmp (argv[INETD_WAIT], "nowait") == 0)
	  sep->se_wait = 0;
	else
	  {
	    syslog (LOG_WARNING, "%s:%lu: bad wait type",
		    file, (unsigned long) *line);
	  }
	if (p)
	  {
	    sep->se_max = strtoul(p, &q, 10);
	    if (*q)
	      syslog (LOG_WARNING, "%s:%lu: invalid number (%s)",
		      file, (unsigned long) *line, p);
	  }
      }

      if (ISMUX (sep))
	{
	  /*
	   * Silently enforce "nowait" for TCPMUX services since
	   * they don't have an assigned port to listen on.
	   */
	  sep->se_wait = 0;

	  if (strncmp (sep->se_proto, "tcp", 3))
	    {
	      syslog (LOG_ERR, "%s:%lu: bad protocol for tcpmux service %s",
		      file, (unsigned long) *line, sep->se_service);
	      continue;
	    }
	  if (sep->se_socktype != SOCK_STREAM)
	    {
	      syslog (LOG_ERR,
		      "%s:%lu: bad socket type for tcpmux service %s",
		      file, (unsigned long) *line, sep->se_service);
	      continue;
	    }
	}

      /* Establish optional group identity:
       *   user:group, user.group
       */
      {
	char *p;

	sep->se_user = newstr (argv[INETD_USER]);

	p = strchr (sep->se_user, ':');
	if (!p)
	  p = strchr (sep->se_user, '.');

	if (p)
	  {
	    *p = '\0';
	    sep->se_group = newstr (++p);
	  }
	else
	  sep->se_group = newstr (NULL);
      }

      sep->se_server = newstr (argv[INETD_SERVER_PATH]);
      if (strcmp (sep->se_server, "internal") == 0)
	{
	  sep->se_bi = bi_lookup (sep);
	  if (!sep->se_bi)
	    {
	      syslog (LOG_ERR, "%s:%lu: internal service %s unknown",
		      file, (unsigned long) *line, sep->se_service);
	      continue;
	    }
	  sep->se_wait = sep->se_bi->bi_wait;
	}
      else
	sep->se_bi = NULL;

      sep->se_argc = argc - INETD_FIELDS_MIN + 1;
      sep->se_argv = calloc (sep->se_argc + 1, sizeof sep->se_argv[0]);
      if (!sep->se_argv)
	{
	  syslog (LOG_ERR, "%s:%lu: Out of memory.",
		  file, (unsigned long) *line);
	  exit (-1);
	}

      for (i = 0; i < sep->se_argc; i++)
	{
	  sep->se_argv[i] = argv[INETD_SERVER_ARGS + i];
	  argv[INETD_SERVER_ARGS + i] = 0;
	}

      /* If no arguments are provided, use server name as argv[0].  */
      if (sep->se_argc == 1)
	{
	  const char *argv0 = strrchr (sep->se_server, '/');
	  if (argv0)
	    argv0++;
	  else
	    argv0 = sep->se_server;
          sep->se_argv[0] = newstr (argv0);
	}

      sep->se_argv[i] = NULL;
      break;
    }
  argcv_free (argc, argv);
  return next_node_sep (sep);
}

void
nextconfig (const char *file)
{
#ifndef IPV6
  struct servent *sp;
#endif
  struct servtab *sep, **sepp;
  struct passwd *pwd;
  struct group *grp;
  FILE *fconfig;
  SIGSTATUS sigstatus;

  size_t line = 0;

  fconfig = setconfig (file);
  if (!fconfig)
    {
      syslog (LOG_ERR, "%s: %m", file);
      return;
    }
  while ((sep = getconfigent (fconfig, file, &line)))
    {
      pwd = getpwnam (sep->se_user);
      if (pwd == NULL)
	{
	  syslog (LOG_ERR, "%s/%s: No such user '%s', service ignored",
		  sep->se_service, sep->se_proto, sep->se_user);
	  continue;
	}
      if (sep->se_group && *sep->se_group)
	{
	  grp = getgrnam (sep->se_group);
	  if (grp == NULL)
	    {
	      syslog (LOG_ERR, "%s/%s: No such group '%s', service ignored",
		      sep->se_service, sep->se_proto, sep->se_group);
	      continue;
	    }
	}
      if (ISMUX (sep))
	{
	  sep->se_fd = -1;
	  sep->se_checked = 1;
	  enter (sep);
	}
      else
	expand_enter (sep);

      if (serv_node)
	free (sep->se_node);
      else
	freeconfig (sep);
    }
  endconfig (fconfig);
  /*
   * Purge anything not looked at above.
   */
  signal_block (&sigstatus);
  sepp = &servtab;
  while ((sep = *sepp))
    {
      if (sep->se_checked)
	{
	  sepp = &sep->se_next;
	  continue;
	}
      *sepp = sep->se_next;
      if (sep->se_fd >= 0)
	close_sep (sep);
      if (debug)
	print_service ("FREE", sep);
      freeconfig (sep);
      free (sep);
    }
  signal_unblock (&sigstatus);
}

void
fix_tcpmux (void)
{
  struct servtab *sep;
  int need_tcpmux = 0;
  int has_tcpmux = 0;

  for (sep = servtab; sep; sep = sep->se_next)
    {
      if (sep->se_checked)
	{
	  if (ISMUX (sep))
	    {
	      if (has_tcpmux)
		return;
	      need_tcpmux = 1;
	    }
	  if (strcmp (sep->se_service, "tcpmux") == 0)
	    {
	      if (need_tcpmux)
		return;
	      has_tcpmux = 1;
	    }
	}
    }
  if (need_tcpmux && !has_tcpmux)
    {
      struct servtab serv;
      memset (&serv, 0, sizeof (serv));

      serv.se_file = "fix_tcpmux";
      serv.se_service = newstr ("tcpmux");
      serv.se_socktype = SOCK_STREAM;
      serv.se_checked = 1;
      serv.se_user = newstr ("root");
      serv.se_group = newstr (NULL);	/* Group name for root is not portable.  */
      serv.se_bi = bi_lookup (&serv);
      if (!serv.se_bi)
	{
	  /* Should not happen */
	  freeconfig (&serv);
	  if (debug)
	    fprintf (stderr, "INTERNAL ERROR: could not find tcpmux built-in");
	  syslog (LOG_ERR, "INTERNAL ERROR: could not find tcpmux built-in");
	  return;
	}
      serv.se_wait = serv.se_bi->bi_wait;
      serv.se_server = newstr ("internal");
      serv.se_fd = -1;
      serv.se_type = NORM_TYPE;
#ifdef IPV6
      serv.se_proto = newstr ("tcp6");
      serv.se_family = AF_INET6;
      serv.se_v4mapped = 1;
#else
      serv.se_proto = newstr ("tcp");
      serv.se_family = AF_INET;
#endif
      if (debug)
	fprintf (stderr, "inserting default tcpmux entry\n");
      syslog (LOG_INFO, "inserting default tcpmux entry");
      expand_enter (&serv);
    }
}

void
config (int signo)
{
  int i;
  struct stat stats;
  struct servtab *sep;

  for (sep = servtab; sep; sep = sep->se_next)
    sep->se_checked = 0;

  for (i = 0; config_files[i]; i++)
    {
      struct stat statbuf;

      if (stat (config_files[i], &statbuf) == 0)
	{
	  if (S_ISDIR (statbuf.st_mode))
	    {
	      DIR *dirp = opendir (config_files[i]);

	      if (dirp)
		{
		  struct dirent *dp;

		  while ((dp = readdir (dirp)) != NULL)
		    {
		      char *path = calloc (strlen (config_files[i])
					   + strlen (dp->d_name) + 2, 1);
		      if (path)
			{
			  sprintf (path, "%s/%s", config_files[i],
				   dp->d_name);
			  if (stat (path, &stats) == 0
			      && S_ISREG (stats.st_mode))
			    {
			      nextconfig (path);
			    }
			  free (path);
			}
		    }
		  closedir (dirp);
		}
	    }
	  else if (S_ISREG (statbuf.st_mode))
	    {
	      nextconfig (config_files[i]);
	    }
	}
      else
	{
	  if (signo == 0)
	    fprintf (stderr, "inetd: %s, %s\n",
		     config_files[i], strerror (errno));
	  else
	    syslog (LOG_ERR, "%s: %m", config_files[i]);
	}
    }

  free (linebuf);
  linebuf = NULL;
  linebufsize = 0;

  fix_tcpmux ();
}



void
set_proc_title (char *a, int s)
{
  socklen_t size;
  char *cp;
#ifdef IPV6
  struct sockaddr_storage saddr;
#else
  struct sockaddr_in saddr;
#endif
  char buf[80];

  cp = Argv[0];
  size = sizeof saddr;
  if (getpeername (s, (struct sockaddr *) &saddr, &size) == 0)
    {
#ifdef IPV6
      int err;
      char buf2[80];

      err = getnameinfo ((struct sockaddr *) &saddr, sizeof (saddr), buf2,
			 sizeof (buf2), NULL, 0, NI_NUMERICHOST);
      if (!err)
	snprintf (buf, sizeof buf, "-%s [%s]", a, buf2);
      else
	snprintf (buf, sizeof buf, "-%s", a);
#else
      snprintf (buf, sizeof buf, "-%s [%s]", a, inet_ntoa (saddr.sin_addr));
#endif
    }
  else
    snprintf (buf, sizeof buf, "-%s", a);
  strncpy (cp, buf, LastArg - cp);
  cp += strlen (cp);
  while (cp < LastArg)
    *cp++ = ' ';
}

/*
 * Internet services provided internally by inetd:
 */
#define BUFSIZE	8192

/* Echo service -- echo data back */
void
echo_stream (int s, struct servtab *sep)
{
  char buffer[BUFSIZE];
  int i;

  set_proc_title (sep->se_service, s);
  while ((i = read (s, buffer, sizeof buffer)) > 0
	 && write (s, buffer, i) > 0)
    ;
  exit (EXIT_SUCCESS);
}

/* Echo service -- echo data back */
void
echo_dg (int s, struct servtab *sep _GL_UNUSED_PARAMETER)
{
  char buffer[BUFSIZE];
  int i;
  socklen_t size;
#ifdef IPV6
  struct sockaddr_storage sa;
#else
  struct sockaddr sa;
#endif

  size = sizeof sa;
  i = recvfrom (s, buffer, sizeof buffer, 0, (struct sockaddr *) &sa, &size);
  if (i < 0)
    return;
  sendto (s, buffer, i, 0, (struct sockaddr *) &sa, sizeof sa);
}

/* Discard service -- ignore data */
void
discard_stream (int s, struct servtab *sep)
{
  int ret;
  char buffer[BUFSIZE];

  set_proc_title (sep->se_service, s);
  while (1)
    {
      while ((ret = read (s, buffer, sizeof buffer)) > 0)
	;
      if (ret == 0 || errno != EINTR)
	break;
    }
  exit (EXIT_SUCCESS);
}

void
/* Discard service -- ignore data */
discard_dg (int s, struct servtab *sep _GL_UNUSED_PARAMETER)
{
  char buffer[BUFSIZE];

  read (s, buffer, sizeof buffer);
}

#include <ctype.h>
#define LINESIZ 72
char ring[128];
char *endring;

void
initring (void)
{
  int i;

  endring = ring;

  for (i = 0; i <= 128; ++i)
    if (isprint (i))
      *endring++ = i;
}

/* Character generator */
void
chargen_stream (int s, struct servtab *sep)
{
  int len;
  char *rs, text[LINESIZ + 2];

  set_proc_title (sep->se_service, s);

  if (!endring)
    {
      initring ();
      rs = ring;
    }

  text[LINESIZ] = '\r';
  text[LINESIZ + 1] = '\n';
  for (rs = ring;;)
    {
      len = endring - rs;
      if (len >= LINESIZ)
	memmove (text, rs, LINESIZ);
      else
	{
	  memmove (text, rs, len);
	  memmove (text + len, ring, LINESIZ - len);
	}
      if (++rs == endring)
	rs = ring;
      if (write (s, text, sizeof text) != sizeof text)
	break;
    }
  exit (EXIT_SUCCESS);
}

/* Character generator */
void
chargen_dg (int s, struct servtab *sep _GL_UNUSED_PARAMETER)
{
#ifdef IPV6
  struct sockaddr_storage sa;
#else
  struct sockaddr sa;
#endif
  static char *rs;
  int len;
  socklen_t size;
  char text[LINESIZ + 2];

  if (endring == 0)
    {
      initring ();
      rs = ring;
    }

  size = sizeof sa;
  if (recvfrom (s, text, sizeof text, 0, (struct sockaddr *) &sa, &size) < 0)
    return;

  len = endring - rs;
  if (len >= LINESIZ)
    memmove (text, rs, LINESIZ);
  else
    {
      memmove (text, rs, len);
      memmove (text + len, ring, LINESIZ - len);
    }
  if (++rs == endring)
    rs = ring;
  text[LINESIZ] = '\r';
  text[LINESIZ + 1] = '\n';
  sendto (s, text, sizeof text, 0, (struct sockaddr *) &sa, sizeof sa);
}

/*
 * Return a machine readable date and time, in the form of the
 * number of seconds since midnight, Jan 1, 1900.  Since gettimeofday
 * returns the number of seconds since midnight, Jan 1, 1970,
 * we must add 2208988800 seconds to this figure to make up for
 * some seventy years Bell Labs was asleep.
 */

long
machtime (void)
{
  struct timeval tv;

  if (gettimeofday (&tv, NULL) < 0)
    {
      if (debug)
	fprintf (stderr, "Unable to get time of day\n");
      return 0L;
    }
#define OFFSET ((unsigned long)25567 * 24*60*60)
  return (htonl ((long) (tv.tv_sec + OFFSET)));
#undef OFFSET
}

void
machtime_stream (int s, struct servtab *sep _GL_UNUSED_PARAMETER)
{
  long result;

  result = machtime ();
  write (s, (char *) &result, sizeof result);
}

void
machtime_dg (int s, struct servtab *sep _GL_UNUSED_PARAMETER)
{
  long result;
#ifdef IPV6
  struct sockaddr_storage sa;
#else
  struct sockaddr sa;
#endif
  socklen_t size;

  size = sizeof sa;
  if (recvfrom (s, (char *) &result, sizeof result, 0,
		(struct sockaddr *) &sa, &size) < 0)
    return;
  result = machtime ();
  sendto (s, (char *) &result, sizeof result, 0,
	  (struct sockaddr *) &sa, sizeof sa);
}

void
/* Return human-readable time of day */
daytime_stream (int s, struct servtab *sep _GL_UNUSED_PARAMETER)
{
  char buffer[256];
  time_t lclock;

  lclock = time ((time_t *) 0);

  sprintf (buffer, "%.24s\r\n", ctime (&lclock));
  write (s, buffer, strlen (buffer));
}

/* Return human-readable time of day */
void
daytime_dg (int s, struct servtab *sep _GL_UNUSED_PARAMETER)
{
  char buffer[256];
  time_t lclock;
#ifdef IPV6
  struct sockaddr_storage sa;
#else
  struct sockaddr sa;
#endif
  socklen_t size;

  lclock = time ((time_t *) 0);

  size = sizeof sa;
  if (recvfrom (s, buffer, sizeof buffer, 0, (struct sockaddr *) &sa, &size) <
      0)
    return;
  sprintf (buffer, "%.24s\r\n", ctime (&lclock));
  sendto (s, buffer, strlen (buffer), 0, (struct sockaddr *) &sa, sizeof sa);
}

/*
 *  Based on TCPMUX.C by Mark K. Lottor November 1988
 *  sri-nic::ps:<mkl>tcpmux.c
 */


/* # of characters upto \r,\n or \0 */
static int
fd_getline (int fd, char *buf, int len)
{
  int count = 0, n;

  do
    {
      n = read (fd, buf, len - count);
      if (n == 0)
	return count;
      if (n < 0)
	return -1;
      while (--n >= 0)
	{
	  if (*buf == '\r' || *buf == '\n' || *buf == '\0')
	    return count;
	  count++;
	  buf++;
	}
    }
  while (count < len);
  return count;
}

#define MAX_SERV_LEN	(256+2)	/* 2 bytes for \r\n */

#define strwrite(fd, buf)	write(fd, buf, sizeof(buf)-1)

void
tcpmux (int s, struct servtab *sep)
{
  char service[MAX_SERV_LEN + 1];
  int len;

  /* Get requested service name */
  len = fd_getline (s, service, MAX_SERV_LEN);
  if (len < 0)
    {
      strwrite (s, "-Error reading service name\r\n");
      _exit (EXIT_FAILURE);
    }
  service[len] = '\0';

  if (debug)
    fprintf (stderr, "tcpmux: someone wants %s\n", service);

  /*
   * Help is a required command, and lists available services,
   * one per line.
   */
  if (!strcasecmp (service, "help"))
    {
      for (sep = servtab; sep; sep = sep->se_next)
	{
	  if (!ISMUX (sep))
	    continue;
	  write (s, sep->se_service, strlen (sep->se_service));
	  strwrite (s, "\r\n");
	}
      _exit (EXIT_FAILURE);
    }

  /* Try matching a service in inetd.conf with the request */
  for (sep = servtab; sep; sep = sep->se_next)
    {
      if (ISMUX (sep) && !strcasecmp (service, sep->se_service))
	{
	  if (ISMUXPLUS (sep))
	    {
	      strwrite (s, "+Go\r\n");
	    }
	  run_service (s, sep);
	  return;
	}
    }
  strwrite (s, "-Service not available\r\n");
  exit (EXIT_FAILURE);
}

/* Set TCP environment variables, modelled after djb's ucspi-tcp tools:
   http://cr.yp.to/ucspi-tcp/environment.html
*/
void
prepenv (int ctrl, struct sockaddr *sa_client, socklen_t sa_len)
{
  char str[16];
  /* IP is used both for numeric addresses and for symbolic ones.
   * Being statically allocated, and only for logging purposes,
   * a full MAXPATHLEN is exaggerated, so a compromise is made.  */
  char ip[4 * INET6_ADDRSTRLEN];
  int ret;
#ifdef IPV6
  struct sockaddr_storage sa_server;
#else
  struct sockaddr_in sa_server;
#endif
  socklen_t len = sizeof (sa_server);

  setenv ("PROTO", "TCP", 1);
  unsetenv ("TCPLOCALIP");
  unsetenv ("TCPLOCALHOST");
  unsetenv ("TCPLOCALPORT");
  unsetenv ("TCPREMOTEIP");
  unsetenv ("TCPREMOTEPORT");
  unsetenv ("TCPREMOTEHOST");

  if (getsockname (ctrl, (struct sockaddr *) &sa_server, &len) < 0)
    syslog (LOG_WARNING, "getsockname(): %m");
  else
    {
      ret = getnameinfo ((struct sockaddr *) &sa_server, len,
			  ip, sizeof (ip), str, sizeof (str),
			  NI_NUMERICHOST | NI_NUMERICSERV);
      if (ret == 0)
	{
	  if (setenv ("TCPLOCALIP", ip, 1) < 0)
	    syslog (LOG_WARNING, "setenv (TCPLOCALIP): %m");
	  else if (debug)
	    fprintf (stderr, "Assigned TCPLOCALIP = %s\n", ip);

	  if (setenv ("TCPLOCALPORT", str, 1) < 0)
	    syslog (LOG_WARNING, "setenv (TCPLOCALPORT): %m");
	}
      else
	syslog (LOG_WARNING, "getnameinfo: %s", gai_strerror (ret));

      if (resolve_option)
	{
	  ret = getnameinfo ((struct sockaddr *) &sa_server, len,
			      ip, sizeof (ip), NULL, 0, 0);
	  if (ret != 0)
	    syslog (LOG_WARNING, "getnameinfo: %s", gai_strerror (ret));
	  else if (setenv ("TCPLOCALHOST", ip, 1) < 0)
	    syslog (LOG_WARNING, "setenv(TCPLOCALHOST): %m");
	}
    }

  ret = getnameinfo (sa_client, sa_len, ip, sizeof (ip), str, sizeof (str),
		      NI_NUMERICHOST | NI_NUMERICSERV);
  if (ret == 0)
    {
      if (setenv ("TCPREMOTEIP", ip, 1) < 0)
	syslog (LOG_WARNING, "setenv(TCPREMOTEIP): %m");
      else if (debug)
	fprintf (stderr, "Assigned TCPREMOTEIP = %s\n", ip);

      if (setenv ("TCPREMOTEPORT", str, 1) < 0)
	syslog (LOG_WARNING, "setenv(TCPREMOTEPORT): %m");

      if (resolve_option)
	{
	  ret = getnameinfo (sa_client, sa_len, ip, sizeof (ip), NULL, 0, 0);
	  if (ret != 0)
	    syslog (LOG_WARNING, "getnameinfo: %s", gai_strerror (ret));
	  else if (setenv ("TCPREMOTEHOST", ip, 1) < 0)
	    syslog (LOG_WARNING, "setenv(TCPREMOTEHOST): %m");
	  else if (debug)
	    fprintf (stderr, "Assigned TCPREMOTEHOST = %s\n", ip);
	}
    }
  else
    syslog (LOG_WARNING, "getnameinfo: %s", gai_strerror (ret));
}



int
main (int argc, char *argv[], char *envp[])
{
  int index;
  struct servtab *sep;
  int dofork;
  pid_t pid;

  set_program_name (argv[0]);

  Argv = argv;
  if (envp == 0 || *envp == 0)
    envp = argv;
  while (*envp)
    envp++;
  LastArg = envp[-1] + strlen (envp[-1]);

  /* Parse command line */
  iu_argp_init ("inetd", program_authors);
  argp_parse (&argp, argc, argv, 0, &index, NULL);

  if (resolve_option)
    env_option = true;

  if (index < argc)
    {
      int i;
      config_files = calloc (argc - index + 1, sizeof (*config_files));
      for (i = 0; index < argc; index++, i++)
	{
	  config_files[i] = strdup (argv[index]);
	}
    }
  else
    {
      config_files = calloc (3, sizeof (*config_files));
      config_files[0] = newstr (PATH_INETDCONF);
      config_files[1] = newstr (PATH_INETDDIR);
    }

  if (!debug)
    {
      if (daemon (0, 0) < 0)
	{
	  syslog (LOG_DAEMON | LOG_ERR,
		  "%s: Unable to enter daemon mode, %m", argv[0]);
	  exit (EXIT_FAILURE);
	};
    }

  openlog ("inetd", LOG_PID | LOG_NOWAIT, LOG_DAEMON);

  if (pidfile_option)
  {
    FILE *fp = fopen (pid_file, "w");
    if (fp != NULL)
      {
	if (debug)
	  fprintf(stderr, "Using pid-file at \"%s\".\n", pid_file);
	fprintf (fp, "%d\n", (int) getpid ());
	fclose (fp);
      }
    else
      syslog (LOG_CRIT, "can't open %s: %s\n", pid_file,
	      strerror (errno));
  }

  signal_set_handler (SIGALRM, retry);
  config (0);
  signal_set_handler (SIGHUP, config);
  signal_set_handler (SIGCHLD, reapchild);
  signal_set_handler (SIGPIPE, SIG_IGN);

  {
    /* space for daemons to overwrite environment for ps */
#define DUMMYSIZE	100
    char dummy[DUMMYSIZE];

    memset (dummy, 'x', DUMMYSIZE - 1);
    dummy[DUMMYSIZE - 1] = '\0';
    setenv ("inetd_dummy", dummy, 1);
  }

  for (;;)
    {
      int n, ctrl;
      fd_set readable;

      if (nsock == 0)
	{
	  SIGSTATUS stat;
	  sigstatus_empty (stat);

	  signal_block (NULL);
	  while (nsock == 0)
	    inetd_pause (stat);
	  signal_unblock (NULL);
	}
      readable = allsock;
      n = select (maxsock + 1, &readable, NULL, NULL, NULL);
      if (n <= 0)
	{
	  if (n < 0 && errno != EINTR)
	    syslog (LOG_WARNING, "select: %m");
	  sleep (1);
	  continue;
	}
      for (sep = servtab; n && sep; sep = sep->se_next)
	if (sep->se_fd != -1 && FD_ISSET (sep->se_fd, &readable))
	  {
	    n--;
	    if (debug)
	      fprintf (stderr, "someone wants %s\n", sep->se_service);
	    if (!sep->se_wait && sep->se_socktype == SOCK_STREAM)
	      {
#ifdef IPV6
		struct sockaddr_storage sa_client;
#else
		struct sockaddr_in sa_client;
#endif
		socklen_t len = sizeof (sa_client);

		ctrl = accept (sep->se_fd, (struct sockaddr *) &sa_client,
			       &len);
		if (debug)
		  fprintf (stderr, "accept, ctrl %d\n", ctrl);
		if (ctrl < 0)
		  {
		    if (errno != EINTR)
		      syslog (LOG_WARNING, "accept (for %s): %m",
			      sep->se_service);
		    continue;
		  }
		if (env_option)
		  prepenv (ctrl, (struct sockaddr *) &sa_client, len);
	      }
	    else
	      ctrl = sep->se_fd;

	    signal_block (NULL);
	    pid = 0;
	    dofork = (sep->se_bi == 0 || sep->se_bi->bi_fork);
	    if (dofork)
	      {
		if (sep->se_count++ == 0)
		  gettimeofday (&sep->se_time, NULL);
		else if ((sep->se_max && sep->se_count > sep->se_max)
			 || sep->se_count >= toomany)
		  {
		    struct timeval now;

		    gettimeofday (&now, NULL);
		    if (now.tv_sec - sep->se_time.tv_sec > CNT_INTVL)
		      {
			sep->se_time = now;
			sep->se_count = 1;
		      }
		    else
		      {
			syslog (LOG_ERR,
				"%s/%s server failing (looping), service terminated",
				sep->se_service, sep->se_proto);
			close_sep (sep);
			if (! sep->se_wait && sep->se_socktype == SOCK_STREAM)
			  close (ctrl);
			signal_unblock (NULL);
			if (!timingout)
			  {
			    timingout = 1;
			    alarm (RETRYTIME);
			  }
			continue;
		      }
		  }
		pid = fork ();
	      }
	    if (pid < 0)
	      {
		syslog (LOG_ERR, "fork: %m");
		if (!sep->se_wait && sep->se_socktype == SOCK_STREAM)
		  close (ctrl);
		signal_unblock (NULL);
		sleep (1);
		continue;
	      }
	    if (pid && sep->se_wait)
	      {
		sep->se_wait = pid;
		if (sep->se_fd >= 0)
		  {
		    FD_CLR (sep->se_fd, &allsock);
		    nsock--;
		  }
	      }
	    signal_unblock (NULL);
	    if (pid == 0)
	      {
		if (debug && dofork)
		  setsid ();
		if (dofork)
		  {
		    int sock;
		    if (debug)
		      fprintf (stderr, "+ Closing from %d\n", maxsock);
		    for (sock = maxsock; sock > 2; sock--)
		      if (sock != ctrl)
			close (sock);
		  }
		run_service (ctrl, sep);
	      }
	    if (!sep->se_wait && sep->se_socktype == SOCK_STREAM)
	      close (ctrl);
	  }
    }
}
